#pragma once
#include "lib/Socket.h"
#include "lib/TCPDataParser.h"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace cppnet
{
class IOHandler final
{
  public:
    using ID = std::size_t;
    using MessageHandler = std::function<void(IOHandler::ID, std::string)>;
    using ErrorHandler = std::function<void(IOHandler::ID, std::string)>;

    struct IOUpdate
    {
        bool wantWrite = false;
        bool shouldClose = false;
    };

    IOHandler(const IOHandler&) = delete;
    IOHandler& operator=(const IOHandler&) = delete;

    IOHandler(IOHandler&&) noexcept = default;
    IOHandler& operator=(IOHandler&&) noexcept = default;

    IOHandler(ID id, Socket socket, MessageHandler msgHandler, ErrorHandler errorHandler);
    ~IOHandler();

    ID Id() const noexcept;
    int Fd() const noexcept;
    bool IsClosed() const noexcept;
    bool WantsWrite() const noexcept;

    IOUpdate Read();
    IOUpdate Write();
    IOUpdate QueueMessage(std::string_view message);
    IOUpdate Close();

  private:
    IOUpdate CurrentUpdate() const noexcept;

  private:
    ID _id = 0;
    Socket _socket;
    TCPDataParser _parser;
    std::string _writeBuffer;
    std::size_t _writeOffset = 0;
    bool _closed = false;

    MessageHandler _msgHandler;
    ErrorHandler _errorHandler;
};
} // namespace cppnet
