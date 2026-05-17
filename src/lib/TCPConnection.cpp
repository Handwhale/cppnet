#include "TCPConnection.h"

#include "lib/MessageProcessor.h"
#include "lib/Socket.h"

#include <atomic>
#include <mutex>
#include <system_error>
#include <thread>
#include <utility>

namespace cppnet
{
TCPConnection::TCPConnection(ID id, Socket socket, MessageReceivedHandler msgHandler, CloseHandler closeHandler, ErrorHandler errorHandler)
    : _socket(std::move(socket)), _id(id), _closeConnection(false), _closeNotified(false), _msgHandler(msgHandler),
      _errorHandler(errorHandler), _closeHandler(closeHandler)
{
    _writeThread = std::thread(&TCPConnection::WriteLoop, this);
    _readThread = std::thread(&TCPConnection::ReadLoop, this);
}

TCPConnection::~TCPConnection()
{
    Close();

    if (_writeThread.joinable())
    {
        _writeThread.join();
    }
    if (_readThread.joinable())
    {
        _readThread.join();
    }
}

bool TCPConnection::Write(MessagePtr msg)
{
    bool needClose = false;

    {
        std::unique_lock lk(_queueMtx);

        if (IsClosed())
        {
            return false;
        }

        if (_outgoingQueue.size() >= MaxQueueSize)
        {
            needClose = true;
        }
        else
        {
            _outgoingQueue.push(std::move(msg));
        }
    }

    if (needClose)
    {
        Close();
        return false;
    }

    _outgoingCV.notify_one();
    return true;
}

void TCPConnection::Close()
{
    bool expexted = false;
    if (_closeConnection.compare_exchange_strong(expexted, true))
    {

        _socket.Shutdown();
        decltype(_outgoingQueue) empty;
        {
            std::lock_guard lk(_queueMtx);
            std::swap(_outgoingQueue, empty);
        }
        _outgoingCV.notify_one();
    }
}

bool TCPConnection::IsClosed() const noexcept
{
    return _closeConnection.load(std::memory_order_acquire);
}

void TCPConnection::CloseWithNotify()
{
    Close();
    NotifyClosed();
}

void TCPConnection::NotifyClosed()
{
    bool expected = false;
    if (_closeNotified.compare_exchange_strong(expected, true))
    {
        _closeHandler(Id());
    }
}

void TCPConnection::ReadLoop()
{
    MessageProcessor msgReader(_socket);

    for (;;)
    {
        if (IsClosed())
        {
            return;
        }

        auto msgResult = msgReader.ReadMessage();

        switch (msgResult.status)
        {
        case MessageProcessor::ReadStatus::Message:
        {
            _msgHandler(Id(), std::move(msgResult.message));
            break;
        }
        case MessageProcessor::ReadStatus::PeerClosed:
        {
            CloseWithNotify();
            return;
        }
        case MessageProcessor::ReadStatus::TooLong:
        {
            _errorHandler(Id(), "Error: Message too long");
            CloseWithNotify();
            return;
        }
        case MessageProcessor::ReadStatus::Error:
        {
            _errorHandler(Id(), "Error: " + std::error_code(msgResult.error_code, std::generic_category()).message());
            CloseWithNotify();
            return;
        }

        default:
        {
            _errorHandler(Id(), "Unknown error");
            CloseWithNotify();
            return;
        }
        }
    }
}

void TCPConnection::WriteLoop()
{
    MessageProcessor msgWriter(_socket);
    for (;;)
    {
        MessagePtr msg;
        {
            std::unique_lock lk(_queueMtx);
            _outgoingCV.wait(lk, [this]() { return _outgoingQueue.size() > 0 || IsClosed(); });

            if (IsClosed())
            {
                return;
            }

            msg = std::move(_outgoingQueue.front());
            _outgoingQueue.pop();
        }

        auto writeResult = msgWriter.WriteMessage(*msg);

        switch (writeResult.status)
        {
        case MessageProcessor::WriteStatus::Ok:
        {
            break;
        }
        case MessageProcessor::WriteStatus::Error:
        {
            _errorHandler(Id(), "Error: " + std::error_code(writeResult.error_code, std::generic_category()).message());
            CloseWithNotify();
            return;
        }
        case MessageProcessor::WriteStatus::UnableToWrite:
        {
            _errorHandler(Id(), "Error: Unable to write");
            CloseWithNotify();
            return;
        }
        case MessageProcessor::WriteStatus::MessageTooLong:
        {
            _errorHandler(Id(), "Error: Message too long");
            CloseWithNotify();
            return;
        }
        default:
        {
            _errorHandler(Id(), "Unknown error");
            CloseWithNotify();
            return;
        }
        }
    }
}
} // namespace cppnet