#include "TestUtils.h"
#include "lib/IOHandler.h"
#include "lib/Socket.h"
#include "lib/UniqueFD.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <utility>
#include <vector>

namespace cppnet
{
namespace
{
class SocketPair
{
  public:
    SocketPair()
    {
        int fds[2] = {-1, -1};
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, fds) == -1)
        {
            throw std::runtime_error(std::strerror(errno));
        }

        first = UniqueFD(fds[0]);
        second = UniqueFD(fds[1]);
    }

    UniqueFD first;
    UniqueFD second;
};

void WriteAll(int fd, std::string_view data)
{
    std::size_t offset = 0;
    while (offset < data.size())
    {
        const auto written = send(fd, data.data() + offset, data.size() - offset, MSG_NOSIGNAL);
        ASSERT_GT(written, 0) << std::strerror(errno);
        offset += static_cast<std::size_t>(written);
    }
}

} // namespace

TEST(IOHandlerTest, SingleMessage)
{
    SocketPair pair;
    std::vector<std::string> receivedMessages;
    std::vector<std::string> writerErrors;
    std::vector<std::string> readerErrors;

    IOHandler writer(
        1, Socket(std::move(pair.first)), [](IOHandler::ID, std::string) { ASSERT_TRUE(false); },
        [&](IOHandler::ID, std::string error) { writerErrors.push_back(std::move(error)); });
    IOHandler reader(
        2, Socket(std::move(pair.second)),
        [&](IOHandler::ID id, std::string message)
        {
            EXPECT_EQ(id, 2u);
            receivedMessages.push_back(std::move(message));
        },
        [&](IOHandler::ID, std::string error) { readerErrors.push_back(std::move(error)); });

    auto update = writer.QueueMessage("hello");
    ASSERT_TRUE(update.wantWrite);
    ASSERT_FALSE(update.shouldClose);

    update = writer.Write();
    EXPECT_FALSE(update.wantWrite);
    EXPECT_FALSE(update.shouldClose);

    const auto readUpdate = reader.Read();

    EXPECT_FALSE(readUpdate.wantWrite);
    EXPECT_FALSE(readUpdate.shouldClose);
    EXPECT_TRUE(writerErrors.empty());
    EXPECT_TRUE(readerErrors.empty());
    ASSERT_EQ(receivedMessages.size(), 1u);
    EXPECT_EQ(receivedMessages[0], "hello");
}

TEST(IOHandlerTest, MultipleMessages)
{
    SocketPair pair;
    std::vector<std::string> receivedMessages;
    std::vector<std::string> errors;

    IOHandler writer(
        1, Socket(std::move(pair.first)), [](IOHandler::ID, std::string) {},
        [&](IOHandler::ID, std::string error) { errors.push_back(std::move(error)); });
    IOHandler reader(
        2, Socket(std::move(pair.second)), [&](IOHandler::ID, std::string message) { receivedMessages.push_back(std::move(message)); },
        [&](IOHandler::ID, std::string error) { errors.push_back(std::move(error)); });

    EXPECT_TRUE(writer.QueueMessage("one").wantWrite);
    EXPECT_TRUE(writer.QueueMessage("two").wantWrite);

    const auto writeUpdate = writer.Write();
    EXPECT_FALSE(writeUpdate.wantWrite);
    EXPECT_FALSE(writeUpdate.shouldClose);

    const auto readUpdate = reader.Read();
    EXPECT_FALSE(readUpdate.shouldClose);
    EXPECT_TRUE(errors.empty());
    ASSERT_EQ(receivedMessages.size(), 2u);
    EXPECT_EQ(receivedMessages[0], "one");
    EXPECT_EQ(receivedMessages[1], "two");
}

TEST(IOHandlerTest, CloseOnReadTooLongMessage)
{
    SocketPair pair;
    std::vector<std::string> errors;

    IOHandler handler(
        11, Socket(std::move(pair.first)), [](IOHandler::ID, std::string) {},
        [&](IOHandler::ID, std::string error) { errors.push_back(std::move(error)); });

    const auto packet = test::PackRaw(std::string(64 * 1024 + 1, 'x'));
    ASSERT_NO_FATAL_FAILURE(WriteAll(pair.second.Get(), packet));

    const auto update = handler.Read();

    EXPECT_TRUE(update.shouldClose);
    EXPECT_TRUE(handler.IsClosed());
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0], "Error: Message too long");
}

TEST(IOHandlerTest, CloseOnWriteTooLongMessage)
{
    SocketPair pair;
    std::vector<std::string> errors;

    IOHandler handler(
        12, Socket(std::move(pair.first)), [](IOHandler::ID, std::string) {},
        [&](IOHandler::ID, std::string error) { errors.push_back(std::move(error)); });

    const auto update = handler.QueueMessage(std::string(64 * 1024 + 1, 'x'));

    EXPECT_TRUE(update.shouldClose);
    EXPECT_TRUE(handler.IsClosed());
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0], "Error: Message too long");
}
} // namespace cppnet
