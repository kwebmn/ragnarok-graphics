#include "app/notes_window.hpp"

#include "app/ui.hpp"

namespace uagfx::app {
namespace {

const wchar_t kNotesClass[] = L"uaroGraphicsNotes";
constexpr int kText = 100;

LRESULT CALLBACK notesProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE: {  // the text over the whole window, OK under it on the right
            const int dpi = int(GetWindowLongPtrW(h, GWLP_USERDATA));
            const int w = LOWORD(lp), height = HIWORD(lp);
            const int pad = px(12, dpi), bh = px(30, dpi), bw = px(110, dpi);
            MoveWindow(GetDlgItem(h, kText), pad, pad, w - 2 * pad, height - 3 * pad - bh, TRUE);
            MoveWindow(GetDlgItem(h, IDOK), w - pad - bw, height - pad - bh, bw, bh, TRUE);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) DestroyWindow(h);
            return 0;
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

}  // namespace

void showNotes(HWND owner, const std::wstring& title, const std::wstring& text, HFONT font, int dpi) {
    HINSTANCE inst = GetModuleHandleW(nullptr);
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof wc;
        wc.lpfnWndProc = notesProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.lpszClassName = kNotesClass;
        registered = RegisterClassExW(&wc) != 0;
    }
    std::wstring lines;  // a multi-line edit breaks at "\r\n" only
    for (wchar_t ch : text) lines += ch == L'\n' ? std::wstring(L"\r\n") : std::wstring(1, ch);
    RECT o;
    GetWindowRect(owner, &o);
    const int w = px(640, dpi), h = px(520, dpi);
    HWND win = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, kNotesClass, title.c_str(),
                               WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, o.left + (o.right - o.left - w) / 2,
                               o.top + (o.bottom - o.top - h) / 2, w, h, owner, nullptr, inst, nullptr);
    if (!win) return;
    SetWindowLongPtrW(win, GWLP_USERDATA, dpi);
    HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", lines.c_str(),
                                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | ES_MULTILINE | ES_READONLY |
                                    ES_AUTOVSCROLL,
                                0, 0, 10, 10, win, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kText)), inst, nullptr);
    HWND ok = CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 10, 10,
                              win, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), inst, nullptr);
    SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    RECT c;
    GetClientRect(win, &c);
    SendMessageW(win, WM_SIZE, 0, MAKELPARAM(c.right, c.bottom));
    EnableWindow(owner, FALSE);
    ShowWindow(win, SW_SHOW);
    SetFocus(ok);
    MSG m;
    while (IsWindow(win)) {
        const BOOL got = GetMessageW(&m, nullptr, 0, 0);
        if (got <= 0) {  // the app is ending: its quit for the main loop
            if (got == 0) PostQuitMessage(int(m.wParam));
            break;
        }
        if (IsDialogMessageW(win, &m)) continue;  // Enter, Esc, Tab
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
}

}  // namespace uagfx::app
