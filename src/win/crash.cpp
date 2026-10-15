#include "win/crash.hpp"

#include <windows.h>

#include <dbghelp.h>

#include "win/log.hpp"

namespace uagfx::win {
namespace {

LPTOP_LEVEL_EXCEPTION_FILTER g_previous = nullptr;
char g_dump[MAX_PATH] = {};
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
            if (ok)
                LOG_ERROR("crash: a minidump is in uaro-graphics\\crash.dmp");
            else
                DeleteFileA(g_dump);
        }
    }
    return g_previous ? g_previous(e) : EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

void installCrashHandler(const std::string& gameDir) {
    static bool installed = false;
    if (installed) return;
    installed = true;
    const std::string folder = gameDir + "\\uaro-graphics";
    CreateDirectoryA(folder.c_str(), nullptr);
    lstrcpynA(g_dump, (folder + "\\crash.dmp").c_str(), MAX_PATH);
    g_previous = SetUnhandledExceptionFilter(onCrash);
}

}  // namespace uagfx::win
