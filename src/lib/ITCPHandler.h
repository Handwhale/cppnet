#pragma once
#include "lib/IOHandler.h"

namespace cppnet
{
template<typename T>
class ITCPHandler
{
  public:
    virtual ~ITCPHandler() = default;
    virtual void OnConnect(T& /*server*/, IOHandler::ID /*id*/) {}
    virtual void OnMessage(T& /*server*/, IOHandler::ID /*id*/, std::string /*message*/) {}
    virtual void OnDisconnect(T& /*server*/, IOHandler::ID /*id*/) {}
    virtual void OnError(T& /*server*/, IOHandler::ID /*id*/, std::string /*error*/) {}
};
} // namespace cppnet