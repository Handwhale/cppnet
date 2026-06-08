#pragma once

#include "lib/Epoll.h"
#include "lib/IOHandler.h"
#include "server/ITCPServerHandler.h"
#include "server/TCPListener.h"

#include <string>
#include <thread>
#include <unordered_map>
#include <atomic>

namespace cppnet
{
class TCPServer
{
  public:
    enum class EpollTargetType
    {
        Server,
        Client
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

    void BroadcastAll(std::string_view data);
    void BroadcastTo(std::string_view data, IOHandler::ID recipient);
    void BroadcastExcept(std::string_view data, std::vector<IOHandler::ID> excludeIds);

  private:
    void EventLoop();
    void BroadcastTo(std::string_view data, IOHandler& recipient);

    bool HandleServerEvents(uint32_t events);
    bool HandleClientEvents(EpollTarget&, uint32_t events);
    void AcceptNewClient();
    void CloseClient(EpollTarget target, bool notify = true);

  private:
    std::atomic_bool _running = false;

    IOHandler::ID _nextIOHandlerId = 1;
    TCPListener _serverSocket;
    std::unordered_map<IOHandler::ID, IOHandler> _clientHandlers;
    std::unordered_map<int, std::unique_ptr<EpollTarget>> _epollTargetsByFD;

    std::thread _epollThread;
    std::unique_ptr<ITCPServerHandler> _serverHandler;

    Epoll _epoll;

    static constexpr IOHandler::ID kNullId = 0;
};
} // namespace cppnet