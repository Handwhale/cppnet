#include "lib/UniqueFD.h"

#include <gtest/gtest.h>

#include <unistd.h>
#include <utility>

namespace cppnet
{
TEST(UniqueFDTest, MoveOwnership)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(pipe(fds), 0);

    UniqueFD readEnd(fds[0]);
    UniqueFD writeEnd(fds[1]);
    const int readFd = readEnd.Get();

    UniqueFD moved(std::move(readEnd));

    EXPECT_FALSE(readEnd.IsValid());
    EXPECT_EQ(moved.Get(), readFd);
    EXPECT_TRUE(writeEnd.IsValid());
}
} // namespace cppnet
