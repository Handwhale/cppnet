#include "Logger.h"
#include <iostream>
#include <mutex>

namespace cppnet
{

static std::mutex printMtx;

void LogInfo(std::string_view message)
{
    std::lock_guard lk(printMtx);
    std::cout << "[System] " << message << '\n';
}

void LogError(std::string_view message, int error)
{
    std::lock_guard lk(printMtx);
    std::cerr << "[System] " << message;

    if (error != 0)
    {
        const auto ec = std::error_code(error, std::generic_category());
        std::cerr << ": " << ec.message() << " (" << ec.value() << ")";
    }

    std::cerr << '\n';
}
} // namespace cppnet
