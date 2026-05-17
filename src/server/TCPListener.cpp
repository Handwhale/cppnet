#include "TCPListener.h"
#include <arpa/inet.h>
#include <memory>
#include <netdb.h>
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

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;     // IPv4 или IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    const std::string portStr = std::to_string(_port);

    const int gaiResult = getaddrinfo(address.c_str(), portStr.c_str(), &hints, &result);

    if (gaiResult != 0)
    {
        throw std::runtime_error(std::string("getaddrinfo failed: ") + gai_strerror(gaiResult));
    }

    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addrList(result, freeaddrinfo);
    std::error_code lastError;
    for (addrinfo* addr = addrList.get(); addr != nullptr; addr = addr->ai_next)
    {
        _ufd.Reset(socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol));
        if (!_ufd.IsValid())
        {
            lastError = std::error_code(errno, std::generic_category());
            continue;
        }

        int opt = 1;
        if (setsockopt(_ufd.Get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        {
            lastError = std::error_code(errno, std::generic_category());
            continue;
        }

        if (bind(_ufd.Get(), addr->ai_addr, addr->ai_addrlen) == -1)
        {
            lastError = std::error_code(errno, std::generic_category());
            continue;
        }

        if (listen(_ufd.Get(), K_BACKLOG) == -1)
        {
            lastError = std::error_code(errno, std::generic_category());
            continue;
        }

        return;
    }
    throw std::system_error(lastError, "listen failed");
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