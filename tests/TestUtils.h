#pragma once

#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace cppnet::test
{
inline std::string PackRaw(std::string_view message)
{
    const auto netSize = htonl(static_cast<std::uint32_t>(message.size()));
    std::string packet(sizeof(netSize) + message.size(), '\0');
    std::memcpy(packet.data(), &netSize, sizeof(netSize));
    std::memcpy(packet.data() + sizeof(netSize), message.data(), message.size());
    return packet;
}
} // namespace cppnet::test
