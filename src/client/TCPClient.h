#pragma once

#include "lib/TCPConnection.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace cppnet
{
class TCPClient
{
  public:
    using MessageHandler = std::function<void(std::string)>;
    using DisconnectHandler = std::function<void()>;
    using ErrorHandler = std::function<void(std::string)>;

    ~TCPClient();

    void OnMessage(MessageHandler handler);
    void OnDisconnect(DisconnectHandler handler);
    void OnError(ErrorHandler handler);

    void Connect(std::string host, uint16_t port);
    void Disconnect();

    bool Send(std::string message);

    bool IsConnected() const noexcept;

  private:
    std::unique_ptr<TCPConnection> _connection;

    MessageHandler _messageHandler;
    DisconnectHandler _disconnectHandler;
    ErrorHandler _errorHandler;

    std::atomic_bool _connected{false};
};
} // namespace cppnet