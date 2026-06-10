#pragma once

#include "lib/EventFD.h"
#include "lib/ITCPHandler.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace cppnet
{
class IOHandler;
class Epoll;

class TCPClient
{
  public:
    TCPClient(std::unique_ptr<ITCPHandler<TCPClient>>&& handler);
    ~TCPClient();

    void Connect(std::string host, uint16_t port);
    void Disconnect();
    void Join();

    void Send(std::string message);

  private:
    void EpollLoop();
    void HandleIOEvents(uint32_t events);
    void HandleEvents();
    void Cleanup();

  private:
    std::unique_ptr<ITCPHandler<TCPClient>> _clientHandler;

    std::unique_ptr<IOHandler> _ioHandler;
    EventFD _eventFD;
    std::unique_ptr<Epoll> _epoll;
    std::thread _epollThread;

    std::queue<std::string> _messageQueue;
    std::mutex _messageMtx;

    std::atomic_bool _connected{false};
};
} // namespace cppnet
