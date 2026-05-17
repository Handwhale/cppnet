#include "TCPListener.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

namespace cppnet
{
TCPListener::TCPListener(UniqueFD ufd) : _ufd(std::move(ufd)) {}

void TCPListener::Open(std::string address, uint16_t port)
{

    _address = address;
    _port = port;
    _ufd.Reset(socket(AF_INET, SOCK_STREAM, 0));
    if (!_ufd.IsValid())
    {
        throw std::system_error(errno, std::generic_category(), "socket failed");
    }

    int opt = 1;
    if (setsockopt(_ufd.Get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "setsockopt SO_REUSEADDR failed");
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;

    server_addr.sin_port = htons(_port);
    if (inet_pton(AF_INET, address.c_str(), &server_addr.sin_addr) != 1)
    {
        throw std::invalid_argument("invalid IPv4 address");
    }

    if (bind(_ufd.Get(), reinterpret_cast<const sockaddr*>(&server_addr), sizeof(server_addr)) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "bind failed");
    }

    if (listen(_ufd.Get(), K_BACKLOG) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "listen failed");
    }
}

TCPListener::AcceptResult TCPListener::Accept()
{
    if (!IsValid())
    {
        return AcceptResult{Socket(UniqueFD()), 0};
    }

    sockaddr_in client_addr{};
    socklen_t client_addr_size = sizeof(client_addr);
    auto socket = Socket(UniqueFD(accept(_ufd.Get(), reinterpret_cast<sockaddr*>(&client_addr), &client_addr_size)));
    if (!socket.IsValid())
    {
        return AcceptResult{std::move(socket), errno};
    }
    return AcceptResult{std::move(socket), 0};
}

bool TCPListener::IsValid() const noexcept
{
    return _ufd.IsValid();
}

void TCPListener::Reset(UniqueFD ufd) noexcept
{
    _ufd = std::move(ufd);
}
} // namespace cppnet