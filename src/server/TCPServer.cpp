#include "TCPServer.h"

#include "lib/Socket.h"
#include "lib/TCPConnection.h"

#include <algorithm>
#include <cerrno>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace cppnet
{
TCPServer::TCPServer(std::unique_ptr<ITCPServerHandler>&& handler) : _serverHandler(std::move(handler)) {}

TCPServer::~TCPServer()
{
    Stop();

    if (_acceptThread.joinable())
    {
        _acceptThread.join();
    }

    _connectionRemoveCV.notify_one();

    if (_connectionRemoveThread.joinable())
    {
        _connectionRemoveThread.join();
    }
}

void TCPServer::Start(std::string address, uint16_t port)
{
    std::cout << "[System] Server start\n";
    _serverSocket.Open(std::move(address), port);

    std::cout << "[System] Server is listening on port " << _serverSocket.GetPort() << std::endl;

    _running.store(true);

    _connectionRemoveThread = std::thread(&TCPServer::RemoveLoop, this);
    _acceptThread = std::thread(&TCPServer::AcceptLoop, this);
}

void TCPServer::Join()
{
    if (_acceptThread.joinable())
    {
        _acceptThread.join();
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

void TCPServer::AcceptLoop()
{
    while (_running.load())
    {
        auto acceptResult = _serverSocket.Accept();

        if (!acceptResult.socket.IsValid())
        {
            if (!_running.load())
            {
                break;
            }

            if (acceptResult.error == EINTR)
            {
                continue;
            }

            if (acceptResult.error == EBADF || acceptResult.error == EINVAL)
            {
                if (!_running.load())
                {
                    break;
                }
            }

            std::cerr << "[System] Error: accept failed";
            _running.store(true);
            break;
        }

        auto newId = _nextConnectionId++;
        std::cout << "[System] Client " << newId << " connected" << '\n';

        auto messageHandler = [this](TCPConnection::ID id, std::string data) { _serverHandler->OnMessage(*this, id, std::move(data)); };

        auto closeHandler = [this](TCPConnection::ID id) { MarkConnectionForRemoval(id); };

        auto errorHandler = [this](TCPConnection::ID id, std::string error)
        { _serverHandler->OnError(*this, id, "Connection " + std::to_string(id) + ": " + error); };

        auto connection = std::make_unique<TCPConnection>(newId, std::move(acceptResult.socket), std::move(messageHandler),
                                                          std::move(closeHandler), std::move(errorHandler));

        {
            std::lock_guard lk(_connectionsMtx);
            _connections.insert(std::make_pair(newId, std::move(connection)));
        }

        _serverHandler->OnConnect(*this, newId);
    }
}

void TCPServer::RemoveLoop()
{
    while (_running.load())
    {
        std::vector<TCPConnection::ID> idsToRemove;
        {
            std::unique_lock lk(_connectionsToRemoveMtx);
            _connectionRemoveCV.wait(lk, [this]() { return _connectionsToRemove.size() > 0 || !_running.load(); });
            if (_connectionsToRemove.empty())
            {
                break;
            }

            idsToRemove.swap(_connectionsToRemove);
        }

        std::vector<std::unique_ptr<TCPConnection>> removed;
        {
            std::lock_guard lk2(_connectionsMtx);
            for (const auto connectionID : _connectionsToRemove)
            {
                auto findIt = _connections.find(connectionID);
                if (findIt != _connections.end())
                {
                    removed.push_back(std::move(findIt->second));
                    _connections.erase(findIt);
                }
            }
        }

        removed.clear();

        for (const auto connectionID : idsToRemove)
        {
            _serverHandler->OnDisconnect(*this, connectionID);
        }
        idsToRemove.clear();
    }
}

void TCPServer::MarkConnectionForRemoval(TCPConnection::ID id)
{
    {
        std::lock_guard lk(_connectionsToRemoveMtx);
        _connectionsToRemove.push_back(id);
    }

    _connectionRemoveCV.notify_one();
}

void TCPServer::BroadcastAll(std::string data)
{
    BroadcastExcept(std::move(data), {});
}

void TCPServer::BroadcastTo(std::string data, TCPConnection::ID recipient)
{
    auto msg = std::make_shared<std::string>(std::move(data));

    std::lock_guard lk(_connectionsMtx);

    auto it = _connections.find(recipient);
    if (it != _connections.end())
    {
        it->second->Write(msg);
    }
}

void TCPServer::BroadcastExcept(std::string data, std::vector<TCPConnection::ID> excludeIds)
{
    auto msg = std::make_shared<std::string>(data);
    {
        std::lock_guard lk(_connectionsMtx);

        for (const auto& [id, connection] : _connections)
        {
            auto findIt = std::find(excludeIds.begin(), excludeIds.end(), connection->Id());
            if (findIt != excludeIds.end())
            {
                continue;
            }
            connection->Write(msg);
        }
    }
}
} // namespace cppnet