#include "TCPClient.h"

#include <utility>

#include "client/TCPConnector.h"

namespace cppnet
{
TCPClient::~TCPClient()
{
    Disconnect();
}

void TCPClient::OnMessage(MessageHandler handler)
{
    _messageHandler = std::move(handler);
}

void TCPClient::OnDisconnect(DisconnectHandler handler)
{
    _disconnectHandler = std::move(handler);
}

void TCPClient::OnError(ErrorHandler handler)
{
    _errorHandler = std::move(handler);
}

void TCPClient::Connect(std::string host, uint16_t port)
{
    Socket socket = TCPConnector::Connect(std::move(host), port);

    auto messageHandler = [this](TCPConnection::ID, std::string message)
    {
        if (_messageHandler)
        {
            _messageHandler(std::move(message));
        }
    };

    auto closeHandler = [this](TCPConnection::ID)
    {
        _connected.store(false);

        if (_disconnectHandler)
        {
            _disconnectHandler();
        }
    };

    auto errorHandler = [this](TCPConnection::ID, std::string error)
    {
        if (_errorHandler)
        {
            _errorHandler(std::move(error));
        }
    };

    _connection =
        std::make_unique<TCPConnection>(1, std::move(socket), std::move(messageHandler), std::move(closeHandler), std::move(errorHandler));

    _connected.store(true);
}

void TCPClient::Disconnect()
{
    _connected.store(false);

    if (_connection)
    {
        _connection->Close();
        _connection.reset();
    }
}

bool TCPClient::Send(std::string message)
{
    if (!_connection || !_connected.load())
    {
        return false;
    }

    return _connection->Write(std::make_shared<std::string>(std::move(message)));
}

bool TCPClient::IsConnected() const noexcept
{
    return _connected.load();
}
} // namespace cppnet