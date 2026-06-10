#include "EventFD.h"

#include <cerrno>
#include <cstdint>
#include <sys/eventfd.h>
#include <unistd.h>

namespace cppnet
{
EventFD::EventFD() : _fd(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC))
{
}

int EventFD::Fd() const noexcept
{
    return _fd.Get();
}

bool EventFD::IsValid() const noexcept
{
    return _fd.IsValid();
}

bool EventFD::Push() noexcept
{
    if (!IsValid())
    {
        return false;
    }

    std::uint64_t value = 1;
    for (;;)
    {
        const auto written = write(Fd(), &value, sizeof(value));
        if (written == static_cast<ssize_t>(sizeof(value)))
        {
            return true;
        }

        if (written == -1 && errno == EINTR)
        {
            continue;
        }
        
        // counter is already filled
        if (written == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            return true;
        }

        return false;
    }
}

bool EventFD::Pull() noexcept
{
    if (!IsValid())
    {
        return false;
    }

    for (;;)
    {
        std::uint64_t value = 0;
        const auto bytesRead = read(Fd(), &value, sizeof(value));
        if (bytesRead == static_cast<ssize_t>(sizeof(value)))
        {
            return true;
        }

        if (bytesRead == -1 && errno == EINTR)
        {
            continue;
        }

        if (bytesRead == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            return false;
        }

        return false;
    }
}

void EventFD::Reset() noexcept
{
    _fd.Reset();
}
} // namespace cppnet
