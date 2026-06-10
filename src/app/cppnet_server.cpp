#include <iostream>

#include "lib/TCPConnection.h"
#include "server/ITCPServerHandler.h"
#include "server/TCPServer.h"

#include <exception>
#include <string>

namespace cppnet
{
class ChatHandler : public ITCPServerHandler
{
  public:
    void OnConnect(TCPServer& server, TCPConnection::ID id) override
    {
        auto message = "[System] Client " + std::to_string(id) + " connected";
        std::cout << message << '\n';
        server.BroadcastAll(message);
    }

    void OnDisconnect(TCPServer& server, TCPConnection::ID id) override
    {
        auto message = "[System] Client " + std::to_string(id) + " disconnected";
        std::cout << message << '\n';
        server.BroadcastAll(message);
    }

    void OnMessage(TCPServer& server, TCPConnection::ID id, std::string message) override
    {
        auto formatedMessage = "Client " + std::to_string(id) + ": " + message;
        server.BroadcastExcept(formatedMessage, {id});
    }

    void OnError(TCPServer& /*server*/, TCPConnection::ID /*id*/, std::string error) override
    {

        std::cout << "[System] Error: " << error << '\n';
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
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    }
    server.Join();
}