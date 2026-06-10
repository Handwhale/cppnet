#pragma once

#include "lib/UniqueFD.h"

namespace cppnet
{
class EventFD final
{
  public:
    EventFD();

    EventFD(const EventFD&) = delete;
    EventFD& operator=(const EventFD&) = delete;

    EventFD(EventFD&&) noexcept = default;
    EventFD& operator=(EventFD&&) noexcept = default;

    int Fd() const noexcept;
    bool IsValid() const noexcept;

    bool Push() noexcept;
    bool Pull() noexcept;
    void Reset() noexcept;

  private:
    UniqueFD _fd;
};
} // namespace cppnet
