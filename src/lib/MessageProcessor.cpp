#include "MessageProcessor.h"
#include <arpa/inet.h>
#include <cstring>

namespace cppnet
{
MessageProcessor::ReadResult MessageProcessor::ReadResult::Message(std::string value)
{
    return ReadResult{ReadStatus::Message, std::move(value), 0};
}

MessageProcessor::ReadResult MessageProcessor::ReadResult::PeerClosed()
{
    return ReadResult{ReadStatus::PeerClosed, {}, 0};
}

MessageProcessor::ReadResult MessageProcessor::ReadResult::TooLong()
{
    return ReadResult{ReadStatus::TooLong, {}, 0};
}

MessageProcessor::ReadResult MessageProcessor::ReadResult::Error(int error_code)
{
    return ReadResult{ReadStatus::Error, {}, error_code};
};

MessageProcessor::WriteResult MessageProcessor::WriteResult::Ok()
{
    return WriteResult{WriteStatus::Ok, 0};
};

MessageProcessor::WriteResult MessageProcessor::WriteResult::Error(int error_code)
{
    return WriteResult{WriteStatus::Error, error_code};
};

MessageProcessor::WriteResult MessageProcessor::WriteResult::UnableToWrite()
{
    return WriteResult{WriteStatus::UnableToWrite, 0};
};

MessageProcessor::WriteResult MessageProcessor::WriteResult::MessageTooLong()
{
    return WriteResult{WriteStatus::MessageTooLong, 0};
};

MessageProcessor::MessageProcessor(Socket& socket) : _socket(socket) {}

MessageProcessor::ReadResult MessageProcessor::ReadMessage()
{
    for (;;)
    {
        if (_buffer.size() >= KHeaderSize)
        {
            std::uint32_t netMsgSize = 0;
            std::memcpy(&netMsgSize, _buffer.data(), KHeaderSize);
            const std::uint32_t msgSize = ntohl(netMsgSize);
            if (msgSize > kMaxMessageSize)
            {
                return ReadResult::TooLong();
            }

            const auto fullMsgSize = KHeaderSize + msgSize;
            if (_buffer.size() >= fullMsgSize)
            {
                auto message = _buffer.substr(KHeaderSize, msgSize);
                _buffer.erase(0, fullMsgSize);
                return ReadResult::Message(std::move(message));
            }
        }

        char tmpBuf[1024];
        ssize_t bytes_read = _socket.Recv(tmpBuf, sizeof(tmpBuf));

        if (bytes_read > 0)
        {
            _buffer.append(tmpBuf, static_cast<std::size_t>(bytes_read));

            continue;
        }

        if (bytes_read == 0)
        {
            return ReadResult::PeerClosed();
        }

        if (errno == EINTR)
        {
            continue;
        }

        return ReadResult::Error(errno);
    }
}

MessageProcessor::WriteResult MessageProcessor::WriteMessage(std::string_view message)
{

    if (message.size() > kMaxMessageSize)
    {
        return WriteResult::MessageTooLong();
    }

    auto msgSize = htonl(static_cast<std::uint32_t>(message.size()));

    std::string packet;
    packet.resize(KHeaderSize + message.size());

    std::memcpy(packet.data(), &msgSize, KHeaderSize);
    std::memcpy(packet.data() + KHeaderSize, message.data(), message.size());

    auto result = _socket.Send(packet);
    if (result > 0)
    {
        return WriteResult::Ok();
    }
    if (result == 0)
    {
        return WriteResult::UnableToWrite();
    }
    return WriteResult::Error(errno);
}
} // namespace cppnet