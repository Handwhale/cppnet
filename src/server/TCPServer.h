#pragma once

#include "lib/TCPConnection.h"
#include "server/ITCPServerHandler.h"
#include "server/TCPListener.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace cppnet
{
class TCPServer
{
  public:
    explicit TCPServer(std::unique_ptr<ITCPServerHandler>&& handler);
    ~TCPServer();

    void Start(std::string address, uint16_t port);
    void Join();
    void Stop();

    void BroadcastAll(std::string data);
    void BroadcastTo(std::string data, TCPConnection::ID recipient);
    void BroadcastExcept(std::string data, std::vector<TCPConnection::ID> excludeIds);

  private:
    void AcceptLoop();
    void RemoveLoop();

    void MarkConnectionForRemoval(TCPConnection::ID id);

  private:
    std::atomic_bool _running = false;

    std::mutex _connectionsMtx;
    std::unordered_map<TCPConnection::ID, std::unique_ptr<TCPConnection>> _connections;
    TCPConnection::ID _nextConnectionId = 1;
    TCPListener _serverSocket;

    std::thread _connectionRemoveThread;
    std::condition_variable _connectionRemoveCV;
    std::vector<TCPConnection::ID> _connectionsToRemove;
    std::mutex _connectionsToRemoveMtx;

    std::thread _acceptThread;
    std::unique_ptr<ITCPServerHandler> _serverHandler;
};
} // namespace cppnet