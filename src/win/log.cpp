#include "win/log.hpp"

#include <windows.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>

#include "core/log_files.hpp"

namespace uagfx::wlog {
namespace {
FILE* g_file = nullptr;
std::atomic<void (*)(LogLevel, const char*)> g_sink{nullptr};  // other threads log too (Streamline's)
LogLevel g_level = LogLevel::Info;
CRITICAL_SECTION g_lock;
std::string g_path;
uint64_t g_written = 0;  // bytes in the file now
int g_parts = 0;         // how many times it reached logfiles::kCap

const char* nameOf(const std::string& path) {
    const size_t slash = path.find_last_of('\\');
    return path.c_str() + (slash == std::string::npos ? 0 : slash + 1);
}

// 2.8: at logfiles::kCap the log so far goes aside as .1.log once - the start of the game, what was set up - and
// later the middle is dropped; the file goes on from empty with a line that says so.
void nextPart(const SYSTEMTIME& t) {
    std::fclose(g_file);
    const std::string part = logfiles::part(g_path);
    if (g_parts++ == 0) MoveFileExA(g_path.c_str(), part.c_str(), MOVEFILE_REPLACE_EXISTING);
    g_file = std::fopen(g_path.c_str(), "w");
    g_written = 0;
    if (!g_file) return;
    const int n = std::fprintf(g_file, "%02d:%02d:%02d.%03d INFO  log continued: the start of this game's log is in "
                               "%s%s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, nameOf(part),
                               g_parts > 1 ? ", its middle left out" : "");
    if (n > 0) g_written += uint64_t(n);
}
}  // namespace

void open(const std::string& path, LogLevel level) {
    if (g_file) return;
    InitializeCriticalSection(&g_lock);
    g_level = level;
    g_path = path;
    // 2.8: uaro-graphics\logs, made here; the previous game's log kept beside it (.prev.log) for a report sent after
    // the next start
    const std::string folder = logfiles::folderOf(path);
    if (!folder.empty()) {
        if (!logfiles::folderOf(folder).empty()) CreateDirectoryA(logfiles::folderOf(folder).c_str(), nullptr);
        CreateDirectoryA(folder.c_str(), nullptr);
    }
    MoveFileExA(path.c_str(), logfiles::previous(path).c_str(), MOVEFILE_REPLACE_EXISTING);
    g_file = std::fopen(path.c_str(), "w");
}

bool enabled(LogLevel level) { return g_file && int(level) <= int(g_level); }

void setLevel(LogLevel level) {
    if (!g_file) return;
    EnterCriticalSection(&g_lock);
    g_level = level;
    LeaveCriticalSection(&g_lock);
}

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
    char text[1100];
    const int n = std::snprintf(text, sizeof text, "%02d:%02d:%02d.%03d %-5s %s\n", t.wHour, t.wMinute, t.wSecond,
                                t.wMilliseconds, kNames[int(level)], msg);
    EnterCriticalSection(&g_lock);
    if (n > 0 && g_written + uint64_t(n) > logfiles::kCap) nextPart(t);
    if (g_file && n > 0) {
        std::fputs(text, g_file);
        std::fflush(g_file);
        g_written += uint64_t(n);
    }
    LeaveCriticalSection(&g_lock);
    if (const auto sink = g_sink.load()) sink(level, msg);  // read once: it may go meanwhile (0.10 review 10)
}

void setSink(void (*sink)(LogLevel level, const char* text)) { g_sink = sink; }

}  // namespace uagfx::wlog
