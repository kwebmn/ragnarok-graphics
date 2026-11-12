#include "win/start_watch.hpp"

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "core/layout.hpp"
#include "core/start_watch.hpp"
#include "win/crash.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::win::startwatch {
namespace {

namespace sw = uagfx::startwatch;

constexpr DWORD kWaitMs = 25000;        // the game makes its window in a few seconds
constexpr DWORD kLaterMs = 10 * 60000;  // its DirectDraw made after all: told within ten minutes
constexpr int kSamples = 5;
constexpr size_t kWords = 1024;  // 4 KB of the stack

HANDLE g_main = nullptr;  // the game's main thread
DWORD g_mainId = 0;
HANDLE g_passed = nullptr;
ULONGLONG g_since = 0;
uint32_t g_stack[kWords];

// Our own memory: a place not mapped fails instead of faulting
bool readSelf(uint32_t address, void* out, size_t size) {
    SIZE_T got = 0;
    return address &&
           ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(address)), out, size, &got) &&
           got == size;
}

std::string utf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size_t(n > 0 ? n : 0), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), int(w.size()), &out[0], n, nullptr, nullptr);
    return out;
}

// The process's modules from its loader's list, read without the loader's lock (the main thread may hold it): its
// PEB's Ldr, InLoadOrderModuleList - each entry's DllBase +0x18, SizeOfImage +0x20, FullDllName +0x24 (32-bit)
std::vector<sw::Module> modules(std::vector<std::string>& paths) {
    std::vector<sw::Module> out;
    uint32_t peb = 0, ldr = 0, link = 0;
    asm volatile("movl %%fs:0x30, %0" : "=r"(peb));  // the TEB's PEB (NtCurrentTeb's intrinsic warns in GCC)
    if (!readSelf(peb + 0x0C, &ldr, 4)) return out;
    const uint32_t head = ldr + 0x0C;
    if (!readSelf(head, &link, 4)) return out;
    for (int n = 0; link && link != head && n < 512; ++n) {
        uint32_t base = 0, size = 0, buffer = 0;
        uint16_t bytes = 0;
        if (!readSelf(link + 0x18, &base, 4) || !readSelf(link + 0x20, &size, 4) || !readSelf(link + 0x24, &bytes, 2) ||
            !readSelf(link + 0x28, &buffer, 4))
            break;
        std::wstring w(bytes / 2, L'\0');
        if (bytes && buffer && !readSelf(buffer, &w[0], bytes)) w.clear();
        const std::string path = utf8(w);
        const size_t slash = path.find_last_of('\\');
        out.push_back(sw::Module{slash == std::string::npos ? path : path.substr(slash + 1), base, size});
        paths.push_back(path);
        if (!readSelf(link, &link, 4)) break;
    }
    return out;
}

// A return address: right after a call (E8 rel32; FF /2 by register, memory or [disp32]) - as win/profiler's
bool afterCall(uint32_t ret) {
    uint8_t b[8];  // b[7] = ret[-1]
    return readSelf(ret - 8, b, sizeof b) &&
           (b[3] == 0xE8 || (b[6] == 0xFF && (b[7] & 0x38) == 0x10) || (b[5] == 0xFF && (b[6] & 0x38) == 0x10) ||
            (b[2] == 0xFF && (b[3] & 0x38) == 0x10));
}

// The main thread stopped only for these reads - no lock, no allocation (it may hold the heap's or the loader's)
bool sample(uint32_t& at, size_t& words) {
    CONTEXT c{};
    c.ContextFlags = CONTEXT_CONTROL;
    bool got = false;
    words = 0;
    if (SuspendThread(g_main) == DWORD(-1)) return false;
    if (GetThreadContext(g_main, &c)) {
        MEMORY_BASIC_INFORMATION mbi{};
        const auto* sp = reinterpret_cast<const uint8_t*>(uintptr_t(c.Esp));
        if (VirtualQuery(sp, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT) {
            const auto* end = static_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
            words = std::min(kWords, size_t(end - sp) / 4);
            std::memcpy(g_stack, sp, words * 4);
        }
        at = uint32_t(c.Eip);
        got = true;
    }
    ResumeThread(g_main);
    return got;
}

std::string windowText(HWND w) {
    wchar_t text[512] = {};
    GetWindowTextW(w, text, 511);
    return utf8(text);
}

BOOL CALLBACK staticText(HWND w, LPARAM out) {
    char cls[64] = {};
    GetClassNameA(w, cls, sizeof cls);
    const std::string text = windowText(w);
    auto& words = *reinterpret_cast<std::string*>(out);
    if (lstrcmpiA(cls, "Static") == 0 && !text.empty() && words.size() < 600)
        words += (words.empty() ? "" : " | ") + text;
    return TRUE;
}

// The main thread's windows: its class, title, shown or not, where - a message box (#32770) brought to the front, onto
// the main screen when it stands on none
BOOL CALLBACK tellWindow(HWND w, LPARAM) {
    char cls[64] = {};
    GetClassNameA(w, cls, sizeof cls);
    RECT r{};
    GetWindowRect(w, &r);
    const bool dialog = lstrcmpA(cls, "#32770") == 0;
    std::string words;
    if (dialog) EnumChildWindows(w, staticText, reinterpret_cast<LPARAM>(&words));
    const bool nowhere = MonitorFromWindow(w, MONITOR_DEFAULTTONULL) == nullptr;
    LOG_INFO("start watch: the main thread's window «%s» «%s», shown %d, at %ld, %ld %ldx%ld%s%s%s", cls,
             windowText(w).c_str(), IsWindowVisible(w) ? 1 : 0, r.left, r.top, r.right - r.left, r.bottom - r.top,
             nowhere ? " (on no screen)" : "", words.empty() ? "" : " - its words: ", words.c_str());
    if (dialog) {
        if (nowhere) {
            const int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
            SetWindowPos(w, nullptr, (sw - (r.right - r.left)) / 2, (sh - (r.bottom - r.top)) / 2, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER);
        }
        ShowWindow(w, SW_SHOWNORMAL);
        SetWindowPos(w, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetForegroundWindow(w);
        LOG_INFO("start watch: that message box brought to the front for the player");
    }
    return TRUE;
}

void tell(const std::vector<sw::Module>& mods, const std::vector<std::string>& paths) {
    const ULONGLONG s = (GetTickCount64() - g_since) / 1000;
    LOG_ERROR("start watch: the game has not made its DirectDraw (its window) %llu s after it started - where it "
              "stands",
              static_cast<unsigned long long>(s));
    LOG_INFO("start watch: its command line: %s", GetCommandLineA());
    for (int i = 0; i < kSamples; ++i) {
        uint32_t at = 0;
        size_t words = 0;
        if (sample(at, words))
            LOG_INFO("start watch: the main thread (%d of %d): %s", i + 1, kSamples,
                     sw::where(mods, at, g_stack, words, afterCall).c_str());
        Sleep(200);
    }
    for (size_t i = 0; i < mods.size(); ++i)
        LOG_INFO("start watch: module %s at 0x%08X, 0x%X bytes - %s", mods[i].name.c_str(), unsigned(mods[i].base),
                 unsigned(mods[i].size), paths[i].c_str());
    EnumThreadWindows(g_mainId, tellWindow, 0);
    LOG_INFO("start watch: %s", writeHangDump() ? "a minidump of every thread is in uaro-graphics\\crash.dmp"
                                                : "no minidump (dbghelp.dll or the file)");
}

DWORD WINAPI watch(void*) {
    // made before the wait, while the loader is free (the stuck main thread may hold its lock later); the minidump
    // last - it loads dbghelp.dll, which waits for that lock: the places, the modules and the windows are told first
    const std::string dir = gameDir();
    const std::string log = layout::join(dir, layout::now(layout::File::DriverLog));
    if (WaitForSingleObject(g_passed, kWaitMs) == WAIT_OBJECT_0) return 0;
    wlog::open(log, LogLevel::Info);  // the driver's own log - its DirectDraw, if it comes, writes on in it
    installCrashHandler(dir);
    std::vector<std::string> paths;
    const std::vector<sw::Module> mods = modules(paths);
    tell(mods, paths);
    if (WaitForSingleObject(g_passed, kLaterMs) == WAIT_OBJECT_0)
        LOG_INFO("start watch: the game made its DirectDraw after all, %llu s after it started",
                 static_cast<unsigned long long>((GetTickCount64() - g_since) / 1000));
    return 0;
}

}  // namespace

void install() {
    g_since = GetTickCount64();
    g_mainId = GetCurrentThreadId();
    g_main = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, g_mainId);
    g_passed = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!g_main || !g_passed) return;
    if (HANDLE t = CreateThread(nullptr, 0, watch, nullptr, 0, nullptr)) CloseHandle(t);
}

void passed() {
    if (g_passed) SetEvent(g_passed);
}

}  // namespace uagfx::win::startwatch
