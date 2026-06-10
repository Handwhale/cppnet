#include "lib/TCPDataParser.h"
#include "TestUtils.h"

#include <gtest/gtest.h>

#include <string>

namespace cppnet
{
TEST(TCPDataParserTest, ExtractsSinglePackedMessage)
{
    TCPDataParser parser;
    const auto packet = parser.TryPackMessage("hello");

    const auto result = parser.TryExtractMessages(packet.data(), packet.size());

    ASSERT_EQ(result.status, TCPDataParser::ParseStatus::Ok);
    ASSERT_EQ(result.messages.size(), 1u);
    EXPECT_EQ(result.messages[0], "hello");
}

TEST(TCPDataParserTest, ExtractSplittedMessage)
{
    TCPDataParser parser;
    const auto packet = parser.TryPackMessage("split");

    const auto first = parser.TryExtractMessages(packet.data(), 2);
    EXPECT_EQ(first.status, TCPDataParser::ParseStatus::Ok);
    EXPECT_TRUE(first.messages.empty());

    const auto second = parser.TryExtractMessages(packet.data() + 2, packet.size() - 2);
    ASSERT_EQ(second.status, TCPDataParser::ParseStatus::Ok);
    ASSERT_EQ(second.messages.size(), 1u);
    EXPECT_EQ(second.messages[0], "split");
}

TEST(TCPDataParserTest, ExtractsMultipleMessagesFromOneBuffer)
{
    TCPDataParser parser;
    auto packet = parser.TryPackMessage("one");
    packet += parser.TryPackMessage("two");

    const auto result = parser.TryExtractMessages(packet.data(), packet.size());

    ASSERT_EQ(result.status, TCPDataParser::ParseStatus::Ok);
    ASSERT_EQ(result.messages.size(), 2u);
    EXPECT_EQ(result.messages[0], "one");
    EXPECT_EQ(result.messages[1], "two");
}

TEST(TCPDataParserTest, RejectsTooLongMessage)
{
    TCPDataParser parser;
    const auto tooLong = test::PackRaw(std::string(64 * 1024 + 1, 'x'));

    const auto result = parser.TryExtractMessages(tooLong.data(), tooLong.size());

    EXPECT_EQ(result.status, TCPDataParser::ParseStatus::TooLong);
    EXPECT_TRUE(result.messages.empty());
}

TEST(TCPDataParserTest, ResetDropsPartialMessage)
{
    TCPDataParser parser;
    const auto partial = parser.TryPackMessage("stale");
    ASSERT_GT(partial.size(), 2u);
    const auto first = parser.TryExtractMessages(partial.data(), 2);
    ASSERT_EQ(first.status, TCPDataParser::ParseStatus::Ok);
    ASSERT_TRUE(first.messages.empty());

    parser.Reset();
    const auto fresh = parser.TryPackMessage("fresh");
    const auto result = parser.TryExtractMessages(fresh.data(), fresh.size());

    ASSERT_EQ(result.status, TCPDataParser::ParseStatus::Ok);
    ASSERT_EQ(result.messages.size(), 1u);
    EXPECT_EQ(result.messages[0], "fresh");
}
} // namespace cppnet
