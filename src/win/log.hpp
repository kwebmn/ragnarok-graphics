#pragma once
#include <string>

#include "core/config.hpp"

namespace uagfx::wlog {

// Opens the log once; when the file cannot be created the driver keeps working without a log.
void open(const std::string& path, LogLevel level);
bool enabled(LogLevel level);
// [debug] log changed while the game runs (stage 2 spec §3.4).
void setLevel(LogLevel level);
// 2.6.20: checked as printf by the compiler - a format with more places than arguments (the effects' line kept the
// removed depth of field's three) read past them and took uaro-render64.exe down at the login (R-FS6S3K)
void line(LogLevel level, const char* fmt, ...) __attribute__((format(gnu_printf, 2, 3)));
// Part 3: uaro-render64.exe also hands every line it writes to the driver, which writes it into the game's log.
void setSink(void (*sink)(LogLevel level, const char* text));

}  // namespace uagfx::wlog

#define LOG_ERROR(...) ::uagfx::wlog::line(::uagfx::LogLevel::Error, __VA_ARGS__)
#define LOG_INFO(...) ::uagfx::wlog::line(::uagfx::LogLevel::Info, __VA_ARGS__)
#define LOG_DEBUG(...) ::uagfx::wlog::line(::uagfx::LogLevel::Debug, __VA_ARGS__)
