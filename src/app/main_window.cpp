#include "app/main_window.hpp"

#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>

#include <algorithm>
#include <memory>

#include "app/client_patch.hpp"
#include "app/mnemonic.hpp"
#include "app/notes_window.hpp"
#include "app/report_send.hpp"
#include "app/reports.hpp"
#include "app/status_info.hpp"
#include "app/launcher.hpp"
#include "app/modes.hpp"
#include "app/update.hpp"
#include "app/update_window.hpp"
#include "core/settings/search.hpp"
#include "core/update_check.hpp"
#include "core/version.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

const wchar_t kClass[] = L"uaroGraphicsSettings";
const wchar_t kViewClass[] = L"uaroGraphicsPage";
constexpr int kWidth = 880;  // the client area at 96 DPI: the least (2.6: it was the only size)
constexpr int kHeight = 600;
constexpr int kStartWidth = 1100;  // 2.6: the first start's, where the screen has room
constexpr int kStartHeight = 720;
constexpr int kSidebar = 10;
constexpr int kLang = 11;
constexpr int kApply = 20;
constexpr int kPlay = 21;
constexpr int kUndo = 22;
constexpr int kCloseButton = 23;
constexpr int kModeFirst = 30;     // 2.6: the three modes' buttons, 30-32
constexpr int kSearch = 33;        // the search field
constexpr int kReportButton = 34;  // 2.13: «Звернення» (0.51-2.12: "Report a problem")
constexpr int kTabFirst = 40;      // a view's tabs, 40-43: the page view's children, made with the page
constexpr int kResetView = 44;     // "Reset the page to the recommended values", made with the page
constexpr int kStrip = 26;         // 2.6: the status bar's height at 96 DPI
constexpr int kStripUpdate = 45;   // 2.10: the status bar's "Update"
constexpr int kHeader = 52;        // 2.6: the header's height at 96 DPI
constexpr int kList = 200;         // the list's width
constexpr UINT kRebuild = WM_APP + 1;
constexpr UINT kShowPage = WM_APP + 7;  // 1.6: wParam the page; 2.6: the view, lParam its tab
constexpr UINT kAskStats = WM_APP + 8;  // 2.2.1: once, the window shown: may the statistics go
constexpr UINT kAskMachine = WM_APP + 9;  // 2.12: the card or the screen changed since the settings were picked
constexpr UINT kOpenReports = WM_APP + 10;  // 2.13: --report - «Нове звернення» once the window is up
// 2.13: WM_COPYDATA of a second app the game's Ctrl+Shift+F10 started: "<shot>\n<map>" in UTF-8
constexpr ULONG_PTR kReportData = 0x52505254;

// 2.2.1: the day's statistics, on a thread of its own (the network may take seconds); the folder's copy its own.
DWORD WINAPI sendStats(void* folder) {
    std::unique_ptr<std::wstring> dir(static_cast<std::wstring*>(folder));
    std::string error;
    sendUsage(*dir, false, error);
    return 0;
}
constexpr UINT_PTR kTimer = 1;
constexpr UINT_PTR kTickTimer = 2;  // 0.43: a page's tick()
constexpr UINT_PTR kSearchTimer = 3;  // 2.6: the search runs once the typing stops for a moment

}  // namespace

int MainWindow::run(HINSTANCE inst, int show) {
    if (HWND other = FindWindowW(kClass, nullptr)) {  // one settings app at a time: two would write over each other
        if (startReport_) {  // 2.13: the open app makes the game's report
            const std::string text = utf8(reportStart_.shot) + "\n" + reportStart_.map;
            COPYDATASTRUCT cd{kReportData, DWORD(text.size()), const_cast<char*>(text.data())};
            SendMessageW(other, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cd));
        }
        ShowWindow(other, SW_RESTORE);
        SetForegroundWindow(other);
        return 0;
    }
    model_ = std::make_unique<Model>();
    const auto saved = model_->savedLanguage();
    // 0.48: English until one is chosen; 2.12: Windows' language until then (uk, ru, else en)
    lang_ = settings::windowLang(saved, PRIMARYLANGID(GetUserDefaultUILanguage()));
    mode_ = model_->startMode();  // 2.6
    shown_ = viewsOf(mode_).front();
    newInstall_ = !model_->usedBefore();  // before the statistics' question writes its answer
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
    WNDCLASSEXW vc = wc;  // the page view (0.22.1)
    vc.lpfnWndProc = viewProc;
    vc.hIcon = vc.hIconSm = nullptr;
    vc.lpszClassName = kViewClass;
    RegisterClassExW(&vc);
    // 2.6: a window that grows and is maximized, as it was closed the last time
    hwnd_ = CreateWindowExW(0, kClass, wtr(lang_, Text::AppTitle).c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            CW_USEDEFAULT, CW_USEDEFAULT, kWidth, kHeight, nullptr, nullptr, inst, this);
    if (!hwnd_) return 1;
    if (const auto place = model_->savedPlace()) {
        RECT r{place->left, place->top, place->right, place->bottom};
        if (MonitorFromRect(&r, MONITOR_DEFAULTTONULL)) {  // a monitor that is gone: the default place
            WINDOWPLACEMENT wp{};
            wp.length = sizeof wp;
            GetWindowPlacement(hwnd_, &wp);
            wp.rcNormalPosition = r;
            wp.showCmd = SW_HIDE;
            SetWindowPlacement(hwnd_, &wp);
            if (place->maximized && (show == SW_SHOWNORMAL || show == SW_SHOWDEFAULT || show == SW_SHOW))
                show = SW_SHOWMAXIMIZED;
        }
    }
    ShowWindow(hwnd_, show);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_MOUSEWHEEL && wheelToPage(msg)) continue;
        if (msg.message == WM_KEYDOWN && msg.wParam == 'F' && GetKeyState(VK_CONTROL) < 0 && search_) {  // 2.6
            SetFocus(search_);
            SendMessageW(search_, EM_SETSEL, 0, -1);
            continue;
        }
        if (IsDialogMessageW(hwnd_, &msg)) {  // Tab, arrows and Alt+letter between the controls
            if (msg.message == WM_KEYDOWN || msg.message == WM_SYSCHAR || msg.message == WM_CHAR) showFocused();
            continue;
        }
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

LRESULT CALLBACK MainWindow::viewProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* self = static_cast<MainWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self && self->view_ == hwnd ? self->handleView(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

// The page's controls report to the main window as before; the view scrolls by its bar and the wheel.
LRESULT MainWindow::handleView(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND:
        case WM_NOTIFY:
        case WM_HSCROLL:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
            return handle(msg, wp, lp);
        case WM_VSCROLL: {
            if (lp) return 0;  // a control's own, not the view's bar
            SCROLLINFO si{};
            si.cbSize = sizeof si;
            si.fMask = SIF_ALL;
            GetScrollInfo(view_, SB_VERT, &si);
            int y = scroll_;
            switch (LOWORD(wp)) {
                case SB_LINEUP: y -= px(40, dpi_); break;
                case SB_LINEDOWN: y += px(40, dpi_); break;
                case SB_PAGEUP: y -= int(si.nPage); break;
                case SB_PAGEDOWN: y += int(si.nPage); break;
                case SB_THUMBTRACK:
                case SB_THUMBPOSITION: y = si.nTrackPos; break;
                case SB_TOP: y = 0; break;
                case SB_BOTTOM: y = pageHeight_; break;
                default: break;
            }
            scrollTo(y);
            return 0;
        }
        case WM_MOUSEWHEEL:  // 120 a notch: 60 pixels at 96 DPI; a touchpad's smaller steps as smoothly
            scrollTo(scroll_ - int(short(HIWORD(wp))) * px(60, dpi_) / WHEEL_DELTA);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(view_, msg, wp, lp);
}

void MainWindow::scrollTo(int y) {
    RECT r;
    GetClientRect(view_, &r);
    y = std::clamp(y, 0, std::max(0, pageHeight_ - int(r.bottom)));
    if (y == scroll_) return;
    ScrollWindowEx(view_, 0, scroll_ - y, nullptr, nullptr, nullptr, nullptr,
                   SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
    scroll_ = y;
    SetScrollPos(view_, SB_VERT, y, TRUE);
    if (panel_) panel_->moveTips();
}

// The bar's range is the page's height; a page that fits shows no bar.
void MainWindow::updateScrollBar() {
    RECT r;
    GetClientRect(view_, &r);
    SCROLLINFO si{};
    si.cbSize = sizeof si;
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = std::max(0, pageHeight_ - 1);
    si.nPage = UINT(std::max(0L, r.bottom));
    si.nPos = scroll_;
    SetScrollInfo(view_, SB_VERT, &si, TRUE);
}

void MainWindow::showFocused() {
    HWND f = GetFocus();
    if (!view_ || !f || !IsChild(view_, f)) return;
    RECT r;
    GetWindowRect(f, &r);
    MapWindowPoints(nullptr, view_, reinterpret_cast<POINT*>(&r), 2);
    RECT c;
    GetClientRect(view_, &c);
    const int margin = px(8, dpi_);
    if (r.top < margin)
        scrollTo(scroll_ + r.top - margin);
    else if (r.bottom > c.bottom - margin)
        scrollTo(scroll_ + r.bottom - c.bottom + margin);
}

bool MainWindow::wheelToPage(const MSG& m) {
    if (!view_) return false;
    const POINT at{short(LOWORD(m.lParam)), short(HIWORD(m.lParam))};  // screen coordinates
    HWND under = WindowFromPoint(at);
    if (!under || (under != view_ && !IsChild(view_, under))) return false;
    // A list scrolls itself (the tables of the AI textures and of the diagnostics); an open combo box's list is a
    // window of its own, outside the view.
    wchar_t cls[32] = {};
    GetClassNameW(under, cls, 32);
    if (lstrcmpiW(cls, WC_LISTVIEWW) == 0 || lstrcmpiW(cls, L"ListBox") == 0) return false;
    SendMessageW(view_, WM_MOUSEWHEEL, m.wParam, m.lParam);
    return true;
}

void MainWindow::makeFonts() {
    if (!pageFont_) {  // the page keeps its fonts until it is made again (buildPage)
        pageFont_ = font_;
        pageHeading_ = heading_;
    } else {  // made at a DPI change before: nothing but the bar keeps them, and layout() gives it the new ones
        if (font_) DeleteObject(font_);
        if (heading_) DeleteObject(heading_);
    }
    font_ = makeFont(dpi_, false);
    heading_ = makeFont(dpi_, true, 115);
    if (bold_) DeleteObject(bold_);  // the list's own: drawn with it
    bold_ = makeFont(dpi_, true);
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
    // 2.6: the header - the modes, «Звернення» (2.13); the list of the mode's views, drawn here
    for (int i = 0; i < kModeCount; ++i)
        modes_[i] =
            child(L"BUTTON", BS_AUTORADIOBUTTON | BS_PUSHLIKE | WS_TABSTOP | (i ? 0 : WS_GROUP), kModeFirst + i);
    search_ = child(L"EDIT", ES_AUTOHSCROLL | WS_TABSTOP | WS_GROUP, kSearch, WS_EX_CLIENTEDGE);
    stripLeft_ = child(L"STATIC", SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX | SS_ENDELLIPSIS, 0);
    stripRight_ = child(L"STATIC", SS_RIGHT | SS_CENTERIMAGE | SS_NOPREFIX, 0);
    stripUpdate_ = child(L"BUTTON", BS_PUSHBUTTON | WS_TABSTOP, kStripUpdate);
    ShowWindow(stripUpdate_, SW_HIDE);
    stripBrush_ = CreateSolidBrush(RGB(243, 243, 243));
    report_ = child(L"BUTTON", BS_PUSHBUTTON | WS_TABSTOP | WS_GROUP, kReportButton);
    listBrush_ = CreateSolidBrush(RGB(247, 247, 247));
    lineBrush_ = CreateSolidBrush(RGB(227, 227, 227));
    chosenBrush_ = CreateSolidBrush(RGB(230, 230, 230));
    accentBrush_ = CreateSolidBrush(toneColor(Tone::Accent));
    sidebar_ = child(L"LISTBOX",
                     LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_TABSTOP | WS_VSCROLL | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
                     kSidebar);
    // The page view: its controls are a group of the window's for Tab (control parent); it scrolls them.
    view_ = CreateWindowExW(WS_EX_CONTROLPARENT, kViewClass, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL,
                            0, 0, 10, 10, hwnd_, nullptr, inst, this);
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
    // 2.6: a client area of 1100 x 720 at this DPI where the screen has room, else 880 x 600
    RECT r{0, 0, px(kStartWidth, dpi_), px(kStartHeight, dpi_)};
    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    if (GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi)) {
        const int workWidth = mi.rcWork.right - mi.rcWork.left;
        const int workHeight = mi.rcWork.bottom - mi.rcWork.top;
        if (r.right > workWidth * 9 / 10 || r.bottom > workHeight * 9 / 10)
            r = RECT{0, 0, px(kWidth, dpi_), px(kHeight, dpi_)};
    }
    AdjustWindowRectEx(&r, DWORD(GetWindowLongPtrW(hwnd_, GWL_STYLE)), FALSE, 0);
    SetWindowPos(hwnd_, nullptr, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER);
    layout();
    retext();
    // 2.4.2: the game's key this app's from its start, setup.exe out of it - written when there is none (the game would
    // start setup.exe and wait for it), setup.exe's values taken over while the game is closed
    const std::wstring owned = model_->ownGameKey(lang_);
    if (!owned.empty()) message(owned, true);
    buildPage();
    SetTimer(hwnd_, kTimer, 2000, nullptr);  // whether the game was started or closed meanwhile
    // 2.12: a fresh install set up for this computer at once («Гра» says so), a changed card or screen asked about
    if (model_->startSetup(lang_) == settings::FirstStart::Ask) PostMessageW(hwnd_, kAskMachine, 0, 0);
    startUpdateCheck();  // 2.10: at every start, on a thread of its own (about a second)
    startLauncherCheck(lang_);  // 2.11: the news and the online count of «Гра»
    startClientPatchCheck(model_->paths().dir);  // 2.14: the game's new files
    // 2.13: the waiting reports sent, the answers counted
    startReportsCheck(model_->paths().dir, computerKey(model_->paths().dir));
    PostMessageW(hwnd_, kAskStats, 0, 0);
    if (startReport_) PostMessageW(hwnd_, kOpenReports, 0, 0);
}

// 2.6: the page view right of the list, between the header and the buttons' row; the page's own left margin holds
// the "changed" dots
RECT MainWindow::pageArea() const {
    RECT client;
    GetClientRect(hwnd_, &client);
    return RECT{px(kList + 8, dpi_), contentTop() + px(8, dpi_), client.right - px(8, dpi_),
                rowTop() - px(9 + 8, dpi_)};
}

int MainWindow::contentTop() const {
    return px(kHeader, dpi_) + 1 + (model_->system().elevated ? px(48, dpi_) : 0);
}

int MainWindow::rowTop() const {
    RECT c;
    GetClientRect(hwnd_, &c);
    return c.bottom - px(kStrip + 46, dpi_);
}

void MainWindow::layout() {
    RECT c;
    GetClientRect(hwnd_, &c);
    const int w = c.right;
    auto at = [&](int v) { return px(v, dpi_); };
    for (HWND ctl : {sidebar_, langBox_, banner_, status_, apply_, play_, undo_, close_, report_, search_, modes_[0],
                     modes_[1], modes_[2], stripLeft_, stripRight_})
        SendMessageW(ctl, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    // 2.6: the modes, the search (what the rest leaves of 300), "Report a problem" and the language on the right
    int x = at(16);
    for (HWND m : modes_) {
        MoveWindow(m, x, at(11), at(100), at(30), TRUE);
        x += at(100) + at(2);
    }
    const int right = w - at(16 + 130 + 12 + 170 + 12);
    MoveWindow(search_, x + at(10), at(12), std::clamp(right - x - at(10), at(120), at(300)), at(28), TRUE);
    MoveWindow(langBox_, w - at(16) - at(130), at(14), at(130), at(240), TRUE);
    MoveWindow(report_, w - at(16 + 130 + 12 + 170), at(11), at(170), at(30), TRUE);
    MoveWindow(banner_, at(16), at(kHeader) + at(5), w - at(32), at(40), TRUE);
    const int top = contentTop();
    const int by = rowTop();
    MoveWindow(sidebar_, 0, top + at(8), at(kList), by - at(9) - top - at(8), TRUE);
    SendMessageW(sidebar_, LB_SETITEMHEIGHT, 0, at(32));
    const int bh = at(32);
    x = w - at(12);
    for (auto b : {std::make_pair(close_, 110), std::make_pair(undo_, 120), std::make_pair(play_, 220),
                   std::make_pair(apply_, 130)}) {
        x -= at(b.second);
        MoveWindow(b.first, x, by, at(b.second), bh, TRUE);
        x -= at(8);
    }
    MoveWindow(status_, at(12), by, x - at(12), bh, TRUE);
    // 2.6: the status bar - the driver and the pack on the left, the game on the right
    const int sy = c.bottom - at(kStrip);
    MoveWindow(stripRight_, w - at(16 + 220), sy, at(220), at(kStrip), TRUE);
    MoveWindow(stripUpdate_, w - at(16 + 220 + 8 + 110), sy + at(2), at(110), at(kStrip - 4), TRUE);
    MoveWindow(stripLeft_, at(16), sy, w - at(16 + 220 + 8 + 110 + 16 + 16), at(kStrip), TRUE);
    const RECT page = pageArea();
    MoveWindow(view_, page.left, page.top, page.right - page.left, page.bottom - page.top, TRUE);
    InvalidateRect(hwnd_, nullptr, TRUE);  // the lines and the list's ground move with them
}

// 2.6: the header's line, the list's ground and edge, the line over the buttons' row
void MainWindow::paint() {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd_, &ps);
    RECT c;
    GetClientRect(hwnd_, &c);
    const int top = contentTop();
    const int bottom = rowTop() - px(9, dpi_);
    const int list = px(kList, dpi_);
    const RECT header{0, px(kHeader, dpi_), c.right, px(kHeader, dpi_) + 1};
    const RECT ground{0, top, list, bottom};
    const RECT edge{list, top, list + 1, bottom};
    const RECT foot{0, bottom, c.right, bottom + 1};
    const RECT strip{0, c.bottom - px(kStrip, dpi_), c.right, c.bottom};
    const RECT stripLine{0, strip.top, c.right, strip.top + 1};
    FillRect(dc, &header, lineBrush_);
    FillRect(dc, &ground, listBrush_);
    FillRect(dc, &edge, lineBrush_);
    FillRect(dc, &foot, lineBrush_);
    FillRect(dc, &strip, stripBrush_);
    FillRect(dc, &stripLine, lineBrush_);
    EndPaint(hwnd_, &ps);
}

// 2.6: the status bar's words - the driver's version and its update, the texture pack, the game
void MainWindow::updateStrip() {
    const Snapshot& now = model_->now();
    const std::string installed = now.installedVersion.substr(0, now.installedVersion.find(' '));
    std::wstring left = wtrf(lang_, Text::MainDriver, {installed.empty() ? std::string(kVersion) : installed});
    const UpdateState u = updateState();
    if (u.kind == UpdateState::Checking) left += L" · " + wtr(lang_, Text::UpdateChecking);
    const bool offered = u.kind == UpdateState::Done && !u.offer.empty();
    if (u.kind == UpdateState::Done)
        left += L" · " + (offered ? wtrf(lang_, Text::MainUpdateNew, {u.offer}) : wtr(lang_, Text::MainUpdateNone));
    if (offered != (IsWindowVisible(stripUpdate_) != 0)) ShowWindow(stripUpdate_, offered ? SW_SHOW : SW_HIDE);
    const PackSummary pack = packSummary(model_->paths().dir);
    left += L"     ";
    if (!pack.exists)
        left += wtr(lang_, Text::StripNoPack);
    else if (pack.counted)
        left += wtrf(lang_, Text::MainPack, {num((long long)pack.pictures), sizeText(pack.bytes, lang_)});
    else
        left += wtrf(lang_, Text::PackWriting, {sizeText(pack.bytes, lang_)});
    SetWindowTextW(stripLeft_, left.c_str());
    SetWindowTextW(stripRight_, wtr(lang_, now.gameRunning ? Text::StatusRunning : Text::StatusClosed).c_str());
}

// 2.6: "Reset the page to the recommended values" - every row of the view's tab the mode shows, as the player's
// change (Apply writes them)
void MainWindow::resetView() {
    if (!ask(wtr(lang_, Text::ResetPageAsk))) return;
    for (const settings::IniSetting& s : iniSettings()) {
        const settings::Place at = placeOf(s);
        if (at.view != shown_ || at.tab != tab_ || levelOf(s) > mode()) continue;
        const std::string want = model_->recommended(s);
        if (!want.empty() && want != model_->token(s)) model_->setToken(s, want);
    }
    changed();
    rebuild();
}

void MainWindow::drawListItem(const DRAWITEMSTRUCT& d) {
    if (d.itemID == UINT(-1)) return;  // an empty list's focus
    HDC dc = d.hDC;
    const RECT r = d.rcItem;
    FillRect(dc, &r, listBrush_);
    const int item = d.itemID < listItems_.size() ? listItems_[d.itemID] : -1;
    const bool chosen = (d.itemState & ODS_SELECTED) && item >= 0;
    const RECT box{r.left + px(6, dpi_), r.top + px(2, dpi_), r.right - px(6, dpi_), r.bottom - px(2, dpi_)};
    if (chosen) {  // grey under it, the accent's bar on its left
        const RECT bar{box.left, box.top, box.left + px(3, dpi_), box.bottom};
        FillRect(dc, &box, chosenBrush_);
        FillRect(dc, &bar, accentBrush_);
    }
    const LRESULT n = SendMessageW(sidebar_, LB_GETTEXTLEN, d.itemID, 0);
    std::wstring text(size_t(n > 0 ? n : 0) + 1, L'\0');
    if (n > 0) SendMessageW(sidebar_, LB_GETTEXT, d.itemID, reinterpret_cast<LPARAM>(&text[0]));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, item < 0                            ? RGB(119, 119, 119)
                     : developerView(settings::View(item)) ? RGB(122, 75, 0)
                                                           : GetSysColor(COLOR_WINDOWTEXT));
    HGDIOBJ old = SelectObject(dc, chosen ? bold_ : font_);
    RECT t{r.left + px(item < 0 ? 14 : 20, dpi_), r.top, r.right - px(8, dpi_), r.bottom};
    DrawTextW(dc, text.c_str(), -1, &t, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, old);
    if ((d.itemState & ODS_FOCUS) && item >= 0) DrawFocusRect(dc, &box);
}

void MainWindow::fillList() {
    SendMessageW(sidebar_, LB_RESETCONTENT, 0, 0);
    listItems_.clear();
    int chosen = -1;
    for (settings::View v : viewsOf(mode_)) {
        if (developerView(v) && std::find(listItems_.begin(), listItems_.end(), -1) == listItems_.end()) {
            SendMessageW(sidebar_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wtr(lang_, Text::DeveloperOnly).c_str()));
            listItems_.push_back(-1);
        }
        if (v == shown_) chosen = int(listItems_.size());
        SendMessageW(sidebar_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wtr(lang_, viewName(v)).c_str()));
        listItems_.push_back(int(v));
    }
    SendMessageW(sidebar_, LB_SETCURSEL, WPARAM(chosen), 0);  // -1: a view off the list, none chosen
    listSel_ = chosen;
    for (int i = 0; i < kModeCount; ++i)
        SendMessageW(modes_[i], BM_SETCHECK, i == int(mode_) ? BST_CHECKED : BST_UNCHECKED, 0);
}

bool MainWindow::listed(settings::View v) const {
    const std::vector<settings::View> list = viewsOf(mode_);
    return std::find(list.begin(), list.end(), v) != list.end();
}

// The search finds what the mode shows; a view off the mode's list shows at least the Detailed level.
Mode MainWindow::mode() const {
    return !query_.empty() || listed(shown_) ? mode_ : std::max(mode_, Mode::Detailed);
}

std::wstring MainWindow::searchText() const {
    const int n = GetWindowTextLengthW(search_);
    std::wstring text(size_t(n) + 1, L'\0');
    GetWindowTextW(search_, &text[0], n + 1);
    text.resize(size_t(n));
    return text;
}

// 2.6: the search's text gone - the view back (made by the caller)
void MainWindow::clearSearch() {
    KillTimer(hwnd_, kSearchTimer);
    query_.clear();
    if (GetWindowTextLengthW(search_) > 0) SetWindowTextW(search_, L"");
    KillTimer(hwnd_, kSearchTimer);  // its EN_CHANGE set it again
}

// The view stays when the new mode lists it, else the mode's first; the mode is kept for the next start.
void MainWindow::setMode(Mode m) {
    if (m == mode_) return;
    mode_ = m;
    model_->saveMode(m);
    if (!listed(shown_)) {
        shown_ = viewsOf(m).front();
        tab_ = 0;
        scroll_ = 0;
    }
    fillList();
    buildPage();
}

std::unique_ptr<Page> MainWindow::makeView() {
    using P = settings::Page;
    switch (shown_) {
        case settings::View::Main: return makeMainPage(*this);
        case settings::View::Play: return makePlayPage(*this);  // 2.11: the launcher «Гра»
        case settings::View::Graphics:
            return makeSettingsPage(*this, {P::Render, P::Quality}, {P::Smoothing, P::Frames});
        case settings::View::Screen: return makeScreenPage(*this);
        case settings::View::Effects: return makeEffectsPage(*this);
        case settings::View::Ai:
            if (tab_ == 2) return makePackPage(*this);
            if (tab_ == 3) return makeAiCachePage(*this);
            return makeAiPage(*this, tab_ == 1 ? AiPart::Models : AiPart::What);
        case settings::View::Sound: return makeSettingsPage(*this, {P::Sound});
        case settings::View::Pad: return makePadPage(*this);
        case settings::View::GameChat: return makeSettingsPage(*this, {P::Game, P::Chat});
        case settings::View::Service: {
            std::vector<std::unique_ptr<Page>> parts;
            parts.push_back(makeServicePage(*this));
            parts.push_back(makeDiagnosticsPage(*this));
            return makePartsPage(*this, std::move(parts));
        }
        case settings::View::Debug: break;
    }
    return makeDebugPage(*this);
}

void MainWindow::retext() {
    SetWindowTextW(hwnd_, wtr(lang_, Text::AppTitle).c_str());
    fillList();
    for (int i = 0; i < kModeCount; ++i) SetWindowTextW(modes_[i], wtr(lang_, modeName(Mode(i))).c_str());
    retextReports();
    SetWindowTextW(stripUpdate_, wtr(lang_, Text::UpdateDo).c_str());
    SendMessageW(search_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(wtr(lang_, Text::SearchCue).c_str()));
    SetWindowTextW(banner_, wtr(lang_, Text::ElevatedBanner).c_str());
    SetWindowTextW(apply_, wtr(lang_, Text::Apply).c_str());
    SetWindowTextW(play_, wtr(lang_, Text::ApplyAndPlay).c_str());
    SetWindowTextW(undo_, wtr(lang_, Text::Revert).c_str());
    SetWindowTextW(close_, wtr(lang_, Text::Close).c_str());
}

void MainWindow::buildPage() {
    rebuilds_.begin();  // every request from here on is served by this rebuild; the page's notifications are quiet
    HWND focus = GetFocus();
    const int focusId = focus && IsChild(hwnd_, focus) ? GetDlgCtrlID(focus) : 0;
    // A number field keeps its caret and selection: Tab selected all of it, typing goes on where it was.
    const bool field = focusId >= 1000 && (SendMessageW(focus, WM_GETDLGCODE, 0, 0) & DLGC_HASSETSEL);
    DWORD selStart = 0;
    DWORD selEnd = 0;
    if (field) SendMessageW(focus, EM_GETSEL, reinterpret_cast<WPARAM>(&selStart), reinterpret_cast<LPARAM>(&selEnd));
    if (focusId >= 1000 && current_) {
        current_->command(focusId, EN_KILLFOCUS);  // a number being typed is kept, not lost with its field
        SetFocus(sidebar_);
    }
    current_.reset();
    panel_.reset();
    // The page is laid out in the view from its top, scroll_ above the view's: where it was before it was made again.
    RECT view;
    GetWindowRect(view_, &view);
    using SystemMetricsForDpiFn = int(WINAPI*)(int, UINT);
    static const SystemMetricsForDpiFn metrics = reinterpret_cast<SystemMetricsForDpiFn>(reinterpret_cast<void*>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetSystemMetricsForDpi")));
    const int bar = metrics ? metrics(SM_CXVSCROLL, UINT(dpi_)) : GetSystemMetrics(SM_CXVSCROLL);
    const RECT area{px(16, dpi_), -scroll_, view.right - view.left - bar - px(8, dpi_),
                    view.bottom - view.top - scroll_};
    builtWidth_ = view.right - view.left;  // 2.6: made again when the window's width changes
    panel_ = std::make_unique<Panel>(view_, font_, heading_, tooltip_, dpi_, area, windowLetters());
    if (tabCount(shown_) > 1 && query_.empty()) {  // 2.6: the view's tabs over its page
        std::vector<std::wstring> names;
        for (int i = 0; i < tabCount(shown_); ++i) names.push_back(wtr(lang_, tabName(shown_, i)));
        panel_->tabs(kTabFirst, names, tab_);
    }
    if (query_.empty() && resettable(shown_, tab_)) {  // 2.6: the rows of this view back to the recommended
        panel_->buttons({{kResetView, wtr(lang_, Text::ResetPage)}});
        panel_->note(wtr(lang_, Text::DotLegend));
        panel_->gap(4);
    }
    current_ = query_.empty() ? makeView() : makeSearchPage(*this, utf8(query_));  // 2.6: the search over the view
    current_->build(*panel_);
    if (current_->ticks())
        SetTimer(hwnd_, kTickTimer, 500, nullptr);
    else
        KillTimer(hwnd_, kTickTimer);
    pageHeight_ = panel_->height() + px(8, dpi_);
    updateScrollBar();
    scrollTo(scroll_);  // a page that got shorter: back up to its end
    for (HWND w : {apply_, play_, undo_, close_, langBox_})  // after the page in the Tab order (0.6.0 review M6)
        SetWindowPos(w, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    typing_ = false;  // a number being typed was taken above
    if (pageFont_) DeleteObject(pageFont_);
    if (pageHeading_) DeleteObject(pageHeading_);
    pageFont_ = pageHeading_ = nullptr;
    rebuilds_.end();
    if (focusId >= 1000)
        if (HWND f = GetDlgItem(view_, focusId)) {
            SetFocus(f);
            if (field) SendMessageW(f, EM_SETSEL, selStart, selEnd);
        }
    updateStatus();
}

void MainWindow::updateStatus() {
    const size_t pending = model_->pendingCount();
    std::wstring text = flash_;  // 2.6: the game's state is the status bar's
    if (text.empty() && pending) text = wtrf(lang_, Text::StatusPending, {num(static_cast<long long>(pending))});
    SetWindowTextW(status_, text.c_str());
    updateStrip();
    EnableWindow(apply_, model_->canApply() || typing_);
    EnableWindow(play_, !model_->system().elevated);
    EnableWindow(undo_, model_->canRevert());
}

void MainWindow::changed() {
    flash_.clear();
    updateStatus();
}

void MainWindow::showPage(settings::Page which) {
    const settings::Place p = placeOf(which);
    showView(p.view, p.tab);
}

void MainWindow::showView(settings::View view, int tab) { PostMessageW(hwnd_, kShowPage, WPARAM(view), LPARAM(tab)); }

void MainWindow::rebuild() {
    // After this message: the control that asked is not taken down under it.
    if (rebuilds_.request()) PostMessageW(hwnd_, kRebuild, 0, 0);
}

void MainWindow::reload() {
    retext();
    rebuild();  // the page asking may be the one taken down: it goes when its code is done
}

// While the box waits, its own message loop runs: the page whose code opened it is not made again under it.
int MainWindow::box(const std::wstring& text, UINT type) {
    rebuilds_.boxOpened();
    const int answer = MessageBoxW(hwnd_, text.c_str(), wtr(lang_, Text::AppTitle).c_str(), type);
    if (rebuilds_.boxClosed()) PostMessageW(hwnd_, kRebuild, 0, 0);
    return answer;
}

void MainWindow::openReports(bool fresh) {
    ReportStart s = startReport_ ? reportStart_ : ReportStart{};
    s.fresh = s.fresh || fresh;
    startReport_ = false;
    reportsOpen_ = true;
    const int unread = showReports(hwnd_, *model_, lang_, dpi_, s);
    reportsOpen_ = false;
    if (unread >= 0) {
        unread_ = unread;
        unreadFromWindow_ = true;
    }
    retextReports();
}

void MainWindow::retextReports() {
    SetWindowTextW(report_, unread_ > 0 ? wtrf(lang_, Text::ReportsButtonUnread, {std::to_string(unread_)}).c_str()
                                        : wtr(lang_, Text::ReportsButton).c_str());
    SendMessageW(report_, WM_SETFONT, reinterpret_cast<WPARAM>(unread_ > 0 && bold_ ? bold_ : font_), TRUE);
}

void MainWindow::showNotes(const std::wstring& text) {
    rebuilds_.boxOpened();  // its own message loop runs like a message box's
    app::showNotes(hwnd_, wtr(lang_, Text::AppTitle), text, font_, dpi_);
    if (rebuilds_.boxClosed()) PostMessageW(hwnd_, kRebuild, 0, 0);
}

void MainWindow::message(const std::wstring& text, bool error) {
    box(text, error ? MB_ICONWARNING : MB_ICONINFORMATION);
}

bool MainWindow::ask(const std::wstring& text) { return box(text, MB_YESNO | MB_ICONQUESTION) == IDYES; }

std::wstring MainWindow::pickFile(bool save) {
    return pickPath(save, wtr(lang_, Text::FileFilterReg), L"*.reg", save ? L"uaro-game-settings.reg" : L"");
}

std::wstring MainWindow::pickPath(bool save, const std::wstring& filterName, const std::wstring& pattern,
                                  const std::wstring& offered) {
    wchar_t file[MAX_PATH] = {};
    offered.copy(file, MAX_PATH - 1);
    std::wstring filter = filterName;
    filter += L'\0';
    filter += pattern;
    filter += L'\0';  // and c_str()'s own: the list ends with two
    const size_t dot = pattern.find(L"*.");
    const std::wstring ext =  // the first pattern's: the name typed without one gets it
        dot == std::wstring::npos ? L"" : pattern.substr(dot + 2, pattern.find(L';', dot) - dot - 2);
    OPENFILENAMEW o{};
    o.lStructSize = sizeof o;
    o.hwndOwner = hwnd_;
    o.lpstrFilter = filter.c_str();
    o.lpstrFile = file;
    o.nMaxFile = MAX_PATH;
    o.lpstrDefExt = ext.empty() ? nullptr : ext.c_str();
    o.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    rebuilds_.boxOpened();  // its own message loop runs like a message box's
    const BOOL ok = save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o);
    if (rebuilds_.boxClosed()) PostMessageW(hwnd_, kRebuild, 0, 0);
    return ok ? std::wstring(file) : std::wstring();
}

std::wstring MainWindow::pickFolder(const std::wstring& title) {
    BROWSEINFOW b{};
    b.hwndOwner = hwnd_;
    b.lpszTitle = title.c_str();
    b.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;  // COM is the thread's since main()
    rebuilds_.boxOpened();
    LPITEMIDLIST id = SHBrowseForFolderW(&b);
    if (rebuilds_.boxClosed()) PostMessageW(hwnd_, kRebuild, 0, 0);
    if (!id) return {};
    wchar_t path[MAX_PATH] = {};
    const bool ok = SHGetPathFromIDListW(id, path) != FALSE;
    CoTaskMemFree(id);
    return ok ? std::wstring(path) : std::wstring();
}

bool MainWindow::confirmClose() { return model_->pendingCount() == 0 || ask(wtr(lang_, Text::AskUnsaved)); }

// 2.6: the window's place for the next start - written when it changed
void MainWindow::savePlace() {
    WINDOWPLACEMENT wp{};
    wp.length = sizeof wp;
    if (!GetWindowPlacement(hwnd_, &wp)) return;
    const RECT& r = wp.rcNormalPosition;
    const bool maximized = wp.showCmd == SW_SHOWMAXIMIZED || (wp.showCmd == SW_SHOWMINIMIZED &&
                                                               (wp.flags & WPF_RESTORETOMAXIMIZED));
    const WindowPlace p{int(r.left), int(r.top), int(r.right), int(r.bottom), maximized};
    const auto saved = model_->savedPlace();
    if (!saved || placeToken(*saved) != placeToken(p)) model_->savePlace(p);
}

int MainWindow::viewWidth() const {
    RECT r;
    GetWindowRect(view_, &r);
    return r.right - r.left;
}

std::wstring MainWindow::windowLetters() const {
    std::wstring taken;
    for (Text t : {Text::Apply, Text::ApplyAndPlay, Text::Revert, Text::Close}) takeMnemonic(wtr(lang_, t), taken);
    return taken;
}

void MainWindow::doApply(bool play) {
    std::wstring err = model_->apply(lang_);
    bool closing = false;
    if (err.empty() && play) {  // 2.11: an offered update first (it starts the game after it), else the game
        // 2.14: the game's new files before either (a failure is told and the game starts as it is; Cancel stops)
        const PatchState patch = clientPatchState();
        if (patch.kind == PatchState::Done && !patch.check.needed.empty()) {
            std::wstring why;
            const auto work = [&](const Report& report, std::wstring& e) {
                std::string error;
                const auto step = [&](uint64_t done, uint64_t total) {
                    Step s;
                    s.done = done;
                    s.total = total;
                    return report(s);
                };
                const bool put = applyClientPatch(model_->paths().dir, patch.check, step, error);
                if (!put) e = wide(error);
                return put;
            };
            const bool put = runWithProgress(hwnd_, wtr(lang_, Text::PatchApplying), lang_, false, work, why);
            if (!put && why == L"stopped") {
                reload();
                return;
            }
            if (!put) message(wtrf(lang_, Text::PatchFailed, {utf8(why)}), true);
            startClientPatchCheck(model_->paths().dir);
        }
        const UpdateState u = updateState();
        err = playNow(*model_, u.kind == UpdateState::Done ? u.offer : "", lang_, closing);
    }
    if (closing) {
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
        return;
    }
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
                int sel = int(SendMessageW(sidebar_, LB_GETCURSEL, 0, 0));
                if (sel >= 0 && size_t(sel) < listItems_.size() && listItems_[size_t(sel)] < 0) {
                    // 2.6: the "Developer only" heading is no entry: on past it the way the arrow went
                    sel += sel > listSel_ ? 1 : -1;
                    if (sel < 0 || size_t(sel) >= listItems_.size()) sel = listSel_;
                    SendMessageW(sidebar_, LB_SETCURSEL, WPARAM(sel), 0);
                }
                listSel_ = sel;
                const int item = sel >= 0 && size_t(sel) < listItems_.size() ? listItems_[size_t(sel)] : -1;
                if (item >= 0 && (settings::View(item) != shown_ || !query_.empty())) {
                    clearSearch();
                    shown_ = settings::View(item);
                    tab_ = 0;
                    scroll_ = 0;  // another page starts at its top
                    buildPage();
                }
            } else if (id >= kModeFirst && id < kModeFirst + kModeCount && code == BN_CLICKED) {
                setMode(Mode(id - kModeFirst));
            } else if (id == kSearch && code == EN_CHANGE) {
                SetTimer(hwnd_, kSearchTimer, 250, nullptr);
            } else if (id == kResetView && code == BN_CLICKED) {
                resetView();
            } else if (id == kStripUpdate && code == BN_CLICKED) {  // 2.10: as the Service page's "Update now"
                if (box(wtr(lang_, Text::UpdateAsk), MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
                Args a;
                a.mode = Args::Mode::Update;
                a.then = Args::Then::App;
                const std::string err = startMode(a);
                if (err.empty())
                    PostMessageW(hwnd_, WM_CLOSE, 0, 0);
                else
                    message(wtrf(lang_, Text::UpdateError, {err}), true);
            } else if (id == kReportButton && code == BN_CLICKED) {
                openReports(false);
            } else if (id >= kTabFirst && id < kTabFirst + tabCount(shown_) && code == BN_CLICKED) {
                if (id - kTabFirst != tab_) {
                    tab_ = id - kTabFirst;
                    scroll_ = 0;
                    rebuild();  // posted: the tab that was clicked is not taken down under its own message
                }
            } else if (id == kLang && (code == CBN_CLOSEUP ||
                                       (code == CBN_SELCHANGE && !SendMessageW(langBox_, CB_GETDROPPEDSTATE, 0, 0)))) {
                const LRESULT sel = SendMessageW(langBox_, CB_GETCURSEL, 0, 0);  // taken when its list is closed
                if (sel >= 0 && sel < kLangCount && Lang(sel) != lang_) {
                    lang_ = Lang(sel);
                    model_->saveLanguage(lang_);
                    startLauncherCheck(lang_);  // 2.11: «Гра»'s news in that language
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
            } else if (id == IDOK && GetFocus() == search_) {  // 2.6: Enter in the search - the first one's page
                const auto hits = findSettings(utf8(searchText()), lang_, mode_);
                if (!hits.empty()) showView(hits.front().place.view, hits.front().place.tab);
            } else if (id == IDCANCEL && GetFocus() == search_ && GetWindowTextLengthW(search_) > 0) {
                clearSearch();  // Esc in the search: the view back
                buildPage();
            } else if (id == IDOK) {  // Enter: a number field takes what was typed
                HWND focus = GetFocus();
                const int focusId = focus ? GetDlgCtrlID(focus) : 0;
                if (focusId >= 1000 && current_) {
                    typing_ = false;
                    current_->command(focusId, EN_KILLFOCUS);
                    updateStatus();
                }
            } else if (id == IDCANCEL) {
                PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            } else if (id >= 1000 && current_ && !rebuilds_.quiet()) {
                // A number being typed counts for Apply, which takes it first: the field loses the focus to the
                // button and takes the number (0.6.0 review M5).
                if (code == EN_CHANGE || code == EN_KILLFOCUS) typing_ = code == EN_CHANGE;
                current_->command(id, code);
                if (code == EN_CHANGE || code == EN_KILLFOCUS) updateStatus();
            }
            return 0;
        }
        case WM_HSCROLL:
            if (lp && current_ && !rebuilds_.quiet()) {
                HWND ctl = reinterpret_cast<HWND>(lp);
                current_->scroll(GetDlgCtrlID(ctl), LOWORD(wp), int(SendMessageW(ctl, TBM_GETPOS, 0, 0)));
            }
            return 0;
        case WM_NOTIFY: {
            const auto* n = reinterpret_cast<const NMHDR*>(lp);
            if (n && n->idFrom >= 1000 && current_ && !rebuilds_.quiet()) return current_->notify(n);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wp);
            HWND ctl = reinterpret_cast<HWND>(lp);
            Tone tone = Tone::Normal;
            if (ctl == stripLeft_ || ctl == stripRight_) {  // 2.6: the status bar's ground
                SetTextColor(dc, RGB(85, 85, 85));
                SetBkMode(dc, TRANSPARENT);
                return reinterpret_cast<LRESULT>(stripBrush_);
            }
            if (ctl == banner_)
                SetTextColor(dc, toneColor(Tone::Bad));
            else if (panel_ && panel_->toneOf(ctl, tone))
                SetTextColor(dc, toneColor(tone));
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        }
        case kShowPage:
            if (int(wp) < settings::kViewCount) {
                const auto view = settings::View(wp);
                const int tab = std::clamp(int(lp), 0, tabCount(view) - 1);
                if (view != shown_ || tab != tab_ || !query_.empty()) {
                    clearSearch();
                    shown_ = view;
                    tab_ = tab;
                    scroll_ = 0;
                    fillList();  // chosen in the list, or none when the mode does not list it
                    buildPage();
                }
            }
            return 0;
        case WM_MEASUREITEM:  // 2.6: the list's entries
            if (wp == kSidebar) {
                reinterpret_cast<MEASUREITEMSTRUCT*>(lp)->itemHeight = UINT(px(32, dpi_));
                return TRUE;
            }
            break;
        case WM_DRAWITEM:
            if (wp == kSidebar) {
                drawListItem(*reinterpret_cast<const DRAWITEMSTRUCT*>(lp));
                return TRUE;
            }
            break;
        case WM_CTLCOLORLISTBOX:
            if (reinterpret_cast<HWND>(lp) == sidebar_) return reinterpret_cast<LRESULT>(listBrush_);
            break;
        case WM_PAINT:
            paint();
            return 0;
        case kRebuild:
            if (rebuilds_.arrived()) buildPage();
            return 0;
        case kOpenReports:  // 2.13: --report; a box over the window: the timer opens it after
            if (startReport_ && !reportsOpen_ && IsWindowEnabled(hwnd_)) openReports(true);
            return 0;
        case WM_COPYDATA: {  // 2.13: a second app's --report
            const COPYDATASTRUCT* cd = reinterpret_cast<const COPYDATASTRUCT*>(lp);
            if (!cd || cd->dwData != kReportData || reportsOpen_) return FALSE;
            const std::string text(static_cast<const char*>(cd->lpData), cd->cbData);
            const size_t nl = text.find('\n');
            reportStart_ = ReportStart{};
            reportStart_.fresh = true;
            reportStart_.shot = wide(text.substr(0, nl));
            reportStart_.map = nl == std::string::npos ? "" : text.substr(nl + 1);
            startReport_ = true;
            PostMessageW(hwnd_, kOpenReports, 0, 0);
            return TRUE;
        }
        case kAskMachine: {  // 2.12: the settings picked again for the new card or screen, if the player wants
            const settings::Machine x = model_->machine();
            const std::string screen = std::to_string(x.screenWidth) + "x" + std::to_string(x.screenHeight);
            if (box(wtrf(lang_, Text::MachineChanged, {x.adapter, screen}), MB_YESNO | MB_ICONQUESTION) == IDYES) {
                bool uiChanged = false;
                model_->pendAutoSetup(uiChanged);
                const std::wstring err = model_->apply(lang_);
                if (!err.empty()) message(err, true);
            }
            model_->recordMachine(lang_);
            reload();
            return 0;
        }
        case kAskStats: {  // 2.2.1: asked once; then the day's statistics go in the background (not while it plays)
            if (!model_->statsAsked()) {
                const bool yes = box(wtr(lang_, Text::StatsAsk), MB_YESNO | MB_ICONQUESTION) == IDYES;
                const std::wstring err = model_->setStats(yes, lang_);
                if (!err.empty()) message(err, true);
                rebuild();
            }
            if (model_->seenVersion() != kVersion) {  // 2.6: a new version's notes, once (not on a new install)
                const std::wstring notes = newInstall_ ? std::wstring() : notesText(model_->paths().dir, lang_);
                if (!notes.empty()) showNotes(notes);
                model_->saveSeenVersion(kVersion);
            }
            if (model_->now().config.statsSend && !model_->now().gameRunning && !statsThread_) {
                auto* dir = new std::wstring(model_->paths().dir);
                statsThread_ = CreateThread(nullptr, 0, &sendStats, dir, 0, nullptr);
                if (!statsThread_) delete dir;
            }
            return 0;
        }
        case WM_TIMER:
            if (wp == kSearchTimer) {  // 2.6: the typing stopped - the search's page, or the view back
                KillTimer(hwnd_, kSearchTimer);
                const std::wstring text = searchText();
                if (text != query_) {
                    query_ = text;
                    scroll_ = 0;
                    buildPage();
                }
                return 0;
            }
            if (wp == kTimer) {
                if (model_->refreshRunning()) rebuild();  // the game started or closed
                updateStrip();  // the update's check, the pack's builder
                if (!unreadFromWindow_ && reportsUnread() != unread_) {  // 2.13: the start's check came back
                    unread_ = reportsUnread();
                    retextReports();
                }
                if (startReport_ && !reportsOpen_ && IsWindowEnabled(hwnd_)) openReports(true);  // 2.13: --report
            }
            if (wp == kTickTimer && current_) current_->tick();
            return 0;
        case WM_DPICHANGED: {
            dpi_ = HIWORD(wp);
            makeFonts();
            const RECT* r = reinterpret_cast<const RECT*>(lp);
            SetWindowPos(hwnd_, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            SendMessageW(tooltip_, TTM_SETMAXTIPWIDTH, 0, px(380, dpi_));
            layout();
            rebuild();  // posted: a page's message box may be open
            return 0;
        }
        case WM_NEXTDLGCTL: {
            // How IsDialogMessage moves the focus for Alt+a label's letter - a dialog's own work, which this window
            // does itself (0.6.1 review): lp set, wp is the control; else the next (wp 0) or the previous tab stop.
            HWND to = LOWORD(lp) ? reinterpret_cast<HWND>(wp) : GetNextDlgTabItem(hwnd_, GetFocus(), wp != 0);
            if (to) {
                SetFocus(to);
                if (SendMessageW(to, WM_GETDLGCODE, 0, 0) & DLGC_HASSETSEL) SendMessageW(to, EM_SETSEL, 0, -1);
            }
            return 0;
        }
        case WM_GETMINMAXINFO: {  // 2.6: 880 x 600 at the least
            auto* mm = reinterpret_cast<MINMAXINFO*>(lp);
            RECT r{0, 0, px(kWidth, dpi_), px(kHeight, dpi_)};
            AdjustWindowRectEx(&r, DWORD(GetWindowLongPtrW(hwnd_, GWL_STYLE)), FALSE, 0);
            mm->ptMinTrackSize = POINT{r.right - r.left, r.bottom - r.top};
            return 0;
        }
        case WM_SIZE:  // 2.6: the window grows - its parts moved, the page made again at the new width
            if (view_ && wp != SIZE_MINIMIZED) {
                layout();
                updateScrollBar();
                scrollTo(scroll_);
                // maximized, restored; not before the first page (create() makes it at the window's size)
                if (!sizing_ && current_ && viewWidth() != builtWidth_) rebuild();
            }
            return 0;
        case WM_ENTERSIZEMOVE:
            sizing_ = true;
            return 0;
        case WM_EXITSIZEMOVE:  // once the frame is let go: the controls are made at most once a drag
            sizing_ = false;
            if (view_ && viewWidth() != builtWidth_) rebuild();
            return 0;
        case WM_CLOSE:
            if (confirmClose()) {
                savePlace();
                DestroyWindow(hwnd_);
            }
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd_, kTimer);
            KillTimer(hwnd_, kTickTimer);
            if (statsThread_) {  // 2.2.2: a send under way finishes (its files whole), at most 3 s more
                WaitForSingleObject(statsThread_, 3000);
                CloseHandle(statsThread_);
                statsThread_ = nullptr;
            }
            current_.reset();
            panel_.reset();
            for (HBRUSH b : {listBrush_, lineBrush_, chosenBrush_, accentBrush_, stripBrush_})
                if (b) DeleteObject(b);
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

}  // namespace uagfx::app
