#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "Socket.h"

namespace cppnet
{
using MessagePtr = std::shared_ptr<std::string>;

class TCPConnection final
{
  public:
    using ID = size_t;
    using MessageReceivedHandler = std::function<void(TCPConnection::ID, std::string)>;
    using ErrorHandler = std::function<void(TCPConnection::ID, std::string)>;
    using CloseHandler = std::function<void(TCPConnection::ID)>;

    TCPConnection(const TCPConnection&) = delete;
    TCPConnection& operator=(const TCPConnection&) = delete;

    TCPConnection(TCPConnection&&) = delete;
    TCPConnection& operator=(TCPConnection&&) = delete;

    TCPConnection(ID id, Socket socket, MessageReceivedHandler msgHandler, CloseHandler closeHandler, ErrorHandler errorHandler);
    ~TCPConnection();

    ID Id() const
    {
        return _id;
    };

    bool Write(MessagePtr msg);

    void Close();
    bool IsClosed() const noexcept;

  private:
    void ReadLoop();
    void WriteLoop();

    void CloseWithNotify();
    void NotifyClosed();

  private:
    Socket _socket;
    ID _id = 0;
    std::atomic_bool _closeConnection;
    std::atomic_bool _closeNotified;

    std::thread _writeThread, _readThread;

    MessageReceivedHandler _msgHandler;
    ErrorHandler _errorHandler;
    CloseHandler _closeHandler;

    std::queue<MessagePtr> _outgoingQueue;
    std::mutex _queueMtx;
    std::condition_variable _outgoingCV;
    const size_t MaxQueueSize = 10;
};
} // namespace cppnet