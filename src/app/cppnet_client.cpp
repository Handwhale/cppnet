#include <atomic>
#include <iostream>
#include <mutex>
#include <string>

#include "client/TCPClient.h"

int main()
{
    using namespace cppnet;

    std::mutex terminalMutex;
    std::atomic_bool running{true};

    TCPClient client;

    client.OnMessage(
        [&](std::string message)
        {
            std::lock_guard lock(terminalMutex);
            std::cout << "\n" << message << '\n';
        });

    client.OnDisconnect(
        [&]()
        {
            {
                std::lock_guard lock(terminalMutex);
                std::cout << "\n[System] Disconnected from server\n";
            }

            running.store(false);
        });

    client.OnError(
        [&](std::string error)
        {
            std::lock_guard lock(terminalMutex);
            std::cout << "\n[System] " << error << "\n";
        });

    try
    {
        client.Connect("127.0.0.1", 8080);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[System] Connect failed: " << ex.what() << "\n";
        return 1;
    }

    std::cout << "[System] Connected to server\n";

    std::string line;

    while (running.load())
    {
        {
            std::lock_guard lock(terminalMutex);
            std::cout << "> " << std::flush;
        }

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

        if (!client.Send(line))
        {
            std::lock_guard lock(terminalMutex);
            std::cout << "\n[System] Send failed\n";
            break;
        }
    }

    client.Disconnect();
    return 0;
}