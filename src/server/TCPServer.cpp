#include "TCPServer.h"

#include "lib/Socket.h"

#include <algorithm>
#include <cerrno>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cppnet
{
TCPServer::TCPServer(std::unique_ptr<ITCPServerHandler>&& handler) : _serverHandler(std::move(handler)) {}

TCPServer::~TCPServer()
{
    Stop();

    if (_epollThread.joinable())
    {
        _epollThread.join();
    }
}

void TCPServer::Start(std::string address, uint16_t port)
{
    std::cout << "[System] Server start\n";
    _serverSocket.Open(std::move(address), port);

    std::cout << "[System] Server is listening on port " << _serverSocket.GetPort() << std::endl;

    _running.store(true);
    auto serverSocketFD = _serverSocket.GetUFD().Get();
    auto [it, inserted] =
        _epollTargetsByFD.emplace(serverSocketFD, std::make_unique<EpollTarget>(EpollTargetType::Server, serverSocketFD, kNullId));

    if (_epoll.Add(serverSocketFD, Epoll::CreateEvents(true), it->second.get()) == -1)
    {
        std::cerr << "Epoll error";
        Stop();
    }

    _epollThread = std::thread(&TCPServer::EventLoop, this);
}

void TCPServer::Join()
{
    if (_epollThread.joinable())
    {
        _epollThread.join();
    }
}

void TCPServer::Stop()
{
    bool expected = true;
    if (_running.compare_exchange_strong(expected, false))
    {
        _serverSocket.Reset();
    }
}

void TCPServer::AcceptNewClient()
{
    while (_running.load())
    {
        auto acceptResult = _serverSocket.Accept();

        if (!acceptResult.socket.IsValid())
        {
            if (!_running.load())
            {
                return;
            }

            if (acceptResult.error == EINTR)
            {
                continue;
            }

            if (acceptResult.error == EAGAIN || acceptResult.error == EWOULDBLOCK)
            {
                return;
            }

            if (acceptResult.error == EBADF || acceptResult.error == EINVAL)
            {
                if (!_running.load())
                {
                    return;
                }
            }

            std::cerr << "[System] Error: accept failed";
            return;
        }

        auto clientFd = acceptResult.socket.GetUFD().Get();
        auto newId = _nextIOHandlerId++;

        auto messageHandler = [this](IOHandler::ID id, std::string data) { _serverHandler->OnMessage(*this, id, std::move(data)); };

        auto errorHandler = [this](IOHandler::ID id, std::string error)
        { _serverHandler->OnError(*this, id, "Connection " + std::to_string(id) + ": " + error); };

        _clientHandlers.emplace(newId,
                                IOHandler(newId, std::move(acceptResult.socket), std::move(messageHandler), std::move(errorHandler)));

        auto [epollIt, inserted] =
            _epollTargetsByFD.emplace(clientFd, std::make_unique<EpollTarget>(EpollTargetType::Client, clientFd, newId));

        if (_epoll.Add(clientFd, Epoll::CreateEvents(true), epollIt->second.get()) == -1)
        {
            CloseClient(*epollIt->second.get(), false);
            continue;
        }
        _serverHandler->OnConnect(*this, newId);
    }
}

void TCPServer::EventLoop()
{
    const int K_MAX_EPOLL_EVENTS = 64;
    epoll_event epollEvents[K_MAX_EPOLL_EVENTS];
    std::vector<EpollTarget> handlersToRemove;

    while (_running.load())
    {

        int eventsNum = _epoll.Wait(epollEvents, K_MAX_EPOLL_EVENTS, -1);
        if (eventsNum == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            // throw std::system_error(errno, std::generic_category());
        }
        for (int i = 0; i < eventsNum; i++)
        {
            auto events = epollEvents[i].events;
            EpollTarget* target = static_cast<EpollTarget*>(epollEvents[i].data.ptr);

            if (target->type == EpollTargetType::Server)
            {
                auto res = HandleServerEvents(events);
                if (!res)
                {
                    return;
                }
            }
            else
            {
                auto res = HandleClientEvents(*target, events);
                if (!res)
                {
                    handlersToRemove.push_back(*target);
                }
            }
        }

        for (auto& target : handlersToRemove)
        {
            CloseClient(target);
        }
        handlersToRemove.clear();
    }
}

void TCPServer::CloseClient(EpollTarget target, bool notify)
{
    _epoll.Remove(target.fd);
    _epollTargetsByFD.erase(target.fd);
    const auto erased =_clientHandlers.erase(target.id);
    if (erased > 0 && notify)
    {
        _serverHandler->OnDisconnect(*this, target.id);
    }
}

bool TCPServer::HandleServerEvents(uint32_t events)
{
    if (events & (EPOLLRDHUP | EPOLLHUP))
    {
        std::cerr << "[System] Server socket error/hangup\n";
        Stop();
        return false;
    }

    if (events & EPOLLIN)
    {
        AcceptNewClient();
    }

    return true;
}

bool TCPServer::HandleClientEvents(EpollTarget& target, uint32_t events)
{
    auto findIt = _clientHandlers.find(target.id);
    if (findIt == _clientHandlers.end())
    {
        std::cerr << "[System] Client socket not found\n";
        return false;
    }
    auto& ioHandler = findIt->second;
    IOHandler::IOUpdate update;

    if (events & EPOLLERR)
    {
        std::cerr << "[System] Client socket error\n";
        return false;
    }

    if (events & EPOLLIN)
    {
        update = ioHandler.Read();
    }

    if (events & EPOLLOUT)
    {
        update = ioHandler.Write();
        if (!update.wantWrite)
        {
            _epoll.Modify(ioHandler.Fd(), Epoll::CreateEvents(true), _epollTargetsByFD[ioHandler.Fd()].get());
        }
    }

    if (events & (EPOLLRDHUP | EPOLLHUP))
    {
        // peer closed/hangup
        update = ioHandler.Close();
    }

    if (update.shouldClose)
    {
        return false;
    }
    return true;
}

void TCPServer::BroadcastAll(std::string_view data)
{
    BroadcastExcept(data, {});
}

void TCPServer::BroadcastTo(std::string_view data, IOHandler::ID recipient)
{
    auto it = _clientHandlers.find(recipient);
    if (it != _clientHandlers.end())
    {
        BroadcastTo(std::move(data), it->second);
    }
}

void TCPServer::BroadcastExcept(std::string_view data, std::vector<IOHandler::ID> excludeIds)
{

    for (auto& [id, ioHandler] : _clientHandlers)
    {
        auto findIt = std::find(excludeIds.begin(), excludeIds.end(), ioHandler.Id());
        if (findIt != excludeIds.end())
        {
            continue;
        }
        BroadcastTo(data, ioHandler);
    }
}

void TCPServer::BroadcastTo(std::string_view data, IOHandler& recipient)
{
    auto result = recipient.QueueMessage(data);
    if (result.wantWrite)
    {
        if (_epoll.Modify(recipient.Fd(), Epoll::CreateEvents(true, true), _epollTargetsByFD[recipient.Fd()].get()) == -1)
        {
            std::cerr << "Epoll error";
            Stop();
        }
    }

    if (result.shouldClose)
    {
        CloseClient(*_epollTargetsByFD[recipient.Fd()].get());
    }
}
} // namespace cppnet