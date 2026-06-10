#include "lib/EventFD.h"

#include <gtest/gtest.h>

namespace cppnet
{
TEST(EventFDTest, DoublePull)
{
    EventFD eventFd;

    ASSERT_TRUE(eventFd.IsValid());
    EXPECT_TRUE(eventFd.Push());
    EXPECT_TRUE(eventFd.Pull());
    EXPECT_FALSE(eventFd.Pull());
}

TEST(EventFDTest, DoublePush)
{
    EventFD eventFd;

    ASSERT_TRUE(eventFd.IsValid());
    EXPECT_TRUE(eventFd.Push());
    EXPECT_TRUE(eventFd.Push());
    EXPECT_TRUE(eventFd.Pull());
    EXPECT_FALSE(eventFd.Pull());
}
} // namespace cppnet
