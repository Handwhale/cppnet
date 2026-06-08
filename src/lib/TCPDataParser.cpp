#include "TCPDataParser.h"

#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace cppnet
{
TCPDataParser::ParseResult TCPDataParser::ParseResult::Ok(std::vector<std::string> value)
{
    return ParseResult{ParseStatus::Ok, std::move(value)};
}

TCPDataParser::ParseResult TCPDataParser::ParseResult::TooLong()
{
    return ParseResult{ParseStatus::TooLong, {}};
}

TCPDataParser::ParseResult TCPDataParser::TryExtractMessages(const char* buffer, std::size_t size)
{
    if (buffer != nullptr && size > 0)
    {
        _buffer.append(buffer, size);
    }

    std::vector<std::string> messages;
    for (;;)
    {
        if (_buffer.size() < KHeaderSize)
        {
            return ParseResult::Ok(std::move(messages));
        }

        std::uint32_t netMsgSize = 0;
        std::memcpy(&netMsgSize, _buffer.data(), KHeaderSize);
        const std::uint32_t msgSize = ntohl(netMsgSize);
        if (msgSize > kMaxMessageSize)
        {
            Reset();
            return ParseResult::TooLong();
        }

        const auto fullMsgSize = static_cast<std::size_t>(KHeaderSize) + msgSize;
        if (_buffer.size() < fullMsgSize)
        {
            return ParseResult::Ok(std::move(messages));
        }

        messages.emplace_back(_buffer.substr(KHeaderSize, msgSize));
        _buffer.erase(0, fullMsgSize);
    }
}

std::string TCPDataParser::PackMessage(std::string_view message) const
{
    if (message.size() > kMaxMessageSize)
    {
        throw std::length_error("message is too long");
    }

    auto msgSize = htonl(static_cast<std::uint32_t>(message.size()));

    std::string packet;
    packet.resize(KHeaderSize + message.size());

    std::memcpy(packet.data(), &msgSize, KHeaderSize);
    std::memcpy(packet.data() + KHeaderSize, message.data(), message.size());

    return packet;
}

void TCPDataParser::Reset()
{
    _buffer.clear();
}
} // namespace cppnet
