#include "TCPConnector.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <system_error>

namespace cppnet
{
Socket TCPConnector::Connect(std::string host, uint16_t port)
{
    UniqueFD clientFD(socket(AF_INET, SOCK_STREAM, 0));
    if (clientFD.Get() < 0)
    {
        throw std::system_error(errno, std::generic_category(), "socket failed");
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.data(), &serverAddr.sin_addr) != 1)
    {
        throw std::system_error(errno, std::generic_category(), "inet_pton failed");
    }

    if (connect(clientFD.Get(), reinterpret_cast<const sockaddr*>(&serverAddr), sizeof(serverAddr)) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "connect failed");
    }

    return Socket(std::move(clientFD));
}
} // namespace cppnet