#pragma once
#include <unistd.h>

namespace cppnet
{
class UniqueFD final
{
  public:
    explicit UniqueFD(int fd = -1) noexcept : _fd(fd) {}

    ~UniqueFD()
    {
        Reset();
    }

    UniqueFD(const UniqueFD&) = delete;
    UniqueFD& operator=(const UniqueFD&) = delete;

    UniqueFD(UniqueFD&& other) noexcept
    {
        _fd = other._fd;
        other._fd = -1;
    }

    UniqueFD& operator=(UniqueFD&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            _fd = other._fd;
            other._fd = -1;
        }
        return *this;
    }

    void Reset(int fd = -1) noexcept
    {
        if (IsValid())
        {
            close(_fd);
        }
        _fd = fd;
    }

    int Get() const noexcept
    {
        return _fd;
    }

    bool IsValid() const noexcept
    {
        return _fd != -1;
    }

  private:
    int _fd;
};
} // namespace cppnet