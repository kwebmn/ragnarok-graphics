#pragma once
#include <string>

#include "pad/core/config/config.hpp"

namespace uapad::wlog {

void open(const std::string& path, LogLevel level);
bool enabled(LogLevel level);
void line(LogLevel level, const char* fmt, ...);

}  // namespace uapad::wlog

#define LOG_ERROR(...) ::uapad::wlog::line(::uapad::LogLevel::Error, __VA_ARGS__)
#define LOG_INFO(...) ::uapad::wlog::line(::uapad::LogLevel::Info, __VA_ARGS__)
#define LOG_DEBUG(...) ::uapad::wlog::line(::uapad::LogLevel::Debug, __VA_ARGS__)
