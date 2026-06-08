#pragma once
#include "lib/IOHandler.h"

namespace cppnet
{
class TCPServer;

class ITCPServerHandler
{
  public:
    virtual ~ITCPServerHandler() = default;
    virtual void OnConnect(TCPServer& /*server*/, IOHandler::ID /*id*/) {}
    virtual void OnMessage(TCPServer& /*server*/, IOHandler::ID /*id*/, std::string /*message*/) {}
    virtual void OnDisconnect(TCPServer& /*server*/, IOHandler::ID /*id*/) {}
    virtual void OnError(TCPServer& /*server*/, IOHandler::ID /*id*/, std::string /*error*/) {}
};
} // namespace cppnet