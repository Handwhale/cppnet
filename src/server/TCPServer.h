#pragma once

#include "lib/EventFD.h"
#include "lib/IOHandler.h"

#include "server/TCPListener.h"

#include <atomic>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

namespace cppnet
{
class Epoll;
class ITCPServerHandler;

class TCPServer
{
  public:
    struct Command
    {
        enum class Type
        {
            BroadcastTo,
            BroadcastExcept
        };

        Type type;
        IOHandler::ID recipient = 0;
        std::string data;
        std::vector<IOHandler::ID> excludeIds;
    };

    enum class EpollTargetType
    {
        Server,
        Client,
        Command
    };

    struct EpollTarget
    {
        EpollTarget(EpollTargetType type_, int fd_, IOHandler::ID id_) : type(type_), fd(fd_), id(id_) {}
        EpollTargetType type;
        int fd;
        IOHandler::ID id;
    };

  public:
    explicit TCPServer(std::unique_ptr<ITCPServerHandler>&& handler);
    ~TCPServer();

    void Start(std::string address, uint16_t port);
    void Join();
    void Stop();

    void BroadcastAll(std::string data);
    void BroadcastTo(std::string data, IOHandler::ID recipient);
    void BroadcastExcept(std::string data, std::vector<IOHandler::ID> excludeIds);

  private:
    void EpollLoop();
    void BroadcastExceptImpl(std::string_view data, std::vector<IOHandler::ID> excludeIds);
    void BroadcastToImpl(std::string_view data, IOHandler& recipient);

    void HandleServerEvents(uint32_t events);
    void HandleClientEvents(EpollTarget&, uint32_t events);
    void HandleCommands();
    void HandleCommand(Command&& command);
    void AcceptNewClient();
    void Cleanup();
    void CloseClient(EpollTarget target, bool notify = true);

    void QueueForClose(int clientFd);
    void QueueForClose(EpollTarget target);
    void CloseQueuedClients();

    EpollTarget* FindEpollTarget(int fd);

    void LogError(std::string_view message, int error = 0);

  private:
    std::atomic_bool _running = false;

    IOHandler::ID _nextIOHandlerId = 1;
    TCPListener _serverSocket;
    std::unordered_map<IOHandler::ID, IOHandler> _clientHandlers;
    std::unordered_map<int, std::unique_ptr<EpollTarget>> _epollTargetsByFD;
    std::vector<EpollTarget> _targetCloseQueue;

    std::thread _epollThread;
    std::unique_ptr<ITCPServerHandler> _serverHandler;

    EventFD _commandsFD;
    std::mutex _commandsMtx;
    std::queue<Command> _commands;

    std::unique_ptr<Epoll> _epoll;

    static constexpr IOHandler::ID kNullId = 0;
};
} // namespace cppnet