#include <iostream>

#include "lib/Logger.h"
#include "lib/ITCPHandler.h"
#include "server/TCPServer.h"

#include <exception>
#include <string>

namespace cppnet
{
class ChatHandler : public ITCPHandler<TCPServer>
{
  public:
    void OnConnect(TCPServer& server, IOHandler::ID id) override
    {
        auto message = "Client " + std::to_string(id) + " connected";
        LogInfo(message);
        server.BroadcastAll(std::move(message));
    }

    void OnDisconnect(TCPServer& server, IOHandler::ID id) override
    {
        auto message = "Client " + std::to_string(id) + " disconnected";
        LogInfo(message);
        server.BroadcastAll(std::move(message));
    }

    void OnMessage(TCPServer& server, IOHandler::ID id, std::string message) override
    {
        auto formatedMessage = "Client " + std::to_string(id) + ": " + message;
        server.BroadcastExcept(formatedMessage, {id});
    }

    void OnError(TCPServer& /*server*/, IOHandler::ID /*id*/, std::string error) override
    {
        LogError(error);
    }
};
} // namespace cppnet

int main(int, char**)
{
    using namespace cppnet;
    TCPServer server(std::make_unique<ChatHandler>());
    try
    {
        server.Start("127.0.0.1", 8080);
    }
    catch (const std::exception& e)
    {
        LogError(std::string("Fatal error: ") + e.what());
        return 1;
    }
    server.Join();
}
