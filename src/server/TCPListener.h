#pragma once
#include "lib/Socket.h"
#include "lib/UniqueFD.h"
#include <cstdint>
#include <string>

namespace cppnet
{
class TCPListener
{
  public:
    struct AcceptResult
    {
        Socket socket;
        int error = 0;
    };

    TCPListener(UniqueFD ufd = UniqueFD());
    TCPListener(const TCPListener&) = delete;
    TCPListener& operator=(const TCPListener&) = delete;

    TCPListener(TCPListener&&) noexcept = default;
    TCPListener& operator=(TCPListener&&) noexcept = default;

    void Open(std::string address, uint16_t port);
    AcceptResult Accept();
    bool IsValid() const noexcept;
    void Reset(UniqueFD ufd = UniqueFD()) noexcept;

    std::string GetAddress() const noexcept
    {
        return _address;
    }
    uint16_t GetPort() const noexcept
    {
        return _port;
    }

  private:
    UniqueFD _ufd;
    std::string _address;
    uint16_t _port = 0;
    static constexpr int K_BACKLOG = 10;
};
} // namespace cppnet