#include "TCPConnector.h"

#include <arpa/inet.h>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <system_error>

namespace cppnet
{
Socket TCPConnector::Connect(std::string host, uint16_t port)
{
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    const std::string portStr = std::to_string(port);
    const int gaiResult = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result);
    if (gaiResult != 0)
    {
        throw std::runtime_error(std::string("getaddrinfo failed: ") + gai_strerror(gaiResult));
    }

    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addrList(result, freeaddrinfo);

    std::error_code lastError;
    for (addrinfo* addr = addrList.get(); addr != nullptr; addr = addr->ai_next)
    {
        UniqueFD clientFD(socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol));
        if (!clientFD.IsValid())
        {
            lastError = std::error_code(errno, std::generic_category());
            continue;
        }

        if (connect(clientFD.Get(), addr->ai_addr, addr->ai_addrlen) == 0)
        {
            return Socket(std::move(clientFD));
        }
        lastError = std::error_code(errno, std::generic_category());
    }

    throw std::system_error(lastError, "connect failed");
}
} // namespace cppnet