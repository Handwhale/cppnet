#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cppnet
{
class TCPDataParser
{
  public:
    enum class ParseStatus
    {
        Ok,
        TooLong
    };

    struct ParseResult
    {
        ParseStatus status = ParseStatus::Ok;
        std::vector<std::string> messages;

        static ParseResult Ok(std::vector<std::string> value);
        static ParseResult TooLong();
    };

    ParseResult TryExtractMessages(const char* buffer, std::size_t size);
    std::string TryPackMessage(std::string_view message) const;
    bool IsValidMessage(std::string_view message) const;

    void Reset();

  private:
    std::string _buffer;

    static constexpr std::uint32_t kMaxMessageSize = 64 * 1024; // 64kb
    static constexpr std::uint32_t KHeaderSize = 4;             // int32
};
} // namespace cppnet
