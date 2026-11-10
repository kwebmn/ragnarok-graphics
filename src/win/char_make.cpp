#include "win/char_make.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "core/char_make.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/head_picture.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::charmakewin {
namespace {

namespace cm = uagfx::charmake;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the character make - the manager's windows' list (a node: next +0, previous +4, the window +8), the
// window's class's vtable (its constructor FUN_0045b7b0) and SendMsg; its name field (an edit control: its words
// FUN_00443300, thiscall), the stats, the hair's style and colour; the manager's control with the keys (+ 0x6C) and
// its SetFocus; the account's sex (as the character block reads it)
constexpr uint32_t kWindowMgr = 0x727ED8, kList = 0x38, kMakeClass = 0x697C3C, kMakeSend = 0x45C5F0;
constexpr uint32_t kNameEdit = 0x60, kStats = 0xC6, kHair = 0x9A, kColour = 0xAA, kRect = 0x14, kSendMsgSlot = 0x94;
constexpr uint32_t kGetText = 0x443300, kFocus = kWindowMgr + 0x6C, kSetFocus = 0x4CE510, kSex = 0x772DEC;
constexpr int kButtonMsg = 6, kAway = -4000;
constexpr size_t kMostWindows = 128;
constexpr ULONGLONG kBlinkMs = 530;
constexpr int kRows = 5;  // the controller's: the hair, its colour, the three pairs
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using GetTextFn = const char*(__attribute__((thiscall)) *)(uint32_t edit);
using SetFocusFn = void(__attribute__((thiscall)) *)(uint32_t manager, uint32_t control);

struct State {
    uisheet::Sheet sheet;  // 3.0.18-beta.33: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool on = false, shown = false, moved = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0, oldX = 0;
    uint32_t window = 0;
    cm::Fields fields;
    std::map<std::tuple<int, int, int, int>, panel::Picture> faces;  // by sex, hair, colour, side
    cm::Show show;
    cm::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(language(), t); }

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

// The character make in the manager's list (it is kept nowhere else); none in the world
uint32_t gameMake() {
    if (inGameWorld()) return 0;
    const uint32_t head = read32(kWindowMgr + kList);
    if (!head) return 0;
    uint32_t node = read32(head);
    for (size_t n = 0; n < kMostWindows && node && node != head; ++n) {
        const uint32_t w = read32(node + 8);
        if (w && read32(w) == kMakeClass && read32(read32(w) + kSendMsgSlot) == kMakeSend) return w;
        node = read32(node);
    }
    return 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (!window) {
        s.moved = false;
        return;
    }
    const int32_t x = int32_t(read32(window + kRect));
    if (on && x != kAway) {
        s.oldX = x;
        write32(window + kRect, uint32_t(kAway));
        if (!s.moved)
            LOG_INFO("character make: the game's window off the screen for the new one (its field keeps the keys)");
        s.moved = true;
    } else if (!on && s.moved) {
        write32(window + kRect, uint32_t(s.oldX));
        s.moved = false;
    }
}

uint16_t read16(uint32_t at) {
    uint16_t v = 0;
    return readGame(at, &v, 2) ? v : 0;
}

cm::Fields fieldsOf(State& s, uint32_t window) {
    cm::Fields f;
    const uint32_t edit = read32(window + kNameEdit);
    if (edit) {
        const char* t = reinterpret_cast<GetTextFn>(uintptr_t(kGetText))(edit);
        char buf[64] = {};
        if (t && readGame(uint32_t(reinterpret_cast<uintptr_t>(t)), buf, sizeof buf - 1)) {
            buf[sizeof buf - 1] = 0;
            f.name = utf8Of(widened(buf, s.codePage));
        }
    }
    f.focus = edit && read32(kFocus) == edit;
    f.caret = (GetTickCount64() / kBlinkMs) % 2 == 0;
    uint8_t stats[6] = {};
    if (readGame(window + kStats, stats, sizeof stats))
        for (int i = 0; i < 6; ++i) f.stats[i] = stats[i];
    f.hair = int16_t(read16(window + kHair));
    f.colour = int16_t(read16(window + kColour));
    const int sex = read32(kSex) != 0 ? 1 : 0, side = cm::faceSide(s.scale);
    const auto key = std::make_tuple(sex, f.hair, f.colour, side);
    auto it = s.faces.find(key);
    if (it == s.faces.end()) {
        panel::Picture p;
        headpicture::make(sex, f.hair, f.colour, side, p);
        it = s.faces.emplace(key, std::move(p)).first;
    }
    f.face = it->second.width > 0 ? &it->second : nullptr;
    return f;
}

cm::Texts texts() {
    cm::Texts t;
    t.title = say(settings::Text::MakeTitle);
    t.name = say(settings::Text::MakeName);
    t.nameHint = say(settings::Text::MakeNameHint);
    t.hair = say(settings::Text::MakeHair);
    t.colour = say(settings::Text::MakeColour);
    t.pairsHint = say(settings::Text::MakePairsHint);
    t.make = say(settings::Text::CharsMake);
    t.cancel = say(settings::Text::MsgCancel);
    const char* const stats[6] = {"STR", "AGI", "VIT", "INT", "DEX", "LUK"};
    for (int i = 0; i < 6; ++i) t.stats[i] = stats[i];
    return t;
}

std::vector<Words> gdiWords(const std::vector<cm::Words>& in) {
    std::vector<Words> out;
    for (const cm::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == cm::Align::Right    ? DT_RIGHT
                                 : w.align == cm::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    if (uaui::readyAnywhere()) {  // 3.0.18-beta.33: uaUI's window says it all again, no picture
        ++s.sheetGen;
        return;
    }
    const uicost::Paint cost("the character make", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    cm::paint(s.picture, s.layout, s.fields, s.show);
    drawWords(s.picture, gdiWords(cm::words(s.layout, s.fields, s.show, texts())));
    s.upload = true;
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(Blocker::CharMake, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::CharMake, nullptr, nullptr, false);
    s.shown = false;
}

void send(State& s, int id, int times, const char* what) {
    const uint32_t window = gameMake();
    if (!window) return;
    if (s.told++ < 40) LOG_INFO("character make: %s - the game's 0x%X%s", what, unsigned(id), times > 1 ? " x8" : "");
    const auto fn = reinterpret_cast<SendMsgFn>(uintptr_t(read32(read32(window) + kSendMsgSlot)));
    for (int i = 0; i < times; ++i) fn(window, 0, kButtonMsg, id, 0, 0);
    s.key.clear();
}

void act(State& s, cm::Part p, int index) {
    switch (p) {
        case cm::Part::Name: {
            const uint32_t window = gameMake();
            const uint32_t edit = window ? read32(window + kNameEdit) : 0;
            if (edit) reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, edit);
            s.key.clear();
            break;
        }
        case cm::Part::HairBefore: send(s, cm::hairButton(false), 1, "the hair before"); break;
        case cm::Part::HairAfter: send(s, cm::hairButton(true), 1, "the hair after"); break;
        case cm::Part::ColourBefore:
            send(s, cm::kColourButton, cm::colourPresses(false), "the hair's colour before");
            break;
        case cm::Part::ColourAfter:
            send(s, cm::kColourButton, cm::colourPresses(true), "the hair's colour after");
            break;
        case cm::Part::PairLeft:
        case cm::Part::PairRight: {
            const bool toLeft = p == cm::Part::PairLeft;
            if (cm::canMove(s.fields, index, toLeft)) send(s, cm::pointButton(index, toLeft), 1, "a point moved");
            break;
        }
        case cm::Part::Make: send(s, cm::kMakeButton, 1, "«Створити»"); break;
        case cm::Part::Cancel: send(s, cm::kCancelButton, 1, "«Скасувати»"); break;
        case cm::Part::None: break;
    }
}

}  // namespace

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("character make: [interface] char_make = %d", on ? 1 : 0);
    }
    const uint32_t window = gameMake();
    hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        s.window = 0;
        return;
    }
    if (!s.shown || window != s.window) {
        LOG_INFO("character make: the game's window - the new one shows it (the account's sex %u)",
                 unsigned(read32(kSex)));
        s.show.focusRow = -1;
    }
    s.window = window;
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.fields = fieldsOf(s, window);
    s.layout = cm::layout(targetWidth, targetHeight, s.scale);
    s.show.hover = cm::Part::None;
    s.show.hoverIndex = -1;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const cm::Hit h = cm::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        s.show.hover = h.part;
        s.show.hoverIndex = h.index;
    }
    const cm::Fields& f = s.fields;
    std::string key = f.name + ';' + (f.focus ? "f" : "") + (f.caret ? "|" : "") + ';' + std::to_string(f.hair) + ',' +
                      std::to_string(f.colour) + (f.face ? "p" : "") + ';' + std::to_string(int(s.show.hover)) + ',' +
                      std::to_string(s.show.hoverIndex) + ',' + std::to_string(s.show.focusRow) + ';' +
                      std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + '#' +
                      std::to_string(uistyle::generation());
    for (int st : f.stats) key += ',' + std::to_string(st);
    if (const std::string drawn = key + (uaui::readyAnywhere() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.33
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.33 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the character make in
// RmlUi's markup, line by line as core charmake paints it - its glass, the face's circle and picture, the hair's and
// the colour's arrows, the controller's frames, the name's field, the stat pairs' arrows and bars (gold the left's
// share), «Створити», «Скасувати»; the words where core charmake places them
static void fillSheet(State& s, uisheet::Content& c) {
    const cm::Layout& L = s.layout;
    const cm::Fields& f = s.fields;
    const cm::Show& sh = s.show;
    const float sc = L.s;
    constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kBlue = 0x3D7EEB, kBlack = 0x000000;
    auto& u = c.under;
    c.w = float(L.w), c.h = float(L.h);
    c.rect(u, L.glass, 14 * sc, kCard, std::max(0.94f, uistyle::window()), kWhite, 0.12f, 1.0f * sc);
    c.rect(u, L.face, L.face.w / 2, kBlack, 0.30f, kWhite, 0.12f, 1.0f * sc);
    if (f.face && f.face->width > 0)
        c.icons.push_back(uisheet::Icon{std::round(L.face.x + (L.face.w - float(f.face->width)) / 2),
                                        std::round(L.face.y + (L.face.h - float(f.face->height)) / 2),
                                        float(f.face->width), float(f.face->height), 1, s.sheet.icon(f.face)});
    const auto arrow = [&](const panel::Box& b, cm::Part p, int index, bool can) {
        const bool hot = sh.hover == p && sh.hoverIndex == index;
        c.rect(u, b, 7 * sc, kWhite, !can ? 0.02f : hot ? 0.14f : 0.06f);
    };
    const auto focused = [&](const panel::Box& b, int row) {
        if (sh.focusRow == row)
            c.rect(c.over, panel::Box{b.x - 4 * sc, b.y - 2 * sc, b.w + 8 * sc, b.h + 4 * sc}, 9 * sc, kGold, 0.0f,
                   kGold, 0.8f, 1.5f * sc);
    };
    arrow(L.hairBefore, cm::Part::HairBefore, -1, true);
    arrow(L.hairAfter, cm::Part::HairAfter, -1, true);
    arrow(L.colourBefore, cm::Part::ColourBefore, -1, true);
    arrow(L.colourAfter, cm::Part::ColourAfter, -1, true);
    focused(panel::Box{L.hairBefore.x, L.hairValue.y, L.hairAfter.x + L.hairAfter.w - L.hairBefore.x, L.hairValue.h},
            0);
    focused(panel::Box{L.colourBefore.x, L.colourValue.y, L.colourAfter.x + L.colourAfter.w - L.colourBefore.x,
                       L.colourValue.h},
            1);
    c.rect(u, L.name, 9 * sc, kBlack, 0.35f, f.focus ? kGold : kWhite, f.focus ? 0.85f : 0.16f, 1.0f * sc);
    for (int i = 0; i < 3; ++i) {
        arrow(L.pairLeft[i], cm::Part::PairLeft, i, cm::canMove(f, i, true));
        arrow(L.pairRight[i], cm::Part::PairRight, i, cm::canMove(f, i, false));
        const panel::Box& bar = L.pairBar[i];
        const int left = f.stats[cm::kPairs[i].left], right = f.stats[cm::kPairs[i].right];
        const float share = left + right > 0 ? float(left) / float(left + right) : 0.5f;
        c.rect(u, bar, bar.h / 2, kBlue, 0.75f);
        c.rect(u, panel::Box{bar.x, bar.y, bar.w * share, bar.h}, bar.h / 2, kGold, 0.9f);
        focused(L.pairs[i], 2 + i);
    }
    c.rect(u, L.make, 9 * sc, kGold, sh.hover == cm::Part::Make ? 0.36f : 0.24f, kGold, 0.85f, 1.0f * sc);
    c.rect(u, L.cancel, 9 * sc, kWhite, sh.hover == cm::Part::Cancel ? 0.12f : 0.06f, kWhite, 0.16f, 1.0f * sc);
    c.say(cm::words(L, f, sh, texts()));
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    s.sheet.anywhere = true;  // 3.0.18-beta.33: before the world
    s.sheetShown = s.sheet.frame("charmake", s.shown, true, s.layout.x, s.layout.y, s.sheetGen,
                                 [&](uisheet::Content& c) { fillSheet(s, c); });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || ((s.picture.width <= 0 || s.picture.height <= 0) && !s.sheetShown)) return;
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;
    if (!s.sheetShown) {  // 3.0.18-beta.33: uaUI draws the window (with the windows' layer)
        s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h),
                      14 * s.scale, s.scale, s.k);
        drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    }
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    (void)wp;
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (!s.layout.glass.contains(lx, ly)) return false;
    result = 0;
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) {
        const cm::Hit h = cm::hitTest(s.layout, lx, ly);
        s.show.focusRow = -1;  // the mouse leads now
        act(s, h.part, h.index);
    }
    return true;
}

bool shown() { return state().shown; }

void beforeGameDraw() {
    State& s = state();
    if (s.on) hold(s, gameMake(), true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    int& row = s.show.focusRow;
    const bool side = c == PadCommand::Left || c == PadCommand::Right;
    switch (c) {
        case PadCommand::Up: row = row <= 0 ? kRows - 1 : row - 1; break;
        case PadCommand::Down: row = row < 0 || row >= kRows - 1 ? 0 : row + 1; break;
        case PadCommand::Make: act(s, cm::Part::Make, -1); break;
        case PadCommand::Cancel: act(s, cm::Part::Cancel, -1); break;
        default: break;
    }
    if (side) {
        if (row < 0) row = 0;
        const bool after = c == PadCommand::Right;
        if (row == 0) act(s, after ? cm::Part::HairAfter : cm::Part::HairBefore, -1);
        else if (row == 1) act(s, after ? cm::Part::ColourAfter : cm::Part::ColourBefore, -1);
        else act(s, after ? cm::Part::PairRight : cm::Part::PairLeft, row - 2);
    }
    s.key.clear();
}

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.shown) return false;
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

}  // namespace uagfx::win::charmakewin
