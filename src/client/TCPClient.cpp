#include "TCPClient.h"

#include "client/TCPConnector.h"
#include "lib/Epoll.h"
#include "lib/IOHandler.h"
#include "lib/Logger.h"

#include <utility>

namespace cppnet
{
TCPClient::TCPClient(std::unique_ptr<ITCPHandler<TCPClient>>&& handler) : _clientHandler(std::move(handler)) {}

TCPClient::~TCPClient()
{
    Disconnect();
    Join();
}

void TCPClient::Join()
{
    if (_epollThread.joinable())
    {
        _epollThread.join();
    }
}

void TCPClient::Connect(std::string host, uint16_t port)
{
    if (_connected.load())
    {
        LogError("Client is already running");
        return;
    }

    if (_epollThread.joinable())
    {
        LogError("Client is joinable");
        return;
    }

    auto messageHandler = [this](IOHandler::ID id, std::string message) { _clientHandler->OnMessage(*this, id, std::move(message)); };

    auto errorHandler = [this](IOHandler::ID id, std::string error) { _clientHandler->OnError(*this, id, std::move(error)); };

    Socket socket = TCPConnector::Connect(std::move(host), port);

    _ioHandler = std::make_unique<IOHandler>(0, std::move(socket), std::move(messageHandler), std::move(errorHandler));

    _epoll = std::make_unique<Epoll>();
    if (!_epoll->IsValid())
    {
        LogError("Epoll error", errno);
        Cleanup();
        return;
    }

    if (_epoll->Add(_ioHandler->Fd(), Epoll::CreateEvents(true)) == -1)
    {
        LogError("Epoll error", errno);
        Cleanup();
        return;
    }

    if (_epoll->Add(_eventFD.Fd(), Epoll::CreateEvents(true)) == -1)
    {
        LogError("Epoll error", errno);
        Cleanup();
        return;
    }

    _connected.store(true);
    _clientHandler->OnConnect(*this, _ioHandler->Id());
    _epollThread = std::thread(&TCPClient::EpollLoop, this);
}

void TCPClient::EpollLoop()
{
    const int K_MAX_EPOLL_EVENTS = 64;
    epoll_event epollEvents[K_MAX_EPOLL_EVENTS];

    while (_connected.load())
    {

        int eventsNum = _epoll->Wait(epollEvents, K_MAX_EPOLL_EVENTS, -1);
        if (eventsNum == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            LogError("Epoll error", errno);
            Disconnect();
        }

        for (int i = 0; i < eventsNum; i++)
        {
            auto events = epollEvents[i].events;

            auto fd = epollEvents[i].data.fd;

            if (fd == _eventFD.Fd())
            {
                HandleEvents();
            }
            else if (fd == _ioHandler->Fd())
            {
                HandleIOEvents(events);
            }
        }
    }
    Cleanup();
}

void TCPClient::HandleEvents()
{
    if (!_eventFD.Pull())
    {
        return;
    }

    std::queue<std::string> messages;
    {
        std::lock_guard lk(_messageMtx);
        _messageQueue.swap(messages);
    }

    IOHandler::IOUpdate result;
    while (!messages.empty())
    {
        result = _ioHandler->QueueMessage(std::move(messages.front()));
        messages.pop();
    }

    if (result.wantWrite)
    {
        _epoll->Modify(_ioHandler->Fd(), Epoll::CreateEvents(true, true));
    }

    if (result.shouldClose){
        Disconnect();
    }
}

void TCPClient::HandleIOEvents(uint32_t events)
{
    IOHandler::IOUpdate update;

    if (events & EPOLLERR)
    {
        LogError("Client socket error", errno);
        Disconnect();
        return;
    }

    if (events & EPOLLIN)
    {
        update = _ioHandler->Read();
    }

    if (events & EPOLLOUT)
    {
        update = _ioHandler->Write();
        if (!update.wantWrite)
        {
            _epoll->Modify(_ioHandler->Fd(), Epoll::CreateEvents(true));
        }
    }

    if (events & (EPOLLRDHUP | EPOLLHUP))
    {
        update = _ioHandler->Close();
    }

    if (update.shouldClose)
    {
        Disconnect();
    }
}

void TCPClient::Disconnect()
{
    bool expected = true;
    if (_connected.compare_exchange_strong(expected, false))
    {
        _eventFD.Push();
    }
}

void TCPClient::Cleanup()
{
    if (_ioHandler)
    {
        _clientHandler->OnDisconnect(*this, _ioHandler->Id());
    }
    _epoll.reset();
    _ioHandler.reset();
    _eventFD.Pull();

    std::queue<std::string> emptyQueue;
    {
        std::lock_guard lk(_messageMtx);
        _messageQueue.swap(emptyQueue);
    }
}

void TCPClient::Send(std::string message)
{
    {
        std::lock_guard lk(_messageMtx);
        if (_connected.load())
        {
            _messageQueue.push(std::move(message));
        }
    }
    _eventFD.Push();
}
} // namespace cppnet
