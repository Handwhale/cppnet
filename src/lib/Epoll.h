#pragma once

#include "lib/UniqueFD.h"

#include <sys/epoll.h>

namespace cppnet
{
class Epoll
{
  public:
    Epoll();

    Epoll(const Epoll&) = delete;
    Epoll(Epoll&&) = delete;

    Epoll& operator=(const Epoll&) = delete;
    Epoll& operator=(Epoll&&) = delete;

    int Add(int fd, u_int32_t events, void* data = nullptr);
    int Modify(int fd, u_int32_t events, void* data = nullptr);
    int Remove(int fd);
    int Wait(epoll_event* events, int maxEvents, int timeout = 0);

    static u_int32_t CreateEvents(bool read = false, bool write = false, bool oneshoot = false, bool edgeTriggered = false);

  private:
    int SysCall(int fd, u_int32_t events, void* data, int op);

  private:
    UniqueFD _epollFD;
};
} // namespace cppnet