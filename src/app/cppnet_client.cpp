#include "client/TCPClient.h"
#include <lib/Logger.h>

#include <atomic>
#include <iostream>
#include <string>

namespace cppnet
{
class ClientHandler : public ITCPHandler<TCPClient>
{
  public:
    ClientHandler(std::atomic_bool& running) : _running(running) {}

    void OnConnect(TCPClient&, IOHandler::ID) override
    {
        LogInfo("Connected to server");
    }

    void OnDisconnect(TCPClient&, IOHandler::ID) override
    {
        LogInfo("Disconnected from server");
        _running.store(false);
    }

    void OnMessage(TCPClient&, IOHandler::ID, std::string message) override
    {
        LogInfo(message);
    }

    void OnError(TCPClient& /*server*/, IOHandler::ID /*id*/, std::string error) override
    {
        LogError(error);
    }

  private:
    std::atomic_bool& _running;
};
} // namespace cppnet

int main()
{
    using namespace cppnet;

    std::atomic_bool running{true};

    TCPClient client(std::make_unique<ClientHandler>(running));

    try
    {
        client.Connect("127.0.0.1", 8080);
    }
    catch (const std::exception& ex)
    {
        LogError(std::string("Connect failed: ") + ex.what());
        return 1;
    }

    std::string line;

    while (running.load())
    {
        if (!std::getline(std::cin, line))
        {
            break;
        }

        if (!running.load())
        {
            break;
        }

        if (line == "/quit")
        {
            break;
        }
        client.Send(line);
    }

    client.Disconnect();
    return 0;
}
