#include "win/log.hpp"

#include <windows.h>

#include <cstdarg>
#include <cstdio>

namespace uagfx::wlog {
namespace {
FILE* g_file = nullptr;
LogLevel g_level = LogLevel::Info;
CRITICAL_SECTION g_lock;
}  // namespace

void open(const std::string& path, LogLevel level) {
    if (g_file) return;
    InitializeCriticalSection(&g_lock);
    g_level = level;
    g_file = std::fopen(path.c_str(), "w");
}

bool enabled(LogLevel level) { return g_file && int(level) <= int(g_level); }

void line(LogLevel level, const char* fmt, ...) {
    if (!enabled(level)) return;
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    SYSTEMTIME t;
    GetLocalTime(&t);
    static const char* const kNames[] = {"ERROR", "INFO", "DEBUG"};
    EnterCriticalSection(&g_lock);
    std::fprintf(g_file, "%02d:%02d:%02d.%03d %-5s %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
                 kNames[int(level)], msg);
    std::fflush(g_file);
    LeaveCriticalSection(&g_lock);
}

}  // namespace uagfx::wlog
