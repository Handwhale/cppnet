# cppnet

`cppnet` is a small C++ TCP networking project built around Linux `epoll`.

The project contains a reusable TCP client/server layer, a length-prefixed message protocol, and simple console chat demo applications.

## Features

- Non-blocking sockets
- `epoll`-based event loops
- `eventfd` wakeups for cross-thread commands
- Length-prefixed TCP message framing
- Shared `IOHandler` for read/write buffering
- Thread-safe callbacks
- GoogleTest-based test target

## Platform

Project targets Linux

## Requirements

- C++17-capable compiler
- CMake 3.10+
- Linux
- Git submodule for GoogleTest

If GoogleTest is not present yet:

```bash
git submodule update --init --recursive
```

## Build

Configure:

```bash
cmake -S . -B out/build/debug -DCMAKE_BUILD_TYPE=Debug
```

Build everything:

```bash
cmake --build out/build/debug
```

## Run Demo

Start the server:

```bash
./out/build/debug/cppnet_server
```

Start clients in another terminals:

```bash
./out/build/debug/cppnet_client
```

By default, the demo uses:

```text
127.0.0.1:8080
```

Type messages in the client terminal. Use:

```text
/quit
```

to exit the client.

## Tests

Run all tests through CTest:

```bash
ctest --test-dir out/build/debug --output-on-failure
```

Run the GoogleTest binary directly:

```bash
./out/build/debug/cppnet_tests
```

## Project Layout

```text
src/
  app/
    cppnet_client.cpp      # console client demo
    cppnet_server.cpp      # console server demo

  client/
    TCPClient.*            # epoll-based TCP client
    TCPConnector.*         # client-side connect helper

  server/
    TCPServer.*            # epoll-based TCP server
    TCPListener.*          # listening socket wrapper

  lib/
    Epoll.*                # epoll wrapper
    EventFD.*              # eventfd wrapper
    IOHandler.*            # read/write/message buffering
    TCPDataParser.*        # length-prefixed framing
    Socket.*               # socket wrapper
    UniqueFD.h             # RAII file descriptor
    ITCPHandler.h          # callback interface
    Logger.*               # thread-safe console logging
```

## Protocol

Messages are encoded as:

```text
[4-byte big-endian payload size][payload bytes]
```

The current maximum message size is:

```text
64 KiB
```

## Notes

This is a learning-oriented networking project. The code intentionally keeps the core abstractions visible: sockets, file descriptors, `epoll`, buffering, and callbacks are not hidden behind a large framework.
