#include "win/roext.hpp"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/layout.hpp"
#include "core/roext.hpp"
#include "core/settings/input_module.hpp"
#include "win/iat.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::win {
namespace {

using CreateFn = HWND(WINAPI*)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
using PeekFn = BOOL(WINAPI*)(LPMSG, HWND, UINT, UINT, UINT);
using MbFn = int(WINAPI*)(UINT, DWORD, LPCCH, int, LPWSTR, int);
using WideFn = int(WINAPI*)(UINT, DWORD, LPCWCH, int, LPSTR, int, LPCCH, LPBOOL);
using NextFn = LPSTR(WINAPI*)(WORD, LPCSTR, DWORD);
using PrevFn = LPSTR(WINAPI*)(WORD, LPCSTR, LPCSTR, DWORD);
using GetClipFn = HANDLE(WINAPI*)(UINT);
using SetClipFn = HANDLE(WINAPI*)(UINT, HANDLE);

roext::Settings g_s;
CreateFn g_create = nullptr;
PeekFn g_peek = nullptr;
MbFn g_mb = nullptr;
WideFn g_wide = nullptr;
NextFn g_next = nullptr;
PrevFn g_prev = nullptr;
GetClipFn g_getClip = nullptr;
SetClipFn g_setClip = nullptr;
DWORD g_thread = 0;          // the one that made the game's window: its messages are changed
HWND g_window = nullptr;
HGLOBAL g_clipText = nullptr;  // the clipboard's text in the game's code page, the game's to read
std::vector<std::string> g_lines;  // for the log, which opens at the driver's start

// Whether the game's folder holds ROExt of its own (dinput.dll or dinput8.dll, told by its bytes): it does its work
bool roextFile(const std::string& path) {
    std::string bytes;
    return readTextFile(path, bytes) &&
           settings::identifyModule(path.substr(path.find_last_of('\\') + 1),
                                    std::vector<uint8_t>(bytes.begin(), bytes.end()))
                   .kind == settings::ModuleKind::Roext;
}

HWND WINAPI createWindow(DWORD ex, LPCSTR cls, LPCSTR name, DWORD style, int x, int y, int w, int h, HWND parent,
                         HMENU menu, HINSTANCE inst, LPVOID param) {
    if (!roext::isGameWindow(ex, style, parent != nullptr, menu != nullptr, param != nullptr))
        return g_create(ex, cls, name, style, x, y, w, h, parent, menu, inst, param);
    g_thread = GetCurrentThreadId();
    RECT work{};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
    RECT frame{};
    AdjustWindowRect(&frame, roext::styleOf(g_s), FALSE);
    const int area[4] = {work.left, work.top, work.right, work.bottom};
    const roext::WindowPlan p =
        roext::windowPlan(g_s, area, frame.right - frame.left, frame.bottom - frame.top);
    if (p.place) {
        x = p.x;
        y = p.y;
        w = p.width;
        h = p.height;
    }
    g_window = g_create(p.exStyle, cls, name, p.style, x, y, w, h, nullptr, nullptr, inst, nullptr);
    char line[160];
    std::snprintf(line, sizeof line, "roext: the game's window %s%s, %dx%d at %d,%d",
                  g_s.windowLock ? "borderless" : "with its frame", g_s.windowOnTop ? ", above the others" : "", w, h,
                  x, y);
    g_lines.push_back(line);
    LOG_INFO("%s", line);  // the log open by now: there; else at the driver's start
    return g_window;
}

// ROExt with code page 65001: a typed character (the keyboard's code page) as its UTF-8 bytes in wParam
void utf8Char(MSG& m) {
    wchar_t number[16] = {};
    UINT cp = CP_THREAD_ACP;
    if (GetLocaleInfoW(LOWORD(HandleToUlong(GetKeyboardLayout(0))), LOCALE_IDEFAULTANSICODEPAGE, number, 16)) {
        cp = 0;
        for (const wchar_t* c = number; *c >= L'0' && *c <= L'9'; ++c) cp = cp * 10 + UINT(*c - L'0');
    }
    char in[4] = {};
    std::memcpy(in, &m.wParam, 4);
    int n = 0;
    while (n < 4 && in[n]) ++n;
    wchar_t w[16] = {};
    const int k = MultiByteToWideChar(cp, 0, in, n, w, 15);
    w[k > 0 ? k : 0] = 0;
    char out[4] = {};
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out, 4, nullptr, nullptr);
    uint32_t packed = 0;
    std::memcpy(&packed, out, 4);
    m.wParam = packed;
}

BOOL WINAPI peekMessage(LPMSG m, HWND hwnd, UINT lo, UINT hi, UINT remove) {
    if (!m || GetCurrentThreadId() != g_thread) return g_peek(m, hwnd, lo, hi, remove);
    for (;;) {
        if (!g_peek(m, hwnd, lo, hi, remove)) {
            // ROExt's AutoFreeCPU: behind another window with nothing to do, wait for input a while
            if (g_s.freeCpu > 0 && g_window && GetForegroundWindow() != g_window)
                MsgWaitForMultipleObjectsEx(0, nullptr, DWORD(g_s.freeCpu), 0x1C07, 0);  // QS_INPUT
            return FALSE;
        }
        if (m->message == WM_CHAR && g_s.codePage == CP_UTF8) {
            utf8Char(*m);
            return TRUE;
        }
        roext::Message x{m->message, uint32_t(m->wParam), int32_t(m->lParam)};
        if (roext::transform(x, g_s, GetKeyState(VK_CONTROL) < 0)) {
            m->message = x.message;
            m->wParam = x.wParam;
            m->lParam = x.lParam;
            return TRUE;
        }
        if (!(remove & PM_REMOVE)) g_peek(m, hwnd, lo, hi, remove | PM_REMOVE);  // dropped: out of the queue too
    }
}

// The exe's text in the code page set (UTF-8 takes no flags)
int WINAPI mbToWide(UINT, DWORD flags, LPCCH s, int n, LPWSTR w, int wn) {
    const UINT cp = UINT(g_s.codePage);
    return g_mb(cp, cp == CP_UTF8 ? 0 : flags, s, n, w, wn);
}

int WINAPI wideToMb(UINT, DWORD flags, LPCWCH w, int wn, LPSTR s, int n, LPCCH def, LPBOOL used) {
    const UINT cp = UINT(g_s.codePage);
    return g_wide(cp, cp == CP_UTF8 ? 0 : flags, w, wn, s, n, def, used);
}

LPSTR WINAPI charNext(WORD, LPCSTR cur, DWORD flags) {
    if (g_s.codePage != CP_UTF8) return g_next(WORD(g_s.codePage), cur, flags);
    if (!cur || !*cur) return const_cast<LPSTR>(cur);
    const auto lead = static_cast<unsigned char>(*cur++);
    int more = (lead & 0xC0) != 0xC0 ? 0 : !(lead & 0x20) ? 1 : (lead & 0x10) ? 3 : 2;
    while (more-- > 0 && (static_cast<unsigned char>(*cur) & 0xC0) == 0x80) ++cur;
    return const_cast<LPSTR>(cur);
}

LPSTR WINAPI charPrev(WORD, LPCSTR start, LPCSTR cur, DWORD flags) {
    if (g_s.codePage != CP_UTF8) return g_prev(WORD(g_s.codePage), start, cur, flags);
    if (!cur) return nullptr;
    while (cur > start && (static_cast<unsigned char>(*--cur) & 0xC0) == 0x80) {
    }
    return const_cast<LPSTR>(cur);
}

// The game reads text from the clipboard: Windows' Unicode in its code page
HANDLE WINAPI getClipboard(UINT format) {
    if (format != CF_TEXT || GetCurrentThreadId() != g_thread) return g_getClip(format);
    HANDLE wide = g_getClip(CF_UNICODETEXT);
    const auto* w = wide ? static_cast<const wchar_t*>(GlobalLock(wide)) : nullptr;
    if (!w) return g_getClip(format);
    const UINT cp = UINT(g_s.codePage);
    const int n = WideCharToMultiByte(cp, 0, w, -1, nullptr, 0, nullptr, nullptr);
    HGLOBAL made = n > 0 ? (g_clipText ? GlobalReAlloc(g_clipText, SIZE_T(n), GMEM_MOVEABLE)
                                        : GlobalAlloc(GMEM_MOVEABLE, SIZE_T(n)))
                         : nullptr;
    char* text = made ? static_cast<char*>(GlobalLock(made)) : nullptr;
    if (made) g_clipText = made;
    if (text) {
        WideCharToMultiByte(cp, 0, w, -1, text, n, nullptr, nullptr);
        GlobalUnlock(made);
    }
    GlobalUnlock(wide);
    return text ? made : g_getClip(format);
}

// The game puts text on the clipboard: as Unicode from its code page
HANDLE WINAPI setClipboard(UINT format, HANDLE data) {
    const char* text = format == CF_TEXT && data ? static_cast<const char*>(GlobalLock(data)) : nullptr;
    if (!text) return g_setClip(format, data);
    const UINT cp = UINT(g_s.codePage);
    const int n = MultiByteToWideChar(cp, 0, text, -1, nullptr, 0);
    HGLOBAL made = n > 0 ? GlobalAlloc(GMEM_MOVEABLE, SIZE_T(n) * 2) : nullptr;
    auto* w = made ? static_cast<wchar_t*>(GlobalLock(made)) : nullptr;
    if (w) {
        MultiByteToWideChar(cp, 0, text, -1, w, n);
        GlobalUnlock(made);
        if (HANDLE put = g_setClip(CF_UNICODETEXT, made)) {
            GlobalUnlock(data);
            GlobalFree(data);
            return put;
        }
    }
    if (made) GlobalFree(made);
    GlobalUnlock(data);
    return g_setClip(format, data);
}

}  // namespace

void installRoext() {
    // the game alone (it imports DINPUT.dll; ROExt, loaded through DirectInput, never ran in setup.exe)
    if (!GetModuleHandleA("dinput.dll")) return;
    std::string text;
    readTextFile(ourFile(layout::File::Ini), text);
    const Config c = parseConfig(text);
    const std::string game = gameDir();
    if (!c.clientRoext || !roextFile(layout::join(layout::join(game, layout::kOriginals), "dinput8.dll"))) return;
    for (const char* own : {"dinput.dll", "dinput8.dll"})
        if (roextFile(layout::join(game, own))) {
            g_lines.push_back(std::string("roext: ROExt's own ") + own + " is in the game's folder - it does its work");
            return;
        }
    g_s = c.roext;
    bool ok = hookImport("user32.dll", "CreateWindowExA", &createWindow, g_create);
    ok = hookImport("user32.dll", "PeekMessageA", &peekMessage, g_peek) && ok;
    if (g_s.codePage != -1) {
        ok = hookImport("kernel32.dll", "MultiByteToWideChar", &mbToWide, g_mb) && ok;
        ok = hookImport("kernel32.dll", "WideCharToMultiByte", &wideToMb, g_wide) && ok;
        ok = hookImport("user32.dll", "CharNextExA", &charNext, g_next) && ok;
        ok = hookImport("user32.dll", "CharPrevExA", &charPrev, g_prev) && ok;
        ok = hookImport("user32.dll", "GetClipboardData", &getClipboard, g_getClip) && ok;
        ok = hookImport("user32.dll", "SetClipboardData", &setClipboard, g_setClip) && ok;
    }
    char line[300];
    std::snprintf(line, sizeof line,
                  "roext: ROExt's work in the driver (its dinput8.dll aside, [roext]): window %s%s, mouse buttons %s, "
                  "Alt+F4 %s, Ctrl+W %s, %d ms behind other windows, code page %d%s",
                  g_s.windowLock ? "locked" : "framed", g_s.windowOnTop ? " on top" : "",
                  g_s.mouseButtons ? "F24-F22" : "as they are", g_s.altF4 ? "as F4" : "closes",
                  g_s.minimizeKey ? "minimizes" : "as it is", g_s.freeCpu, g_s.codePage,
                  ok ? "" : " - not every import found");
    g_lines.insert(g_lines.begin(), line);
}

void logRoext() {
    for (const std::string& line : g_lines) LOG_INFO("%s", line.c_str());
}

}  // namespace uagfx::win
