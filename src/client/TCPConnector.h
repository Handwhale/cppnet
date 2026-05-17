#pragma once

#include "lib/Socket.h"
#include <stdint.h>

namespace cppnet
{
class TCPConnector
{
  public:
    static Socket Connect(std::string host, uint16_t port);
};
} // namespace cppnet