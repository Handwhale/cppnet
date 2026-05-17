#pragma once
#include "lib/TCPConnection.h"

namespace cppnet
{
class TCPServer;

class ITCPServerHandler
{
  public:
    virtual ~ITCPServerHandler() = default;
    virtual void OnConnect(TCPServer& /*server*/, TCPConnection::ID /*id*/) {}
    virtual void OnMessage(TCPServer& /*server*/, TCPConnection::ID /*id*/, std::string /*message*/) {}
    virtual void OnDisconnect(TCPServer& /*server*/, TCPConnection::ID /*id*/) {}
    virtual void OnError(TCPServer& /*server*/, TCPConnection::ID /*id*/, std::string /*error*/) {}
};
} // namespace cppnet