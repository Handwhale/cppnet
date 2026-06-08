#include "Socket.h"

#include <cerrno>

namespace cppnet
{
Socket::Socket(UniqueFD&& ufd) : _ufd(std::move(ufd)) {}

ssize_t Socket::Send(const std::string_view message)
{
    size_t total_sent = 0;

    while (total_sent < message.size())
    {
        ssize_t sent = send(_ufd.Get(), message.data() + total_sent, message.size() - total_sent, MSG_NOSIGNAL);

        if (sent > 0)
        {
            total_sent += static_cast<size_t>(sent);
            continue;
        }

        if (sent == 0)
        {
            return 0;
        }

        if (errno == EAGAIN || errno == EINTR)
        {
            continue;
        }
        return -1;
    }

    return static_cast<ssize_t>(total_sent);
}

ssize_t Socket::SendSome(const char* buffer, std::size_t size)
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