#include "win/log.hpp"

#include <windows.h>

#include <io.h>
#include <share.h>

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
int g_slot = 0;
uint64_t g_written = 0;  // bytes in the file now
int g_parts = 0;         // how many times it reached logfiles::kCap

const char* nameOf(const std::string& path) {
    const size_t slash = path.find_last_of('\\');
    return path.c_str() + (slash == std::string::npos ? 0 : slash + 1);
}

// The text of an open log (read from its start) into a file of its own
void copyOut(FILE* f, const std::string& to) {
    FILE* out = std::fopen(to.c_str(), "wb");
    if (!out) return;
    std::fseek(f, 0, SEEK_SET);
    char chunk[16 << 10];
    for (size_t n; (n = std::fread(chunk, 1, sizeof chunk, f)) > 0;) std::fwrite(chunk, 1, n, out);
    std::fclose(out);
}

// Emptied in place: the file stays open, no other game takes it meanwhile
void empty(FILE* f) {
    _chsize(_fileno(f), 0);
    std::fseek(f, 0, SEEK_SET);
}

// 2.8: at logfiles::kCap the log so far goes aside as .1.log once - the start of the game, what was set up - and
// later the middle is dropped; the file goes on from empty with a line that says so.
void nextPart(const SYSTEMTIME& t) {
    const std::string part = logfiles::part(g_path);
    if (g_parts++ == 0) copyOut(g_file, part);
    empty(g_file);
    g_written = 0;
    const int n = std::fprintf(g_file, "%02d:%02d:%02d.%03d INFO  log continued: the start of this game's log is in "
                               "%s%s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, nameOf(part),
                               g_parts > 1 ? ", its middle left out" : "");
    if (n > 0) g_written += uint64_t(n);
}
}  // namespace

FILE* openFree(const std::string& path, bool keepPrevious, int firstSlot, std::string& opened, int& slot) {
    const int first = firstSlot >= 1 && firstSlot <= logfiles::kSlots ? firstSlot : 1;
    for (int i = 0; i < logfiles::kSlots; ++i) {
        const int s = (first - 1 + i) % logfiles::kSlots + 1;  // the first asked, on from it, then from 1
        const std::string p = logfiles::slotted(path, s);
        // made when missing, its text kept for now; no writing shared - a log a running game writes refuses this,
        // as the first game's did not with fopen "w": the second emptied it and both wrote into it (3.0.3-beta.1)
        FILE* f = _fsopen(p.c_str(), "a+", _SH_DENYWR);
        if (!f) continue;
        if (keepPrevious && _filelength(_fileno(f)) > 0) copyOut(f, logfiles::previous(p));
        empty(f);
        opened = p;
        slot = s;
        return f;
    }
    return nullptr;
}

int open(const std::string& path, LogLevel level, int firstSlot) {
    if (g_file) return g_slot;
    InitializeCriticalSection(&g_lock);
    g_level = level;
    // 2.8: uaro-graphics\logs, made here; the previous game's log kept beside it (.prev.log) for a report sent after
    // the next start
    const std::string folder = logfiles::folderOf(path);
    if (!folder.empty()) {
        if (!logfiles::folderOf(folder).empty()) CreateDirectoryA(logfiles::folderOf(folder).c_str(), nullptr);
        CreateDirectoryA(folder.c_str(), nullptr);
    }
    g_file = openFree(path, true, firstSlot, g_path, g_slot);
    return g_slot;
}

const std::string& path() { return g_path; }

int slot() { return g_slot; }

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
