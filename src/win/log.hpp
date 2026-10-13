#pragma once
#include <string>

#include "core/config.hpp"

namespace uagfx::wlog {

// Opens the log once; when the file cannot be created the driver keeps working without a log.
void open(const std::string& path, LogLevel level);
bool enabled(LogLevel level);
// [debug] log changed while the game runs (stage 2 spec §3.4).
void setLevel(LogLevel level);
void line(LogLevel level, const char* fmt, ...);

}  // namespace uagfx::wlog

#define LOG_ERROR(...) ::uagfx::wlog::line(::uagfx::LogLevel::Error, __VA_ARGS__)
#define LOG_INFO(...) ::uagfx::wlog::line(::uagfx::LogLevel::Info, __VA_ARGS__)
#define LOG_DEBUG(...) ::uagfx::wlog::line(::uagfx::LogLevel::Debug, __VA_ARGS__)
