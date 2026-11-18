#include "win/crash.hpp"

#include <windows.h>

#include <dbghelp.h>

#include <cstdlib>
#include <exception>
#include <string>

#include "core/exception_words.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

LPTOP_LEVEL_EXCEPTION_FILTER g_previous = nullptr;
std::terminate_handler g_previousTerminate = nullptr;
char g_dump[MAX_PATH] = {};
volatile bool g_crashDumped = false;  // 3.0.18-beta.145: a crash's dump written this run
LONG g_once = 0;

LONG WINAPI onCrash(EXCEPTION_POINTERS* e) {
    if (InterlockedExchange(&g_once, 1) == 0 && e && e->ExceptionRecord) {
        const EXCEPTION_RECORD* r = e->ExceptionRecord;
        HMODULE module = nullptr;
        char name[MAX_PATH] = "?";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               static_cast<LPCSTR>(r->ExceptionAddress), &module) &&
            module) {
            char path[MAX_PATH];
            const DWORD n = GetModuleFileNameA(module, path, MAX_PATH);
            const char* slash = path;
            for (DWORD i = 0; i < n; ++i)
                if (path[i] == '\\' || path[i] == '/') slash = path + i + 1;
            lstrcpynA(name, n ? slash : "?", MAX_PATH);
        }
        const unsigned long offset = static_cast<unsigned long>(reinterpret_cast<uintptr_t>(r->ExceptionAddress) -
                                                                reinterpret_cast<uintptr_t>(module));
        LOG_ERROR("crash: exception 0x%08lx at %s+0x%lx (thread %lu)", static_cast<unsigned long>(r->ExceptionCode),
                  name, module ? offset : 0ul, GetCurrentThreadId());
        if (r->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r->NumberParameters >= 2)  // 3.0.18-beta.145
            LOG_ERROR("crash: %s at 0x%08lx", r->ExceptionInformation[0] == 1   ? "a write"
                                                : r->ExceptionInformation[0] == 8 ? "an execution"
                                                                                  : "a read",
                      static_cast<unsigned long>(r->ExceptionInformation[1]));
        using WriteFn = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                                      PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
        HMODULE help = LoadLibraryA("dbghelp.dll");
        const auto write = help ? reinterpret_cast<WriteFn>(
                                      reinterpret_cast<void*>(GetProcAddress(help, "MiniDumpWriteDump")))
                                : nullptr;
        HANDLE file = write ? CreateFileA(g_dump, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                          nullptr)
                            : INVALID_HANDLE_VALUE;
        if (file != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION info{};
            info.ThreadId = GetCurrentThreadId();
            info.ExceptionPointers = e;
            info.ClientPointers = FALSE;
            const BOOL ok = write(GetCurrentProcess(), GetCurrentProcessId(), file,
                                  MINIDUMP_TYPE(MiniDumpNormal | MiniDumpWithThreadInfo), &info, nullptr, nullptr);
            CloseHandle(file);
            if (ok) g_crashDumped = true;  // 3.0.18-beta.145: a hang after it keeps it (the cause is in it)
            if (ok)
                LOG_ERROR("crash: a minidump is in %s", g_dump);
            else
                DeleteFileA(g_dump);
        }
    }
    return g_previous ? g_previous(e) : EXCEPTION_CONTINUE_SEARCH;
}

// 3.0.21-beta.14 (R-DP7PVN: a game ended a second after a picture's load and its log said nothing - no exception
// for onCrash, its DLLs told of the exit): a C++ exception out of the driver's code meets the game's own frames, which
// have no unwinding for it, and std::terminate ends the game (msvcrt's abort: «Runtime Error!», exit code 3). Now the
// log says what left and a dump is written; the end is as before.
[[noreturn]] void onTerminate() {
    static LONG once = 0;
    if (InterlockedExchange(&once, 1) == 0) {
        const std::string what = exceptionWords(std::current_exception());
        LOG_ERROR("crash: a C++ exception left the driver's code - %s (thread %lu); the game ends", what.c_str(),
                  GetCurrentThreadId());
        if (writeHangDump()) LOG_ERROR("crash: a minidump is in %s", g_dump);
    }
    if (g_previousTerminate) g_previousTerminate();
    std::abort();
}

}  // namespace

using WriteDumpFn = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                                  PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
static WriteDumpFn g_writeDump = nullptr;  // 3.0.22-beta.8: found ahead (prepareHangDump)

static WriteDumpFn writeDumpFn() {
    if (g_writeDump) return g_writeDump;
    HMODULE help = LoadLibraryA("dbghelp.dll");
    void* found = help ? reinterpret_cast<void*>(GetProcAddress(help, "MiniDumpWriteDump")) : nullptr;
    g_writeDump = reinterpret_cast<WriteDumpFn>(found);
    return g_writeDump;
}

bool prepareHangDump() { return writeDumpFn() != nullptr; }

bool writeHangDump() {
    if (!g_dump[0] || g_crashDumped) return false;  // 3.0.18-beta.145: the crash's dump kept
    const WriteDumpFn write = writeDumpFn();
    if (!write) return false;
    HANDLE file = CreateFileA(g_dump, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    const BOOL ok = write(GetCurrentProcess(), GetCurrentProcessId(), file,
                          MINIDUMP_TYPE(MiniDumpNormal | MiniDumpWithThreadInfo), nullptr, nullptr, nullptr);
    CloseHandle(file);
    if (!ok) DeleteFileA(g_dump);
    return ok != FALSE;
}

void installCrashHandler(const std::string& gameDir, const char* dumpName) {
    static bool installed = false;
    if (installed) return;
    installed = true;
    const std::string folder = gameDir + "\\uaro-graphics";
    CreateDirectoryA(folder.c_str(), nullptr);
    lstrcpynA(g_dump, (folder + "\\" + dumpName).c_str(), MAX_PATH);
    g_previous = SetUnhandledExceptionFilter(onCrash);
    g_previousTerminate = std::set_terminate(onTerminate);  // 3.0.21-beta.14
}

}  // namespace uagfx::win
