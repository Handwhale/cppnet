#pragma once

#include <cstddef>
#include <sys/socket.h>

#include "UniqueFD.h"

namespace cppnet
{
class Socket
{
  public:
    explicit Socket(UniqueFD&& ufd = UniqueFD());

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&&) noexcept = default;
    Socket& operator=(Socket&&) noexcept = default;

    ssize_t Send(const char* buffer, std::size_t size);
    ssize_t Recv(char* buffer, std::size_t size);
    void Shutdown();
    bool IsValid() const;
    const UniqueFD& GetUFD() const { return _ufd; }

  private:
    UniqueFD _ufd;
};
} // namespace cppnet