#pragma once
#include <string_view>

namespace cppnet
{
void LogInfo(std::string_view message);

void LogError(std::string_view message, int error = 0);
} // namespace cppnet
