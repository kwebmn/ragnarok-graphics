#include "win/window_panel.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/layout.hpp"
#include "core/settings/strings.hpp"
#include "win/code_page.hpp"
#include "win/dpi.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/real_ddraw.hpp"

namespace uagfx::win {
namespace {

struct Words {
    panel::Box box;
    std::wstring text;
    uint32_t rgb = panel::kText;
    float px = 12;
    bool bold = false;
    UINT align = DT_LEFT;
};

// The words, white on black by GDI into a picture of the panel's size: each one's coverage in its own colour over
// the panel's shapes (the words' boxes do not overlap)
void drawWords(panel::Picture& pic, const std::vector<Words>& words) {
    if (pic.width <= 0 || pic.height <= 0) return;
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = pic.width;
    bi.bmiHeader.biHeight = -pic.height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bmp && bits) {
        HGDIOBJ oldBmp = SelectObject(dc, bmp);
        std::memset(bits, 0, size_t(pic.width) * size_t(pic.height) * 4);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        for (const Words& w : words) {
            if (w.text.empty() || w.box.w <= 0) continue;
            HFONT font = CreateFontW(-std::max(8, int(std::lround(w.px))), 0, 0, 0, w.bold ? FW_SEMIBOLD : FW_NORMAL,
                                     FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            HGDIOBJ oldFont = SelectObject(dc, font);
            RECT r{LONG(std::lround(w.box.x)), LONG(std::lround(w.box.y)), LONG(std::lround(w.box.x + w.box.w)),
                   LONG(std::lround(w.box.y + w.box.h))};
            DrawTextW(dc, w.text.c_str(), int(w.text.size()), &r,
                      DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS | w.align);
            SelectObject(dc, oldFont);
            DeleteObject(font);
        }
        GdiFlush();
        const auto* cover = static_cast<const uint32_t*>(bits);
        for (const Words& w : words) {
            const int x0 = std::max(0, int(w.box.x)), y0 = std::max(0, int(w.box.y));
            const int x1 = std::min(pic.width, int(std::ceil(w.box.x + w.box.w)));
            const int y1 = std::min(pic.height, int(std::ceil(w.box.y + w.box.h)));
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x) {
                    const size_t i = size_t(y) * size_t(pic.width) + size_t(x);
                    const uint32_t c = cover[i];
                    const uint32_t v = std::max({c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF});
                    if (v) panel::over(pic.px[i], w.rgb, float(v) / 255.0f);
                }
        }
        SelectObject(dc, oldBmp);
    }
    if (bmp) DeleteObject(bmp);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
}

std::wstring utf8Wide(const std::string& s) { return widened(s, CP_UTF8); }

std::wstring levelsOf(const gamewindows::Window& w) {
    if (w.baseLevel <= 0) return L"";
    return std::to_wstring(w.baseLevel) + (w.jobLevel > 0 ? L"/" + std::to_wstring(w.jobLevel) : L"");
}

}  // namespace

WindowPanel::~WindowPanel() {
    if (window_) DestroyWindow(window_);
}

bool WindowPanel::create(HWND game) {
    static ATOM atom = 0;
    if (!atom) {
        WNDCLASSEXA c{};
        c.cbSize = sizeof c;
        c.lpfnWndProc = &WindowPanel::proc;
        c.hInstance = GetModuleHandleA(nullptr);
        c.hCursor = LoadCursor(nullptr, IDC_ARROW);
        c.lpszClassName = panel::kWindowClass;
        atom = RegisterClassExA(&c);
        if (!atom) return false;
    }
    window_ = CreateWindowExA(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, panel::kWindowClass, "", WS_POPUP,
                              0, 0, 1, 1, game, nullptr, GetModuleHandleA(nullptr), nullptr);
    if (!window_) return false;
    SetWindowLongPtrA(window_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    return true;
}

void WindowPanel::update(const std::vector<gamewindows::Window>& all, uint32_t selfPid) {
    all_ = all;
    self_ = selfPid;
    std::string data;  // what the cards say: drawn again only when it changes
    for (const gamewindows::Window& w : all_)
        data += std::to_string(w.number) + '|' + w.name + '|' + std::to_string(w.baseLevel) + '/' +
                std::to_string(w.jobLevel) + '|' + std::to_string(w.hp) + '|' + std::to_string(w.sp) + '|' +
                (w.pid == self_ ? "*" : "") + ';';
    if (data != shownData_) {
        shownData_ = data;
        dirty_ = true;
    }
}

void WindowPanel::frame(HWND game, bool main, Config& config, int gameWidth) {
    config_ = &config;
    if (!game) return;
    // In the screen's real pixels: the panel's window made aware of them, sharp under Windows' scaling (uaRO.exe is
    // not), and the game window measured in them; its own messages come in them by themselves
    const RealPixels real;
    game_ = game;
    if (seenSetting_ && !panelOn_ && config.windowPanel && config.windowPanelHidden)
        hide(false);  // switched on in the settings: shown again, whatever its cross did
    panelOn_ = config.windowPanel;
    seenSetting_ = true;
    const bool want = config.windowPanel && !config.windowPanelHidden && main && all_.size() >= 2 &&
                      IsWindowVisible(game) && !IsIconic(game);
    if (!want) {
        if (dragging_) ReleaseCapture();  // gone from under the hand (another window in front): the drag is over
        dragging_ = false;
        if (shown_ && window_) ShowWindow(window_, SW_HIDE);
        shown_ = false;
        return;
    }
    if (!window_ && !create(game)) return;
    RECT client{};
    GetClientRect(game, &client);
    POINT origin{0, 0};
    ClientToScreen(game, &origin);
    const float interface = gameWidth > 0 && client.right > 0 ? float(client.right) / float(gameWidth) : 1.0f;
    const float scale = std::clamp(interface * float(config.windowPanelSize) / 100.0f, 0.75f, 4.0f);
    const panel::Layout layout =
        config.windowPanelLayout == PanelLayout::Column ? panel::Layout::Column : panel::Layout::Row;
    bool move = origin.x != origin_.x || origin.y != origin_.y;
    if (scale != scale_ || layout != layout_ || client.right != roomW_ || client.bottom != roomH_) {
        scale_ = scale;
        layout_ = layout;
        roomW_ = client.right;
        roomH_ = client.bottom;
        dirty_ = move = true;
    }
    if (config.windowPanelPlace != placeText_) {  // dragged here or in another game window
        placeText_ = config.windowPanelPlace;
        place_ = panel::parsePlace(placeText_);
        move = true;
    }
    origin_ = origin;
    if (dirty_) redraw();
    if (move && !dragging_) {
        const POINT at = target();
        SetWindowPos(window_, nullptr, at.x, at.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (!shown_) {
        ShowWindow(window_, SW_SHOWNOACTIVATE);
        shown_ = true;
        if (!everShown_)
            LOG_INFO("windows: the panel shown - %d game windows, %s, scale %.2f, at %s", int(all_.size()),
                     panelLayoutToken(config.windowPanelLayout), double(scale_),
                     placeText_.empty() ? "its first place" : placeText_.c_str());
        everShown_ = true;
    }
}

void WindowPanel::follow() {
    if (!shown_ || !window_ || !game_ || dragging_) return;
    const RealPixels real;
    POINT origin{0, 0};
    ClientToScreen(game_, &origin);
    if (origin.x == origin_.x && origin.y == origin_.y) return;
    origin_ = origin;
    const POINT at = target();
    SetWindowPos(window_, nullptr, at.x, at.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

POINT WindowPanel::target() const {
    const panel::Point p =
        panel::position(place_, geometry_.width, geometry_.height, roomW_, roomH_, geometry_.scale);
    return POINT{origin_.x + p.x, origin_.y + p.y};
}

void WindowPanel::redraw() {
    geometry_ = panel::geometry(int(all_.size()), layout_, scale_, roomW_);
    std::vector<panel::Card> cards;
    std::vector<Words> words;
    const settings::Lang lang = startup().lang;
    const float s = geometry_.scale;
    words.push_back(Words{geometry_.title, utf8Wide(settings::tr(lang, settings::Text::PanelTitle)), panel::kMuted,
                          11 * s, false, DT_LEFT});
    for (size_t i = 0; i < all_.size() && i < geometry_.cards.size(); ++i) {
        const gamewindows::Window& w = all_[i];
        const panel::CardBoxes& b = geometry_.cards[i];
        panel::Card c;
        c.current = w.pid == self_;
        c.hover = int(i) == hover_ && !c.current;
        c.hp = w.hp;
        c.sp = w.sp;
        cards.push_back(c);
        words.push_back(Words{panel::Box{b.badgeX - b.badgeR, b.badgeY - b.badgeR, 2 * b.badgeR, 2 * b.badgeR},
                              std::to_wstring(w.number), c.current ? panel::kOnBadge : panel::kText, 10.5f * s, true,
                              DT_CENTER});
        const bool named = !w.name.empty();
        const std::string unnamed =  // the login, the character select, another build: «window 2»
            settings::fmt(settings::tr(lang, settings::Text::PanelWindow), {std::to_string(w.number)});
        const std::wstring name = utf8Wide(named ? w.name : unnamed);
        words.push_back(Words{b.name, name, named ? panel::kText : panel::kMuted, 13 * s, c.current, DT_LEFT});
        if (b.levels.w > 0) words.push_back(Words{b.levels, levelsOf(w), panel::kMuted, 11 * s, false, DT_RIGHT});
    }
    panel::Picture pic;
    panel::paint(pic, geometry_, cards, hoverClose_);
    drawWords(pic, words);
    dirty_ = false;
    if (!window_ || pic.width <= 0 || pic.height <= 0) return;
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = pic.width;
    bi.bmiHeader.biHeight = -pic.height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bmp && bits) {
        HGDIOBJ old = SelectObject(dc, bmp);
        std::memcpy(bits, pic.px.data(), pic.px.size() * 4);
        SIZE size{pic.width, pic.height};
        POINT from{0, 0};
        POINT at = target();
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        // dragged now: where the hand has it; else at its place
        UpdateLayeredWindow(window_, screen, dragging_ ? nullptr : &at, &size, dc, &from, 0, &blend, ULW_ALPHA);
        SelectObject(dc, old);
    }
    if (bmp) DeleteObject(bmp);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
}

void WindowPanel::hide(bool hidden) {
    if (!config_) return;
    config_->windowPanelHidden = hidden;  // what the file says now: not taken for a change of it
    editIniFile(ourFile(layout::File::Ini), {{"windows", "panel_hidden", hidden ? "1" : "0"}});
    LOG_INFO("windows: the panel %s", hidden ? "hidden (its cross or Ctrl+Alt+0; Ctrl+Alt+0 shows it again)" : "shown");
}

void WindowPanel::toggle() {
    if (config_) hide(!config_->windowPanelHidden);
}

void WindowPanel::keepPlace() {
    RECT r{};
    if (!window_ || !GetWindowRect(window_, &r) || !config_) return;
    place_ = panel::placeOf(panel::Point{r.left - origin_.x, r.top - origin_.y}, geometry_.width, geometry_.height,
                            roomW_, roomH_, geometry_.scale);
    placeText_ = panel::formatPlace(place_);
    config_->windowPanelPlace = placeText_;
    if (editIniFile(ourFile(layout::File::Ini), {{"windows", "panel_place", placeText_}}))
        LOG_INFO("windows: the panel dragged to %s (of the room beside it)", placeText_.c_str());
}

LRESULT CALLBACK WindowPanel::proc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    auto* self = reinterpret_cast<WindowPanel*>(GetWindowLongPtrA(w, GWLP_USERDATA));
    return self && self->window_ == w ? self->message(m, wp, lp) : DefWindowProcA(w, m, wp, lp);
}

LRESULT WindowPanel::message(UINT m, WPARAM wp, LPARAM lp) {
    const auto hitAt = [&](LPARAM at) {
        return panel::hitTest(geometry_, float(GET_X_LPARAM(at)), float(GET_Y_LPARAM(at)));
    };
    switch (m) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;  // the game stays the window in front
        case WM_SETCURSOR: {
            POINT p{};
            GetCursorPos(&p);
            ScreenToClient(window_, &p);
            const panel::Part part = panel::hitTest(geometry_, float(p.x), float(p.y)).part;
            SetCursor(LoadCursor(nullptr, part == panel::Part::Top    ? IDC_SIZEALL
                                          : part == panel::Part::None ? IDC_ARROW
                                                                      : IDC_HAND));
            return TRUE;
        }
        case WM_MOUSEMOVE: {
            if (dragging_) {
                POINT c{};
                GetCursorPos(&c);
                const LONG right = origin_.x + std::max(0, roomW_ - geometry_.width);  // within the game window
                const LONG bottom = origin_.y + std::max(0, roomH_ - geometry_.height);
                const LONG x = std::clamp(dragStart_.x + c.x - dragFrom_.x, origin_.x, right);
                const LONG y = std::clamp(dragStart_.y + c.y - dragFrom_.y, origin_.y, bottom);
                SetWindowPos(window_, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                return 0;
            }
            if (!tracking_) {
                TRACKMOUSEEVENT t{sizeof t, TME_LEAVE, window_, 0};
                tracking_ = TrackMouseEvent(&t) != FALSE;
            }
            const panel::Hit h = hitAt(lp);
            const int hover = h.part == panel::Part::Card ? h.card : -1;
            const bool close = h.part == panel::Part::Close;
            if (hover != hover_ || close != hoverClose_) {
                hover_ = hover;
                hoverClose_ = close;
                redraw();
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            tracking_ = false;
            if (hover_ != -1 || hoverClose_) {
                hover_ = -1;
                hoverClose_ = false;
                redraw();
            }
            return 0;
        case WM_LBUTTONDOWN: {
            const panel::Hit h = hitAt(lp);
            if (h.part == panel::Part::Top) {
                RECT r{};
                GetWindowRect(window_, &r);
                GetCursorPos(&dragFrom_);
                dragStart_ = POINT{r.left, r.top};
                dragging_ = true;
                SetCapture(window_);
            }
            pressed_ = h.part == panel::Part::Close ? -1 : h.part == panel::Part::Card ? h.card : -2;
            return 0;
        }
        case WM_LBUTTONUP: {
            if (dragging_) {
                dragging_ = false;
                ReleaseCapture();
                keepPlace();
                return 0;
            }
            const panel::Hit h = hitAt(lp);
            const int part = h.part == panel::Part::Close ? -1 : h.part == panel::Part::Card ? h.card : -2;
            const int pressed = pressed_;
            pressed_ = -2;
            if (part != pressed || part == -2) return 0;
            if (part == -1) {
                hide(true);
            } else if (size_t(part) < all_.size() && all_[size_t(part)].pid != self_ && onPick) {
                onPick(all_[size_t(part)].number);
            }
            return 0;
        }
        case WM_CAPTURECHANGED:
            if (dragging_) {
                dragging_ = false;
                keepPlace();
            }
            return 0;
        default:
            return DefWindowProcA(window_, m, wp, lp);
    }
}

}  // namespace uagfx::win
