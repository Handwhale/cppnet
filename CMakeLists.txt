cmake_minimum_required(VERSION 3.10.0)
project(TCPTest VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
function(enable_project_warnings target_name)
    target_compile_options(${target_name} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Woverloaded-virtual
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
    )

    if (CPPNET_WARNINGS_AS_ERRORS)
        target_compile_options(${target_name} PRIVATE -Werror)
    endif()
endfunction()

add_library(cppnet_lib
    src/lib/MessageProcessor.cpp
    src/lib/Socket.cpp
    src/lib/TCPConnection.cpp
    src/lib/TCPDataParser.cpp
    src/lib/IOHandler.cpp
    src/lib/Epoll.cpp
)

target_include_directories(cppnet_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(cppnet_server 
    src/app/cppnet_server.cpp
    src/server/TCPServer.cpp
    src/server/TCPListener.cpp
)

target_include_directories(cppnet_server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(cppnet_server
    PRIVATE cppnet_lib
)

add_executable(cppnet_client
    src/app/cppnet_client.cpp
    src/client/TCPConnector.cpp
    src/client/TCPClient.cpp
)

target_include_directories(cppnet_client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(cppnet_client
    PRIVATE cppnet_lib
)
enable_project_warnings(cppnet_lib)
enable_project_warnings(cppnet_server)
enable_project_warnings(cppnet_client)

