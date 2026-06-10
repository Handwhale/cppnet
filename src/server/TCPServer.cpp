#include "TCPServer.h"

#include "lib/Epoll.h"
#include "lib/Socket.h"
#include "lib/Logger.h"

#include <algorithm>
#include <cerrno>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cppnet
{
TCPServer::TCPServer(std::unique_ptr<ITCPHandler<TCPServer>>&& handler) : _serverHandler(std::move(handler)) {}

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
    if (_running.load())
    {

        LogError("Server is already running");
        return;
    }

    if (_epollThread.joinable())
    {
        LogError("Server is joinable");
        return;
    }

    LogInfo("Server start");
    _serverSocket.Open(std::move(address), port);

    LogInfo("Server is listening on port " + std::to_string(_serverSocket.GetPort()));

    _epoll = std::make_unique<Epoll>();
    if (!_epoll->IsValid())
    {
        LogError("Epoll error", errno);
        Cleanup();
        return;
    }

    {
        auto serverSocketFD = _serverSocket.GetUFD().Get();
        auto [it, inserted] =
            _epollTargetsByFD.emplace(serverSocketFD, std::make_unique<EpollTarget>(EpollTargetType::Server, serverSocketFD, kNullId));

        if (_epoll->Add(serverSocketFD, Epoll::CreateEvents(true), it->second.get()) == -1)
        {
            LogError("Epoll error", errno);
            Cleanup();
            return;
        }
    }
    {
        auto [it, inserted] =
            _epollTargetsByFD.emplace(_commandsFD.Fd(), std::make_unique<EpollTarget>(EpollTargetType::Command, _commandsFD.Fd(), kNullId));

        if (_epoll->Add(_commandsFD.Fd(), Epoll::CreateEvents(true), it->second.get()) == -1)
        {
            LogError("Epoll error", errno);
            Cleanup();
            return;
        }
    }
    _running.store(true);
    _epollThread = std::thread(&TCPServer::EpollLoop, this);
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
        _commandsFD.Push();
    }
}

void TCPServer::Cleanup()
{
    for (auto& [id, handler] : _clientHandlers)
    {
        handler.Close();
        _serverHandler->OnDisconnect(*this, id);
    }

    decltype(_commands) tmpCommands;
    {
        std::lock_guard lk(_commandsMtx);
        _commands.swap(tmpCommands);
    }
    decltype(_targetCloseQueue) tmpCloseQueue;
    _targetCloseQueue.swap(tmpCloseQueue);
    _clientHandlers.clear();
    _epollTargetsByFD.clear();
    _serverSocket.Reset();
    _epoll.reset();
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

            LogError("Error: accept failed", acceptResult.error);
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

        if (_epoll->Add(clientFd, Epoll::CreateEvents(true), epollIt->second.get()) == -1)
        {
            CloseClient(*epollIt->second.get(), false);
            continue;
        }
        _serverHandler->OnConnect(*this, newId);
    }
}

void TCPServer::EpollLoop()
{
    const int K_MAX_EPOLL_EVENTS = 64;
    epoll_event epollEvents[K_MAX_EPOLL_EVENTS];

    while (_running.load())
    {

        int eventsNum = _epoll->Wait(epollEvents, K_MAX_EPOLL_EVENTS, -1);
        if (eventsNum == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            LogError("Epoll error", errno);

            Stop();
        }

        for (int i = 0; i < eventsNum; i++)
        {
            auto events = epollEvents[i].events;
            EpollTarget* target = static_cast<EpollTarget*>(epollEvents[i].data.ptr);

            switch (target->type)
            {
            case EpollTargetType::Server:
            {
                HandleServerEvents(events);
                continue;
            }
            case EpollTargetType::Client:
            {
                HandleClientEvents(*target, events);
                continue;
            }
            case EpollTargetType::Command:
            {
                HandleCommands();
                continue;
            }
            default:
            {
                LogError("Unknown epoll target type");
                Stop();
            }
            }
        }
        CloseQueuedClients();
    }
    Cleanup();
}

void TCPServer::CloseClient(EpollTarget target, bool notify)
{
    auto findIt = _clientHandlers.find(target.id);
    if (findIt == _clientHandlers.end())
    {
        return;
    }

    _epoll->Remove(target.fd);
    _epollTargetsByFD.erase(target.fd);
    _clientHandlers.erase(findIt);
    if (notify)
    {
        _serverHandler->OnDisconnect(*this, target.id);
    }
}

void TCPServer::QueueForClose(int clientFD)
{
    auto res = FindEpollTarget(clientFD);
    if (res != nullptr)
    {
        _targetCloseQueue.push_back(*(res));
    }
}

void TCPServer::QueueForClose(EpollTarget target)
{
    _targetCloseQueue.push_back(target);
}

void TCPServer::CloseQueuedClients()
{
    for (auto& target : _targetCloseQueue)
    {
        CloseClient(std::move(target));
    }
    _targetCloseQueue.clear();
}

void TCPServer::HandleServerEvents(uint32_t events)
{
    if (events & (EPOLLERR | EPOLLHUP))
    {
        LogError("Server socket error/hangup", errno);

        Stop();
        return;
    }

    if (events & EPOLLIN)
    {
        AcceptNewClient();
    }
}

void TCPServer::HandleClientEvents(EpollTarget& target, uint32_t events)
{
    auto findIt = _clientHandlers.find(target.id);
    if (findIt == _clientHandlers.end())
    {
        LogError("Client socket not found");

        QueueForClose(target);
        return;
    }
    auto& ioHandler = findIt->second;
    IOHandler::IOUpdate update;

    if (events & EPOLLERR)
    {
        LogError("Client socket error", errno);
        QueueForClose(target);
        return;
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
            _epoll->Modify(ioHandler.Fd(), Epoll::CreateEvents(true), &target);
        }
    }

    if (events & (EPOLLRDHUP | EPOLLHUP))
    {
        // peer closed/hangup
        update = ioHandler.Close();
    }

    if (update.shouldClose)
    {
        return QueueForClose(target);
    }
}

void TCPServer::HandleCommands()
{
    if (!_commandsFD.Pull())
    {
        return;
    }

    std::queue<Command> commands;
    {
        std::lock_guard lk(_commandsMtx);
        commands.swap(_commands);
    }

    while (!commands.empty())
    {
        HandleCommand(std::move(commands.front()));
        commands.pop();
    }
}

void TCPServer::HandleCommand(Command&& command)
{
    switch (command.type)
    {
    case Command::Type::BroadcastExcept:
    {
        BroadcastExceptImpl(command.data, command.excludeIds);
        return;
    }
    case Command::Type::BroadcastTo:
    {
        auto findIt = _clientHandlers.find(command.recipient);
        if (findIt != _clientHandlers.end())
        {
            BroadcastToImpl(command.data, findIt->second);
        }
        return;
    }
    default:
    {
        LogError("Unknown command");
        return;
    }
    }
}

void TCPServer::BroadcastAll(std::string data)
{
    BroadcastExcept(data, {});
}

void TCPServer::BroadcastTo(std::string data, IOHandler::ID recipient)
{
    Command command;
    command.data = std::move(data);
    command.recipient = recipient;
    command.type = Command::Type::BroadcastTo;
    {
        std::lock_guard lk(_commandsMtx);
        if (_running.load())
        {
            _commands.push(std::move(command));
        }
    }
    _commandsFD.Push();
}

void TCPServer::BroadcastExcept(std::string data, std::vector<IOHandler::ID> excludeIds)
{
    if (!_running.load())
    {
        return;
    }
    Command command;
    command.data = std::move(data);
    command.excludeIds = excludeIds;
    command.type = Command::Type::BroadcastExcept;
    {
        std::lock_guard lk(_commandsMtx);
        if (_running.load())
        {
            _commands.push(std::move(command));
        }
    }
    _commandsFD.Push();
}

void TCPServer::BroadcastExceptImpl(std::string_view data, std::vector<IOHandler::ID> excludeIds)
{
    for (auto& [id, ioHandler] : _clientHandlers)
    {
        auto findIt = std::find(excludeIds.begin(), excludeIds.end(), ioHandler.Id());
        if (findIt != excludeIds.end())
        {
            continue;
        }
        BroadcastToImpl(data, ioHandler);
    }
}

void TCPServer::BroadcastToImpl(std::string_view data, IOHandler& recipient)
{
    auto result = recipient.QueueMessage(data);

    auto target = FindEpollTarget(recipient.Fd());
    if (target == nullptr)
    {
        return;
    }

    if (result.wantWrite)
    {
        if (_epoll->Modify(recipient.Fd(), Epoll::CreateEvents(true, true), target) == -1)
        {
            LogError("Epoll error", errno);
            Stop();
        }
    }

    if (result.shouldClose)
    {
        QueueForClose(recipient.Fd());
    }
}

TCPServer::EpollTarget* TCPServer::FindEpollTarget(int fd)
{
    auto findIt = _epollTargetsByFD.find(fd);
    if (findIt != _epollTargetsByFD.end())
    {
        return findIt->second.get();
    }
    return nullptr;
}
} // namespace cppnet
