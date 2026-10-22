#include "app/update_window.hpp"

#include <commctrl.h>

#include <atomic>
#include <cstdio>

#include "app/text.hpp"
#include "app/ui.hpp"

namespace uagfx::app {
namespace {

using settings::Text;

const wchar_t kClass[] = L"uaroGraphicsSmall";
constexpr int kFirst = 1001, kSecond = 1002, kCheck = 1003;
constexpr UINT kStepMessage = WM_APP + 1, kEndMessage = WM_APP + 2;

struct Shared {  // between the progress window and the work's thread
    std::atomic<bool> cancel{false};
    SRWLOCK lock = SRWLOCK_INIT;
    Step step;
    bool ok = false;
    std::wstring error;
};

struct Small {  // a window's state
    int pressed = 0;  // kFirst, kSecond; 0: closed
    bool keep = true;
    Shared* shared = nullptr;  // the progress window's
    settings::Lang lang = settings::Lang::En;
    bool thenGame = false;
    HWND bar = nullptr, status = nullptr, cancel = nullptr;
};

std::wstring megabytes(uint64_t bytes, settings::Lang l) {
    char text[32];
    std::snprintf(text, sizeof text, "%.1f", double(bytes) / (1024.0 * 1024.0));
    std::string s = text;
    if (l != settings::Lang::En)
        for (char& c : s) c = c == '.' ? ',' : c;  // "5,1 з 8,0 МБ"
    return wide(s);
}

void showStep(Small& s) {
    Step step;
    AcquireSRWLockShared(&s.shared->lock);
    step = s.shared->step;
    ReleaseSRWLockShared(&s.shared->lock);
    const settings::Lang l = s.lang;
    const auto mb = [&](uint64_t v) { return utf8(megabytes(v, l)); };
    std::wstring text;
    int permille = -1;
    switch (step.what) {
        case Step::Download:
            text = wtrf(l, s.thenGame ? Text::UpdateLoadedGame : Text::UpdateLoadedApp,
                        {mb(step.done), mb(step.total)});
            break;
        case Step::Dlss: text = wtrf(l, Text::UpdateLoadingDlss, {mb(step.done), mb(step.total)}); break;
        case Step::Models: text = wtrf(l, Text::UpdateLoadingModels, {mb(step.done), mb(step.total)}); break;
        case Step::Wait: text = wtr(l, Text::UpdateWaitGame); break;
        case Step::Install: text = wtr(l, Text::UpdateInstalling); permille = 1000; break;
        case Step::Done: permille = 1000; break;
    }
    if (step.what == Step::Download || step.what == Step::Dlss || step.what == Step::Models)
        permille = step.total ? int(step.done * 1000 / step.total) : 0;
    SetWindowTextW(s.status, text.c_str());
    if (permille >= 0) SendMessageW(s.bar, PBM_SETPOS, WPARAM(permille), 0);
    EnableWindow(s.cancel, step.what != Step::Install && step.what != Step::Done && !s.shared->cancel);
}

LRESULT CALLBACK proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    Small* s = reinterpret_cast<Small*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (msg) {
        case WM_COMMAND: {
            if (!s) break;
            const int id = LOWORD(wp);
            if (id == kCheck) {
                s->keep = SendMessageW(reinterpret_cast<HWND>(lp), BM_GETCHECK, 0, 0) == BST_CHECKED;
            } else if (s->shared) {  // the progress window: Cancel, Esc
                if ((id == kSecond || id == IDCANCEL) && IsWindowEnabled(s->cancel)) {
                    s->shared->cancel = true;
                    EnableWindow(s->cancel, FALSE);
                }
            } else if (id == kFirst || id == kSecond || id == IDCANCEL) {
                s->pressed = id == IDCANCEL ? 0 : id;
                DestroyWindow(h);
            }
            return 0;
        }
        case kStepMessage:
            if (s && s->shared) showStep(*s);
            return 0;
        case kEndMessage: DestroyWindow(h); return 0;
        case WM_CLOSE:
            if (s && s->shared) {  // as Cancel: the work ends it
                SendMessageW(h, WM_COMMAND, MAKEWPARAM(kSecond, BN_CLICKED), 0);
                return 0;
            }
            DestroyWindow(h);
            return 0;
        default: break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

struct Window {
    HWND win = nullptr;
    int dpi = 96;
    HFONT font = nullptr, bold = nullptr;
    ~Window() {
        if (font) DeleteObject(font);
        if (bold) DeleteObject(bold);
    }
    int at(int v) const { return px(v, dpi); }
    HWND control(const wchar_t* cls, const std::wstring& text, DWORD style, int id, int x, int y, int w, int h,
                 bool heading = false) {
        HWND c = CreateWindowExW(0, cls, text.c_str(), WS_CHILD | WS_VISIBLE | style, at(x), at(y), at(w), at(h), win,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(heading ? bold : font), TRUE);
        return c;
    }
};

// A window of `width` x `height` (at 96 DPI) over everything, in the middle of `owner` or of the screen.
bool open(Window& w, Small& s, HWND owner, int width, int height) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof wc;
        wc.lpfnWndProc = proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
        wc.lpszClassName = kClass;
        registered = RegisterClassExW(&wc) != 0;
    }
    w.dpi = windowDpi(owner);
    w.font = makeFont(w.dpi, false);
    w.bold = makeFont(w.dpi, true, 125);
    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    const DWORD ex = WS_EX_TOPMOST | WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT;
    RECT r{0, 0, w.at(width), w.at(height)};
    AdjustWindowRectEx(&r, style, FALSE, ex);
    RECT area{};
    if (!owner || !GetWindowRect(owner, &area)) SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
    const int cx = r.right - r.left, cy = r.bottom - r.top;
    w.win = CreateWindowExW(ex, kClass, L"uaRO Graphics", style, area.left + (area.right - area.left - cx) / 2,
                            area.top + (area.bottom - area.top - cy) / 2, cx, cy, owner, nullptr,
                            GetModuleHandleW(nullptr), nullptr);
    if (!w.win) return false;
    SetWindowLongPtrW(w.win, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&s));
    return true;
}

void run(HWND win, HWND focus) {
    ShowWindow(win, SW_SHOW);
    SetForegroundWindow(win);
    if (focus) SetFocus(focus);
    MSG m;
    while (IsWindow(win)) {
        const BOOL got = GetMessageW(&m, nullptr, 0, 0);
        if (got <= 0) {
            if (got == 0) PostQuitMessage(int(m.wParam));
            break;
        }
        if (IsDialogMessageW(win, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
}

struct WorkArgs {
    Shared* shared;
    HWND win;
    const Work* work;
};

DWORD WINAPI workThread(void* p) {
    WorkArgs& a = *static_cast<WorkArgs*>(p);
    int lastPermille = -1, lastWhat = -1;
    const Report report = [&](const Step& st) {
        const int permille = st.total ? int(st.done * 1000 / st.total) : 0;
        if (permille != lastPermille || int(st.what) != lastWhat) {  // the window is told only what it shows
            AcquireSRWLockExclusive(&a.shared->lock);
            a.shared->step = st;
            ReleaseSRWLockExclusive(&a.shared->lock);
            PostMessageW(a.win, kStepMessage, 0, 0);
            lastPermille = permille;
            lastWhat = int(st.what);
        }
        return !a.shared->cancel;
    };
    std::wstring error;
    const bool ok = (*a.work)(report, error);
    AcquireSRWLockExclusive(&a.shared->lock);
    a.shared->ok = ok;
    a.shared->error = ok ? L"" : error.empty() || a.shared->cancel ? std::wstring(L"stopped") : error;
    ReleaseSRWLockExclusive(&a.shared->lock);
    PostMessageW(a.win, kEndMessage, 0, 0);
    return 0;
}

}  // namespace

Answer askUpdate(const std::string& version, settings::Lang l) {
    Small s;
    Window w;
    if (!open(w, s, nullptr, 440, 150)) return Answer::Play;
    w.control(L"STATIC", wtrf(l, Text::UpdateOut, {version}), SS_LEFT | SS_NOPREFIX, 0, 16, 14, 408, 28, true);
    w.control(L"STATIC", wtr(l, Text::UpdateRestartNote), SS_LEFT | SS_NOPREFIX, 0, 16, 48, 408, 44);
    w.control(L"BUTTON", wtr(l, Text::UpdatePlayWithout), BS_PUSHBUTTON | WS_TABSTOP, kSecond, 136, 106, 170, 30);
    HWND update =
        w.control(L"BUTTON", wtr(l, Text::UpdateDo), BS_DEFPUSHBUTTON | WS_TABSTOP, kFirst, 314, 106, 110, 30);
    run(w.win, update);
    return s.pressed == kFirst ? Answer::Update : Answer::Play;
}

bool runWithProgress(HWND owner, const std::wstring& title, settings::Lang l, bool thenGame, const Work& work,
                     std::wstring& error) {
    Shared shared;
    Small s;
    s.shared = &shared;
    s.lang = l;
    s.thenGame = thenGame;
    Window w;
    if (!open(w, s, owner, 460, 150)) {
        error = L"no window for it";
        return false;
    }
    w.control(L"STATIC", title, SS_LEFT | SS_NOPREFIX, 0, 16, 14, 428, 28, true);
    s.bar = w.control(PROGRESS_CLASSW, L"", PBS_SMOOTH, 0, 16, 50, 428, 16);
    SendMessageW(s.bar, PBM_SETRANGE32, 0, 1000);
    s.status = w.control(L"STATIC", L"", SS_LEFT | SS_NOPREFIX, 0, 16, 74, 428, 36);
    s.cancel = w.control(L"BUTTON", wtr(l, Text::ButtonCancel), BS_PUSHBUTTON | WS_TABSTOP, kSecond, 334, 112, 110, 30);
    WorkArgs args{&shared, w.win, &work};
    if (owner) EnableWindow(owner, FALSE);
    HANDLE thread = CreateThread(nullptr, 0, workThread, &args, 0, nullptr);
    if (!thread) {
        if (owner) EnableWindow(owner, TRUE);
        DestroyWindow(w.win);
        error = L"no thread for it";
        return false;
    }
    run(w.win, s.cancel);
    WaitForSingleObject(thread, INFINITE);  // the window ends only with the work (Cancel waits for it)
    CloseHandle(thread);
    if (owner) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
    error = shared.error;
    return shared.ok;
}

bool askUninstall(const std::wstring& dir, settings::Lang l, bool& keep) {
    Small s;
    Window w;
    if (!open(w, s, nullptr, 480, 170)) return false;
    w.control(L"STATIC", wtr(l, Text::UninstallButton), SS_LEFT | SS_NOPREFIX, 0, 16, 14, 448, 28, true);
    w.control(L"STATIC", wtrf(l, Text::UninstallAsk, {utf8(dir)}), SS_LEFT | SS_NOPREFIX, 0, 16, 46, 448, 48);
    HWND box = w.control(L"BUTTON", wtr(l, Text::UninstallKeep), BS_AUTOCHECKBOX | WS_TABSTOP, kCheck, 16, 98, 448, 22);
    SendMessageW(box, BM_SETCHECK, BST_CHECKED, 0);
    w.control(L"BUTTON", wtr(l, Text::UninstallDo), BS_PUSHBUTTON | WS_TABSTOP, kFirst, 236, 128, 110, 30);
    HWND no =
        w.control(L"BUTTON", wtr(l, Text::ButtonCancel), BS_DEFPUSHBUTTON | WS_TABSTOP, kSecond, 354, 128, 110, 30);
    run(w.win, no);
    keep = s.keep;
    return s.pressed == kFirst;
}

void tell(const std::wstring& text, bool error) {
    MessageBoxW(nullptr, text.c_str(), L"uaRO Graphics",
                (error ? MB_ICONERROR : MB_ICONINFORMATION) | MB_OK | MB_TOPMOST | MB_SETFOREGROUND);
}

}  // namespace uagfx::app
