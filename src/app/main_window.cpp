#include "app/main_window.hpp"

#include <commctrl.h>

namespace uagfx::app {
namespace {

using namespace settings;

const wchar_t kClass[] = L"uaroGraphicsSettings";
constexpr int kWidth = 880;  // the client area at 96 DPI
constexpr int kHeight = 600;
constexpr int kSidebar = 10;
constexpr int kLang = 11;
constexpr int kApply = 20;
constexpr int kPlay = 21;
constexpr int kUndo = 22;
constexpr int kCloseButton = 23;
constexpr UINT kRebuild = WM_APP + 1;
constexpr UINT_PTR kTimer = 1;

}  // namespace

int MainWindow::run(HINSTANCE inst, int show) {
    if (HWND other = FindWindowW(kClass, nullptr)) {  // one settings app at a time: two would write over each other
        ShowWindow(other, SW_RESTORE);
        SetForegroundWindow(other);
        return 0;
    }
    model_ = std::make_unique<Model>();
    const auto saved = model_->savedLanguage();
    lang_ = saved ? *saved : langForWindows(GetUserDefaultUILanguage());
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);
    hwnd_ = CreateWindowExW(0, kClass, wtr(lang_, Text::AppTitle).c_str(),
                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
                            kWidth, kHeight, nullptr, nullptr, inst, this);
    if (!hwnd_) return 1;
    ShowWindow(hwnd_, show);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (IsDialogMessageW(hwnd_, &msg)) continue;  // Tab, arrows and Alt+letter between the controls
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return int(msg.wParam);
}

LRESULT CALLBACK MainWindow::proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* self = static_cast<MainWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->handle(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

void MainWindow::makeFonts() {
    if (font_) DeleteObject(font_);
    if (heading_) DeleteObject(heading_);
    font_ = makeFont(dpi_, false);
    heading_ = makeFont(dpi_, true, 115);
}

void MainWindow::create() {
    dpi_ = windowDpi(hwnd_);
    makeFonts();
    HINSTANCE inst = GetModuleHandleW(nullptr);
    auto child = [&](const wchar_t* cls, DWORD style, int id, DWORD ex = 0) {
        HWND c = CreateWindowExW(ex, cls, L"", WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, hwnd_,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), inst, nullptr);
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        return c;
    };
    sidebar_ = child(L"LISTBOX", LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_TABSTOP | WS_VSCROLL, kSidebar, WS_EX_CLIENTEDGE);
    langBox_ = child(WC_COMBOBOXW, CBS_DROPDOWNLIST | WS_TABSTOP, kLang);
    for (int i = 0; i < kLangCount; ++i)
        SendMessageW(langBox_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide(langName(Lang(i))).c_str()));
    SendMessageW(langBox_, CB_SETCURSEL, WPARAM(lang_), 0);
    banner_ = child(L"STATIC", SS_LEFT | SS_NOPREFIX, 0);
    ShowWindow(banner_, model_->system().elevated ? SW_SHOW : SW_HIDE);
    status_ = child(L"STATIC", SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX | SS_ENDELLIPSIS, 0);
    apply_ = child(L"BUTTON", BS_PUSHBUTTON | WS_TABSTOP, kApply);
    play_ = child(L"BUTTON", BS_PUSHBUTTON | WS_TABSTOP, kPlay);
    undo_ = child(L"BUTTON", BS_PUSHBUTTON | WS_TABSTOP, kUndo);
    close_ = child(L"BUTTON", BS_PUSHBUTTON | WS_TABSTOP, kCloseButton);
    tooltip_ = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd_, nullptr, inst, nullptr);
    SendMessageW(tooltip_, TTM_SETMAXTIPWIDTH, 0, px(380, dpi_));
    SendMessageW(tooltip_, TTM_SETDELAYTIME, TTDT_AUTOPOP, 20000);
    // The client area of 880 x 600 at this DPI.
    RECT r{0, 0, px(kWidth, dpi_), px(kHeight, dpi_)};
    AdjustWindowRectEx(&r, DWORD(GetWindowLongPtrW(hwnd_, GWL_STYLE)), FALSE, 0);
    SetWindowPos(hwnd_, nullptr, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER);
    layout();
    retext();
    SendMessageW(sidebar_, LB_SETCURSEL, 0, 0);
    buildPage();
    SetTimer(hwnd_, kTimer, 2000, nullptr);  // whether the game was started or closed meanwhile
}

RECT MainWindow::pageArea() const {
    RECT client;
    GetClientRect(hwnd_, &client);
    return RECT{px(204, dpi_), px(56, dpi_), client.right - px(16, dpi_), client.bottom - px(56, dpi_)};
}

void MainWindow::layout() {
    RECT c;
    GetClientRect(hwnd_, &c);
    const int w = c.right;
    const int h = c.bottom;
    auto at = [&](int v) { return px(v, dpi_); };
    for (HWND ctl : {sidebar_, langBox_, banner_, status_, apply_, play_, undo_, close_})
        SendMessageW(ctl, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    MoveWindow(sidebar_, at(12), at(12), at(180), h - at(12) - at(60), TRUE);
    SendMessageW(sidebar_, LB_SETITEMHEIGHT, 0, at(30));
    MoveWindow(langBox_, w - at(12) - at(150), at(12), at(150), at(240), TRUE);
    MoveWindow(banner_, at(204), at(8), w - at(204) - at(12) - at(160), at(44), TRUE);
    const int by = h - at(46);
    const int bh = at(32);
    int x = w - at(12);
    for (auto b : {std::make_pair(close_, 110), std::make_pair(undo_, 120), std::make_pair(play_, 220),
                   std::make_pair(apply_, 130)}) {
        x -= at(b.second);
        MoveWindow(b.first, x, by, at(b.second), bh, TRUE);
        x -= at(8);
    }
    MoveWindow(status_, at(12), by, x - at(12), bh, TRUE);
}

void MainWindow::retext() {
    SetWindowTextW(hwnd_, wtr(lang_, Text::AppTitle).c_str());
    const int selected = int(SendMessageW(sidebar_, LB_GETCURSEL, 0, 0));
    SendMessageW(sidebar_, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < kPageCount; ++i)
        SendMessageW(sidebar_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wtr(lang_, pageName(settings::Page(i))).c_str()));
    SendMessageW(sidebar_, LB_SETCURSEL, WPARAM(selected < 0 ? 0 : selected), 0);
    SetWindowTextW(banner_, wtr(lang_, Text::ElevatedBanner).c_str());
    SetWindowTextW(apply_, wtr(lang_, Text::Apply).c_str());
    SetWindowTextW(play_, wtr(lang_, Text::ApplyAndPlay).c_str());
    SetWindowTextW(undo_, wtr(lang_, Text::Revert).c_str());
    SetWindowTextW(close_, wtr(lang_, Text::Close).c_str());
}

void MainWindow::buildPage() {
    rebuildPosted_ = false;
    HWND focus = GetFocus();
    const int focusId = focus && IsChild(hwnd_, focus) ? GetDlgCtrlID(focus) : 0;
    if (focusId >= 1000 && current_) {
        current_->command(focusId, EN_KILLFOCUS);  // a number being typed is kept, not lost with its field
        rebuildPosted_ = false;
        SetFocus(sidebar_);
    }
    quiet_ = true;
    current_.reset();
    panel_.reset();
    panel_ = std::make_unique<Panel>(hwnd_, font_, heading_, tooltip_, dpi_, pageArea());
    switch (settings::Page(page_)) {
        case settings::Page::Screen: current_ = makeScreenPage(*this); break;
        case settings::Page::Diagnostics: current_ = makeDiagnosticsPage(*this); break;
        case settings::Page::Service: current_ = makeServicePage(*this); break;
        default: current_ = makeSettingsPage(*this, settings::Page(page_)); break;
    }
    current_->build(*panel_);
    quiet_ = false;
    if (focusId >= 1000)
        if (HWND f = GetDlgItem(hwnd_, focusId)) SetFocus(f);
    updateStatus();
}

void MainWindow::updateStatus() {
    const size_t pending = model_->pendingCount();
    std::wstring text = flash_;
    if (text.empty()) {
        text = wtr(lang_, model_->now().gameRunning ? Text::StatusRunning : Text::StatusClosed);
        if (pending) text += L" " + wtrf(lang_, Text::StatusPending, {num(static_cast<long long>(pending))});
    }
    SetWindowTextW(status_, text.c_str());
    EnableWindow(apply_, pending > 0);
    EnableWindow(play_, !model_->system().elevated);
    EnableWindow(undo_, model_->canRevert());
}

void MainWindow::changed() {
    flash_.clear();
    updateStatus();
}

void MainWindow::rebuild() {
    if (rebuildPosted_) return;
    rebuildPosted_ = true;
    PostMessageW(hwnd_, kRebuild, 0, 0);  // after this message: the control that asked is not taken down under it
}

void MainWindow::reload() {
    retext();
    buildPage();
}

void MainWindow::message(const std::wstring& text, bool error) {
    MessageBoxW(hwnd_, text.c_str(), wtr(lang_, Text::AppTitle).c_str(), error ? MB_ICONWARNING : MB_ICONINFORMATION);
}

bool MainWindow::confirmClose() {
    return model_->pendingCount() == 0 || MessageBoxW(hwnd_, wtr(lang_, Text::AskUnsaved).c_str(),
                                                      wtr(lang_, Text::AppTitle).c_str(),
                                                      MB_YESNO | MB_ICONQUESTION) == IDYES;
}

void MainWindow::doApply(bool play) {
    std::wstring err = model_->apply(lang_);
    if (err.empty() && play) err = model_->launchGame(lang_);
    if (!err.empty())
        message(err, true);
    else
        flash_ = wtr(lang_, Text::StatusApplied);
    reload();
}

LRESULT MainWindow::handle(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            create();
            return 0;
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            const int code = HIWORD(wp);
            if (id == kSidebar && code == LBN_SELCHANGE) {
                const LRESULT sel = SendMessageW(sidebar_, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel != page_) {
                    page_ = int(sel);
                    buildPage();
                }
            } else if (id == kLang && code == CBN_SELCHANGE) {
                const LRESULT sel = SendMessageW(langBox_, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < kLangCount) {
                    lang_ = Lang(sel);
                    model_->saveLanguage(lang_);
                    reload();
                }
            } else if (id == kApply && code == BN_CLICKED) {
                doApply(false);
            } else if (id == kPlay && code == BN_CLICKED) {
                doApply(true);
            } else if (id == kUndo && code == BN_CLICKED) {
                const std::wstring err = model_->revert(lang_);
                if (!err.empty()) message(err, true);
                flash_.clear();
                model_->load();
                reload();
            } else if (id == kCloseButton && code == BN_CLICKED) {
                PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            } else if (id == IDOK) {  // Enter: a number field takes what was typed
                HWND focus = GetFocus();
                const int focusId = focus ? GetDlgCtrlID(focus) : 0;
                if (focusId >= 1000 && current_) current_->command(focusId, EN_KILLFOCUS);
            } else if (id == IDCANCEL) {
                PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            } else if (id >= 1000 && current_ && !quiet_) {
                current_->command(id, code);
            }
            return 0;
        }
        case WM_HSCROLL:
            if (lp && current_ && !quiet_) {
                HWND ctl = reinterpret_cast<HWND>(lp);
                current_->scroll(GetDlgCtrlID(ctl), LOWORD(wp), int(SendMessageW(ctl, TBM_GETPOS, 0, 0)));
            }
            return 0;
        case WM_NOTIFY: {
            const auto* n = reinterpret_cast<const NMHDR*>(lp);
            if (n && n->idFrom >= 1000 && current_ && !quiet_) return current_->notify(n);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wp);
            HWND ctl = reinterpret_cast<HWND>(lp);
            Tone tone = Tone::Normal;
            if (ctl == banner_)
                SetTextColor(dc, toneColor(Tone::Bad));
            else if (panel_ && panel_->toneOf(ctl, tone))
                SetTextColor(dc, toneColor(tone));
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        }
        case kRebuild:
            if (rebuildPosted_) buildPage();
            return 0;
        case WM_TIMER:
            if (wp == kTimer && model_->refreshRunning()) buildPage();  // the game started or closed
            return 0;
        case WM_DPICHANGED: {
            dpi_ = HIWORD(wp);
            makeFonts();
            const RECT* r = reinterpret_cast<const RECT*>(lp);
            SetWindowPos(hwnd_, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            SendMessageW(tooltip_, TTM_SETMAXTIPWIDTH, 0, px(380, dpi_));
            layout();
            buildPage();
            return 0;
        }
        case WM_CLOSE:
            if (confirmClose()) DestroyWindow(hwnd_);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd_, kTimer);
            current_.reset();
            panel_.reset();
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

}  // namespace uagfx::app
