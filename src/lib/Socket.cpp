#include "Socket.h"

#include <cerrno>
#include <utility>

namespace cppnet
{
Socket::Socket(UniqueFD&& ufd) : _ufd(std::move(ufd)) {}

ssize_t Socket::Send(const char* buffer, std::size_t size)
{
    return send(_ufd.Get(), buffer, size, MSG_NOSIGNAL);
}

ssize_t Socket::Recv(char* buffer, std::size_t size)
{
    return recv(_ufd.Get(), buffer, size, 0);
}

void Socket::Shutdown()
{
    if (IsValid())
    {
        shutdown(_ufd.Get(), SHUT_RDWR);
    }
}

bool Socket::IsValid() const
{
    return _ufd.Get() != -1;
}
} // namespace cppnet