#include "IOHandler.h"

#include <cerrno>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <system_error>
#include <utility>

namespace cppnet
{
IOHandler::IOHandler(ID id, Socket socket, MessageHandler msgHandler, ErrorHandler errorHandler)
    : _id(id), _socket(std::move(socket)), _msgHandler(std::move(msgHandler)), _errorHandler(std::move(errorHandler))
{
}

IOHandler::~IOHandler()
{
    Close();
}

IOHandler::ID IOHandler::Id() const noexcept
{
    return _id;
}

int IOHandler::Fd() const noexcept
{
    return _socket.GetUFD().Get();
}

bool IOHandler::IsClosed() const noexcept
{
    return _closed;
}

bool IOHandler::WantsWrite() const noexcept
{
    return !_closed && _writeOffset < _writeBuffer.size();
}

IOHandler::IOUpdate IOHandler::Read()
{
    if (_closed)
    {
        return CurrentUpdate();
    }

    for (;;)
    {
        char tmpBuf[4096];
        const ssize_t bytesRead = _socket.Recv(tmpBuf, sizeof(tmpBuf));

        if (bytesRead > 0)
        {
            auto parseResult = _parser.TryExtractMessages(tmpBuf, static_cast<std::size_t>(bytesRead));
            if (parseResult.status == TCPDataParser::ParseStatus::TooLong)
            {
                _errorHandler(Id(), "Error: Message too long");
                return Close();
            }

            for (auto& message : parseResult.messages)
            {
                _msgHandler(Id(), std::move(message));
                if (_closed)
                {
                    return CurrentUpdate();
                }
            }

            continue;
        }

        if (bytesRead == 0)
        {
            return Close();
        }

        if (errno == EINTR)
        {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return CurrentUpdate();
        }

        _errorHandler(Id(), "Error: " + std::error_code(errno, std::generic_category()).message());
        return Close();
    }
}

IOHandler::IOUpdate IOHandler::Write()
{
    if (_closed)
    {
        return CurrentUpdate();
    }

    while (_writeOffset < _writeBuffer.size())
    {
        const auto* data = _writeBuffer.data() + _writeOffset;
        const auto size = _writeBuffer.size() - _writeOffset;
        const ssize_t bytesSent = _socket.SendSome(data, size);
        if (bytesSent > 0)
        {
            _writeOffset += static_cast<std::size_t>(bytesSent);
            continue;
        }

        if (bytesSent == 0)
        {
            return Close();
        }

        if (errno == EINTR)
        {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return CurrentUpdate();
        }

        _errorHandler(Id(), "Error: " + std::error_code(errno, std::generic_category()).message());
        return Close();
    }

    _writeBuffer.clear();
    _writeOffset = 0;
    return CurrentUpdate();
}

IOHandler::IOUpdate IOHandler::QueueMessage(std::string_view message)
{
    if (_closed)
    {
        return CurrentUpdate();
    }

    try
    {
        _writeBuffer += _parser.PackMessage(message);
    }
    catch (const std::length_error&)
    {
        _errorHandler(Id(), "Error: Message too long");
        return Close();
    }

    return CurrentUpdate();
}

IOHandler::IOUpdate IOHandler::CurrentUpdate() const noexcept
{
    return IOUpdate{WantsWrite(), _closed};
}

IOHandler::IOUpdate IOHandler::Close()
{
    if (!_closed)
    {
        _closed = true;
        _socket.Shutdown();
        _parser.Reset();
        _writeBuffer.clear();
        _writeOffset = 0;
    }

    return CurrentUpdate();
}
} // namespace cppnet
