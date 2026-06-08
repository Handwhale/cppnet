#include "Epoll.h"

namespace cppnet
{
Epoll::Epoll() : _epollFD(epoll_create1(EPOLL_CLOEXEC)) {}

int Epoll::Add(int fd, u_int32_t events, void* data)
{
    return SysCall(fd, events, data, EPOLL_CTL_ADD);
}

int Epoll::Modify(int fd, u_int32_t events, void* data)
{
    return SysCall(fd, events, data, EPOLL_CTL_MOD);
}

int Epoll::Remove(int fd)
{
    return SysCall(fd, 0, nullptr, EPOLL_CTL_DEL);
}

int Epoll::Wait(epoll_event* events, int maxEvents, int timeout)
{
    return epoll_wait(_epollFD.Get(), events, maxEvents, timeout);
}

int Epoll::SysCall(int fd, u_int32_t events, void* data, int op)
{
    epoll_event epollEvent{};
    epollEvent.events = events;
    if (data == nullptr)
    {
        epollEvent.data.fd = fd;
    }
    else
    {
        epollEvent.data.ptr = data;
    }

    return epoll_ctl(_epollFD.Get(), op, fd, &epollEvent);
}

u_int32_t Epoll::CreateEvents(bool read, bool write, bool oneshoot, bool edgeTriggered)
{
    u_int32_t events = 0;
    if (read)
        events |= EPOLLIN;
    if (write)
        events |= EPOLLOUT;
    if (oneshoot)
        events |= EPOLLONESHOT;
    if (edgeTriggered)
        events |= EPOLLET;

    return events;
}
} // namespace cppnet