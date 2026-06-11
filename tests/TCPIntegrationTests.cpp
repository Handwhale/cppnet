#include "client/TCPClient.h"
#include "lib/ITCPHandler.h"
#include "server/TCPServer.h"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace cppnet
{
namespace
{
using namespace std::chrono_literals;

class EventLog
{
  public:
    void Push(std::string value)
    {
        {
            std::lock_guard lk(_mtx);
            _events.push_back(std::move(value));
        }
        _cv.notify_all();
    }

    bool WaitForCount(std::size_t count, std::chrono::seconds timeout)
    {
        std::unique_lock lk(_mtx);
        return _cv.wait_for(lk, timeout, [&]() { return _events.size() >= count; });
    }

    std::vector<std::string> Snapshot() const
    {
        std::lock_guard lk(_mtx);
        return _events;
    }

  private:
    mutable std::mutex _mtx;
    std::condition_variable _cv;
    std::vector<std::string> _events;
};

class TestServerHandler final : public ITCPHandler<TCPServer>
{
  public:
    TestServerHandler(std::shared_ptr<EventLog> connects,
                      std::shared_ptr<EventLog> messages,
                      std::shared_ptr<EventLog> disconnects,
                      bool broadcastMessages = false)
        : _connects(std::move(connects)), _messages(std::move(messages)), _disconnects(std::move(disconnects)),
          _broadcastMessages(broadcastMessages)
    {
    }

    void OnConnect(TCPServer&, IOHandler::ID id) override
    {
        _connects->Push(std::to_string(id));
    }

    void OnMessage(TCPServer& server, IOHandler::ID, std::string message) override
    {
        _messages->Push(message);
        if (_broadcastMessages)
        {
            server.BroadcastAll(std::move(message));
        }
    }

    void OnDisconnect(TCPServer&, IOHandler::ID id) override
    {
        _disconnects->Push(std::to_string(id));
    }

  private:
    std::shared_ptr<EventLog> _connects;
    std::shared_ptr<EventLog> _messages;
    std::shared_ptr<EventLog> _disconnects;
    bool _broadcastMessages = false;
};

class TestClientHandler final : public ITCPHandler<TCPClient>
{
  public:
    TestClientHandler(std::shared_ptr<EventLog> connects, std::shared_ptr<EventLog> messages, std::shared_ptr<EventLog> disconnects)
        : _connects(std::move(connects)), _messages(std::move(messages)), _disconnects(std::move(disconnects))
    {
    }

    void OnConnect(TCPClient&, IOHandler::ID id) override
    {
        _connects->Push(std::to_string(id));
    }

    void OnMessage(TCPClient&, IOHandler::ID, std::string message) override
    {
        _messages->Push(std::move(message));
    }

    void OnDisconnect(TCPClient&, IOHandler::ID id) override
    {
        _disconnects->Push(std::to_string(id));
    }

  private:
    std::shared_ptr<EventLog> _connects;
    std::shared_ptr<EventLog> _messages;
    std::shared_ptr<EventLog> _disconnects;
};

struct ConnectedPair
{
    std::unique_ptr<TCPServer> server;
    std::unique_ptr<TCPClient> client;
    std::shared_ptr<EventLog> serverConnects;
    std::shared_ptr<EventLog> serverMessages;
    std::shared_ptr<EventLog> serverDisconnects;
    std::shared_ptr<EventLog> clientConnects;
    std::shared_ptr<EventLog> clientMessages;
    std::shared_ptr<EventLog> clientDisconnects;
};

ConnectedPair StartConnectedPair(bool broadcastMessages = false)
{
    ConnectedPair pair;
    pair.serverConnects = std::make_shared<EventLog>();
    pair.serverMessages = std::make_shared<EventLog>();
    pair.serverDisconnects = std::make_shared<EventLog>();
    pair.clientConnects = std::make_shared<EventLog>();
    pair.clientMessages = std::make_shared<EventLog>();
    pair.clientDisconnects = std::make_shared<EventLog>();

    pair.server = std::make_unique<TCPServer>(
        std::make_unique<TestServerHandler>(pair.serverConnects, pair.serverMessages, pair.serverDisconnects, broadcastMessages));
    pair.server->Start("127.0.0.1", 0);

    pair.client = std::make_unique<TCPClient>(
        std::make_unique<TestClientHandler>(pair.clientConnects, pair.clientMessages, pair.clientDisconnects));
    pair.client->Connect("127.0.0.1", pair.server->GetPort());

    return pair;
}

void StopPair(ConnectedPair& pair)
{
    if (pair.client)
    {
        pair.client->Disconnect();
        pair.client->Join();
    }
    if (pair.server)
    {
        pair.server->Stop();
        pair.server->Join();
    }
}
} // namespace

TEST(TCPIntegrationTest, ClientConnectsToServer)
{
    auto pair = StartConnectedPair();

    EXPECT_TRUE(pair.clientConnects->WaitForCount(1, 1s));
    EXPECT_TRUE(pair.serverConnects->WaitForCount(1, 1s));

    StopPair(pair);
}

TEST(TCPIntegrationTest, ClientMessageReachesServer)
{
    auto pair = StartConnectedPair();
    ASSERT_TRUE(pair.serverConnects->WaitForCount(1, 1s));

    pair.client->Send("hello server");

    ASSERT_TRUE(pair.serverMessages->WaitForCount(1, 1s));
    const auto messages = pair.serverMessages->Snapshot();
    EXPECT_EQ(messages[0], "hello server");

    StopPair(pair);
}

TEST(TCPIntegrationTest, ServerBroadcastReachesClient)
{
    auto pair = StartConnectedPair(true);
    ASSERT_TRUE(pair.serverConnects->WaitForCount(1, 1s));

    pair.client->Send("hello client");

    ASSERT_TRUE(pair.clientMessages->WaitForCount(1, 1s));
    const auto messages = pair.clientMessages->Snapshot();
    EXPECT_EQ(messages[0], "hello client");

    StopPair(pair);
}

TEST(TCPIntegrationTest, ClientDisconnectNotifiesClientAndServer)
{
    auto pair = StartConnectedPair();
    ASSERT_TRUE(pair.serverConnects->WaitForCount(1, 1s));

    pair.client->Disconnect();
    pair.client->Join();

    EXPECT_TRUE(pair.clientDisconnects->WaitForCount(1, 1s));
    EXPECT_TRUE(pair.serverDisconnects->WaitForCount(1, 1s));

    pair.server->Stop();
    pair.server->Join();
}
} // namespace cppnet
