#include "win/window_panel.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/d3d7.hpp"
#include "core/effect_exe.hpp"
#include "core/glint.hpp"
#include "core/layout.hpp"
#include "core/settings/strings.hpp"
#include "core/states.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/gpu/renderer.hpp"
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

bool sameRows(const std::vector<panel::MenuRow>& a, const std::vector<panel::MenuRow>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].kind != b[i].kind || a[i].segments != b[i].segments || a[i].chosen != b[i].chosen ||
            a[i].on != b[i].on)
            return false;
    return true;
}

// A picture as it is into the frame: its texture made for its size, uploaded when it changed, one quad with its top
// left at (x, y) in the target's pixels over the game's interface. The commands added (0: no texture)
size_t drawPicture(FrameRecord& record, gpu::Renderer& renderer, const panel::Picture& pic, uint32_t& texture,
                   int& texW, int& texH, bool& upload, int x, int y, float k) {
    if (!texture || texW != pic.width || texH != pic.height) {
        if (texture) renderer.releaseTexture(texture);
        texture = renderer.createTexture(pic.width, pic.height, 1);
        texW = pic.width;
        texH = pic.height;
        upload = true;
        if (!texture) return 0;
    }
    size_t added = 1;
    if (upload) {
        record.upload(texture, 0, Rect{0, 0, texW, texH}, pic.px.data());
        upload = false;
        ++added;
    }
    // Premultiplied, no depth, a texel a target pixel
    DrawSetup s;
    s.ui = true;
    s.blend = true;
    s.srcBlend = d3d7::BLEND_ONE;
    s.dstBlend = d3d7::BLEND_INVSRCALPHA;
    s.stage[0].colorOp = s.stage[0].alphaOp = d3d7::TOP_SELECTARG1;
    s.stage[0].colorArg1 = s.stage[0].alphaArg1 = d3d7::TA_TEXTURE;
    s.stage[0].addressU = s.stage[0].addressV = d3d7::TADDRESS_CLAMP;
    s.stage[0].magFilter = d3d7::TFG_POINT;
    s.stage[0].minFilter = d3d7::TFN_POINT;
    s.stage[1].colorOp = s.stage[1].alphaOp = d3d7::TOP_DISABLE;
    DrawCmd cmd;
    cmd.state = record.addState(s);
    cmd.texture[0] = texture;
    cmd.effect = uint8_t(effect_exe::Kind::Panel);  // sampled as it is (draw.cpp, renderer11.cpp)
    cmd.primitive = d3d7::PT_TRIANGLELIST;
    cmd.fvf = d3d7::FVF_TL1;
    // a game x lands at x * k + 0.5 in the target (shaders.cpp draw_vs): the quad's edges on the target's pixel edges
    const float x0 = (float(x) - 0.5f) / k, y0 = (float(y) - 0.5f) / k;
    const float x1 = (float(x + texW) - 0.5f) / k, y1 = (float(y + texH) - 0.5f) / k;
    const uint32_t white = 0xFFFFFFFFu;
    const glint::Vertex v[6] = {{x0, y0, 0, 1, white, 0, 0, 0}, {x1, y0, 0, 1, white, 0, 1, 0},
                                {x0, y1, 0, 1, white, 0, 0, 1}, {x1, y0, 0, 1, white, 0, 1, 0},
                                {x1, y1, 0, 1, white, 0, 1, 1}, {x0, y1, 0, 1, white, 0, 0, 1}};
    record.draw(cmd, v, 6, nullptr, 0);
    return added;
}

}  // namespace

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

void WindowPanel::frame(HWND game, bool main, Config& config, double k, int targetWidth, int targetHeight) {
    config_ = &config;
    if (seenSetting_ && !panelOn_ && config.panelShow && config.panelHidden)
        hide(false);  // switched on in the settings or the console: shown again, whatever its cross did
    panelOn_ = config.panelShow;
    seenSetting_ = true;
    const bool want = game && config.panelShow && !config.panelHidden && main && all_.size() >= 2 && k > 0 &&
                      targetWidth > 0 && targetHeight > 0;
    if (!want) {
        if (shown_) {
            if (dragging_ && GetCapture() == game) ReleaseCapture();
            dragging_ = false;
            uapad::gamewindow::setBlocked(nullptr, false);
        }
        shown_ = false;
        closeMenu();
        return;
    }
    k_ = float(k);
    const float scale = std::clamp(k_ * float(config.panelSize) / 100.0f, 0.75f, 6.0f);
    const panel::Layout layout =
        config.panelLayout == PanelLayout::Column ? panel::Layout::Column : panel::Layout::Row;
    bool move = !shown_;
    if (scale != scale_ || layout != layout_ || targetWidth != targetW_ || targetHeight != targetH_ ||
        config.panelOpacity != opacity_) {
        scale_ = scale;
        layout_ = layout;
        targetW_ = targetWidth;
        targetH_ = targetHeight;
        opacity_ = config.panelOpacity;
        dirty_ = move = true;
    }
    if (config.panelPlace != placeText_) {  // dragged here or in another game window
        placeText_ = config.panelPlace;
        place_ = panel::parsePlace(placeText_);
        move = true;
    }
    if (dirty_) {
        const int w = geometry_.width, h = geometry_.height;
        render();
        move = move || geometry_.width != w || geometry_.height != h;  // a card more or less: kept at its edge
    }
    if (move && !dragging_) {
        const panel::Point p =
            panel::position(place_, geometry_.width, geometry_.height, targetW_, targetH_, geometry_.scale);
        x_ = p.x;
        y_ = p.y;
    }
    if (!everShown_)
        LOG_INFO("windows: the panel shown in the game's picture - %d game windows, %s, scale %.2f, at %s",
                 int(all_.size()), panelLayoutToken(config.panelLayout), double(scale_),
                 placeText_.empty() ? "its first place" : placeText_.c_str());
    shown_ = everShown_ = true;
    if (menuOpen_ && GetForegroundWindow() != game) closeMenu();  // the window behind another one: as Windows' menus
    if (menuOpen_) {  // its rows as the settings are now (changed by it, the settings, the console, another window)
        std::vector<panel::MenuRow> rows = panel::menuRows(config);
        const float menuScale = std::clamp(k_, 0.75f, 6.0f);
        if (!sameRows(rows, menuRows_) || menuScale != menuScale_) {
            menuRows_ = std::move(rows);
            menuScale_ = menuScale;
            menuDirty_ = true;
        }
        if (menuDirty_) renderMenu();
    }
    block(game);
}

void WindowPanel::render() {
    geometry_ = panel::geometry(int(all_.size()), layout_, scale_, targetW_);
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
    panel::paint(picture_, geometry_, cards, hoverPart_, layout_);
    drawWords(picture_, words);
    panel::fade(picture_, float(opacity_) / 100.0f);
    dirty_ = false;
    upload_ = true;
}

void WindowPanel::draw(FrameRecord& record, gpu::Renderer& renderer) {
    if (!shown_ || picture_.width <= 0 || picture_.height <= 0 || k_ <= 0) return;
    // 3.0.3-beta.9 (the user: the mouse over the panel «меняет цвет»): under the game's own cursor - found among the
    // frame's last draws, the panel's commands go before it - so that cursor stays over it, not Windows' arrow
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    size_t added = drawPicture(record, renderer, picture_, texture_, texW_, texH_, upload_, x_, y_, k_);
    if (menuOpen_ && menuPicture_.width > 0 && menuPicture_.height > 0)  // 3.0.3-beta.10: its menu over it
        added += drawPicture(record, renderer, menuPicture_, menuTexture_, menuTexW_, menuTexH_, menuUpload_, menuX_,
                             menuY_, k_);
    if (cursor >= 0 && added) record.moveLastBefore(added, size_t(cursor));
    underCursor_ = cursor >= 0;
}

bool WindowPanel::toTarget(HWND game, int x, int y, float& tx, float& ty) const {
    RECT client{};
    if (!GetClientRect(game, &client) || client.right <= 0 || client.bottom <= 0) return false;
    tx = float(x) * float(targetW_) / float(client.right);  // Windows' size of the window (uaRO.exe is not DPI-aware)
    ty = float(y) * float(targetH_) / float(client.bottom);
    return true;
}

panel::Hit WindowPanel::hitAt(float tx, float ty) const {
    return panel::hitTest(geometry_, tx - float(x_), ty - float(y_));
}

void WindowPanel::block(HWND game) {
    RECT client{};
    if (!GetClientRect(game, &client) || targetW_ <= 0 || targetH_ <= 0) return;
    const RECT r{LONG(int64_t(x_) * client.right / targetW_), LONG(int64_t(y_) * client.bottom / targetH_),
                 LONG(int64_t(x_ + geometry_.width) * client.right / targetW_ + 1),
                 LONG(int64_t(y_ + geometry_.height) * client.bottom / targetH_ + 1)};
    uapad::gamewindow::setBlocked(&r, dragging_ || menuOpen_);  // its menu open: every click is its or closes it
}

bool WindowPanel::mouse(HWND game, UINT m, LPARAM lp, LRESULT& result) {
    if (!shown_) return false;
    float tx = 0, ty = 0;
    const auto onMenu = [&] {
        return menuOpen_ && tx >= float(menuX_) && ty >= float(menuY_) && tx < float(menuX_ + menuGeometry_.width) &&
               ty < float(menuY_ + menuGeometry_.height);
    };
    if (m == WM_SETCURSOR) {  // over it Windows' arrow when the game's own cursor was not found over the panel
        if (LOWORD(lp) != HTCLIENT || underCursor_) return false;
        POINT p{};
        GetCursorPos(&p);
        ScreenToClient(game, &p);
        if (!toTarget(game, p.x, p.y, tx, ty)) return false;
        const panel::Hit h = hitAt(tx, ty);
        const bool over = tx >= float(x_) && ty >= float(y_) && tx < float(x_ + geometry_.width) &&
                          ty < float(y_ + geometry_.height);
        const bool menu = onMenu();
        if (!over && !menu && !dragging_) return false;
        SetCursor(LoadCursor(nullptr, menu                                     ? IDC_ARROW
                                      : dragging_ || h.part == panel::Part::Top ? IDC_SIZEALL
                                      : h.part == panel::Part::None             ? IDC_ARROW
                                                                                : IDC_HAND));
        result = TRUE;
        return true;
    }
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if ((m != WM_MOUSEMOVE && !button) || !toTarget(game, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), tx, ty)) return false;
    const bool over = tx >= float(x_) && ty >= float(y_) && tx < float(x_ + geometry_.width) &&
                      ty < float(y_ + geometry_.height);
    result = 0;
    const bool up = m == WM_LBUTTONUP || m == WM_RBUTTONUP || m == WM_MBUTTONUP;
    const uint8_t bit = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK   ? 1
                        : m == WM_RBUTTONDOWN || m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK ? 2
                        : m == WM_MOUSEMOVE                                                 ? 0
                                                                                            : 4;
    if (up && (swallowed_ & bit)) {  // the let-go of a press that closed the menu: nobody's either
        swallowed_ = uint8_t(swallowed_ & ~bit);
        return true;
    }
    if (menuOpen_) {  // 3.0.3-beta.10: its menu open - the mouse is the menu's
        const bool menu = onMenu();
        const panel::MenuHit h =
            menu ? panel::menuHit(menuGeometry_, tx - float(menuX_), ty - float(menuY_)) : panel::MenuHit{};
        if (m == WM_MOUSEMOVE) {
            if (h.row != menuHover_.row || h.segment != menuHover_.segment) {
                menuHover_ = h;
                menuDirty_ = true;
            }
            return false;  // the game's cursor still moves (drawn over the menu)
        }
        if (up) {  // a row picked: the left button pressed and let go on it
            if (menu && m == WM_LBUTTONUP && h.row >= 0 && h.row == menuPressed_.row &&
                h.segment == menuPressed_.segment)
                pick(h);
            if (m == WM_LBUTTONUP) menuPressed_ = panel::MenuHit{};
            return true;
        }
        if (menu) {
            if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) menuPressed_ = h;
            return true;
        }
        // pressed off it: closed, the press nobody's - the right button on the panel opens it there again (let go)
        closeMenu();
        if (m == WM_RBUTTONDOWN && over)
            rightOnPanel_ = true;
        else
            swallowed_ = uint8_t(swallowed_ | bit);
        block(game);
        return true;
    }
    if (m == WM_MOUSEMOVE) {
        if (dragging_) {
            x_ = std::clamp(dragStartX_ + int(std::lround(tx - dragFromX_)), 0,
                            std::max(0, targetW_ - geometry_.width));
            y_ = std::clamp(dragStartY_ + int(std::lround(ty - dragFromY_)), 0,
                            std::max(0, targetH_ - geometry_.height));
            block(game);
            return true;
        }
        const panel::Hit h = over ? hitAt(tx, ty) : panel::Hit{};
        const int hover = h.part == panel::Part::Card ? h.card : -1;
        const panel::Part part =
            h.part == panel::Part::Close || h.part == panel::Part::Layout ? h.part : panel::Part::None;
        if (hover != hover_ || part != hoverPart_) {
            hover_ = hover;
            hoverPart_ = part;
            dirty_ = true;
        }
        return false;  // the game's cursor still moves (drawn under the panel)
    }
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) {
        if (!over) return false;
        const panel::Hit h = hitAt(tx, ty);
        if (h.part == panel::Part::Top) {
            dragging_ = true;
            dragFromX_ = tx;
            dragFromY_ = ty;
            dragStartX_ = x_;
            dragStartY_ = y_;
            SetCapture(game);
            block(game);
        }
        pressed_ = h;
        return true;
    }
    if (m == WM_LBUTTONUP) {
        if (dragging_) {
            dragging_ = false;
            if (GetCapture() == game) ReleaseCapture();
            keepPlace();
            block(game);
            return true;
        }
        const panel::Hit was = pressed_;
        pressed_ = panel::Hit{};
        if (!over) return was.part != panel::Part::None;  // pressed on it, let go off it: still the panel's
        const panel::Hit h = hitAt(tx, ty);
        if (h.part != was.part || h.card != was.card) return true;
        if (h.part == panel::Part::Close) {
            hide(true);
        } else if (h.part == panel::Part::Layout && config_) {
            config_->panelLayout = config_->panelLayout == PanelLayout::Row ? PanelLayout::Column : PanelLayout::Row;
            editIniFile(ourFile(layout::File::Ini), {{"panel", "layout", panelLayoutToken(config_->panelLayout)}});
            LOG_INFO("windows: the panel's cards %s (its button)",
                     config_->panelLayout == PanelLayout::Row ? "in a row" : "in a column");
        } else if (h.part == panel::Part::Card && size_t(h.card) < all_.size() && all_[size_t(h.card)].pid != self_ &&
                   onPick) {
            onPick(all_[size_t(h.card)].number);
        }
        return true;
    }
    // 3.0.3-beta.10: the right button pressed and let go on it - its menu at the cursor; pressed off it (the game's
    // camera turned with it): the let-go the game's
    if (m == WM_RBUTTONDOWN || m == WM_RBUTTONDBLCLK) rightOnPanel_ = over;
    if (m == WM_RBUTTONUP) {
        const bool mine = rightOnPanel_;
        rightOnPanel_ = false;
        if (mine && over && !dragging_ && config_) {
            openMenu(tx, ty);
            block(game);
        }
        return mine;
    }
    return over;  // the other buttons on it: not the game's either
}

void WindowPanel::openMenu(float tx, float ty) {
    menuOpen_ = true;
    menuAtX_ = tx;
    menuAtY_ = ty;
    menuHover_ = menuPressed_ = panel::MenuHit{};
    menuRows_ = panel::menuRows(*config_);
    menuScale_ = std::clamp(k_, 0.75f, 6.0f);  // the game's interface scale: not [panel] size, set in it
    renderMenu();
    if (hover_ >= 0 || hoverPart_ != panel::Part::None) {  // the panel's mouse is the menu's now
        hover_ = -1;
        hoverPart_ = panel::Part::None;
        dirty_ = true;
    }
}

void WindowPanel::renderMenu() {
    menuGeometry_ = panel::menuGeometry(menuRows_, menuScale_);
    panel::paintMenu(menuPicture_, menuGeometry_, menuRows_, menuHover_);
    const std::vector<panel::MenuText> texts = panel::menuTexts(startup().lang);
    const float s = menuGeometry_.scale;
    std::vector<Words> words;
    for (size_t i = 0; i < menuRows_.size() && i < texts.size() && i < menuGeometry_.label.size(); ++i) {
        const bool choice = menuRows_[i].kind == panel::MenuRow::Kind::Choice;
        words.push_back(Words{menuGeometry_.label[i], utf8Wide(texts[i].label), choice ? panel::kMuted : panel::kText,
                              12.5f * s, false, DT_LEFT});
        for (size_t k = 0; k < texts[i].segments.size() && k < menuGeometry_.segments[i].size(); ++k)
            words.push_back(Words{menuGeometry_.segments[i][k], utf8Wide(texts[i].segments[k]), panel::kText,
                                  11.5f * s, int(k) == menuRows_[i].chosen, DT_CENTER});
    }
    drawWords(menuPicture_, words);
    // right of the cursor and below it; no room there: left of it, above it; inside the target whatever it takes
    const int w = menuGeometry_.width, h = menuGeometry_.height;
    int x = int(std::lround(menuAtX_)), y = int(std::lround(menuAtY_));
    if (x + w > targetW_) x -= w;
    if (y + h > targetH_) y -= h;
    menuX_ = std::clamp(x, 0, std::max(0, targetW_ - w));
    menuY_ = std::clamp(y, 0, std::max(0, targetH_ - h));
    menuDirty_ = false;
    menuUpload_ = true;
}

void WindowPanel::pick(panel::MenuHit h) {
    panel::MenuChange change;
    if (!config_ || !panel::menuPick(*config_, h, change)) return;
    if (change.hide) {
        closeMenu();
        hide(true);
        return;
    }
    editIniFile(ourFile(layout::File::Ini), {{change.section, change.key, change.value}});
    LOG_INFO("windows: the panel's menu - [%s] %s = %s", change.section.c_str(), change.key.c_str(),
             change.value.c_str());
}

bool WindowPanel::closeMenu() {
    if (!menuOpen_) return false;
    menuOpen_ = false;
    menuHover_ = menuPressed_ = panel::MenuHit{};
    return true;
}

void WindowPanel::hide(bool hidden) {
    if (!config_) return;
    config_->panelHidden = hidden;  // what the file says now: not taken for a change of it
    editIniFile(ourFile(layout::File::Ini), {{"panel", "hidden", hidden ? "1" : "0"}});
    LOG_INFO("windows: the panel %s",
             hidden ? "hidden (its cross, its menu or Ctrl+Alt+0; Ctrl+Alt+0 shows it again)" : "shown");
}

void WindowPanel::toggle() {
    if (config_) hide(!config_->panelHidden);
}

void WindowPanel::keepPlace() {
    if (!config_) return;
    place_ = panel::placeOf(panel::Point{x_, y_}, geometry_.width, geometry_.height, targetW_, targetH_,
                            geometry_.scale);
    placeText_ = panel::formatPlace(place_);
    config_->panelPlace = placeText_;
    if (editIniFile(ourFile(layout::File::Ini), {{"panel", "place", placeText_}}))
        LOG_INFO("windows: the panel dragged to %s (of the room beside it)", placeText_.c_str());
}

}  // namespace uagfx::win
