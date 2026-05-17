#pragma once
#include "Socket.h"
#include <cstdint>
#include <string>

namespace cppnet
{
class MessageProcessor
{
  public:
    enum class ReadStatus
    {
        Message,
        PeerClosed,
        TooLong,
        Error
    };

    struct ReadResult
    {
        ReadStatus status;
        std::string message;
        int error_code = 0;

        static ReadResult Message(std::string value);
        static ReadResult PeerClosed();
        static ReadResult TooLong();
        static ReadResult Error(int error_code);
    };

    enum class WriteStatus
    {
        Ok,
        Error,
        UnableToWrite,
        MessageTooLong
    };

    struct WriteResult
    {
        WriteStatus status;
        int error_code = 0;

        static WriteResult Ok();
        static WriteResult Error(int error_code);
        static WriteResult UnableToWrite();
        static WriteResult MessageTooLong();
    };

    MessageProcessor(Socket& socket);

    ReadResult ReadMessage();
    WriteResult WriteMessage(std::string_view message);

  private:
    Socket& _socket;
    std::string _buffer;

    static constexpr std::uint32_t kMaxMessageSize = 64 * 1024; // 64kb
    static constexpr std::uint32_t KHeaderSize = 4;             // int32
};
} // namespace cppnet