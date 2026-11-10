#include "win/room_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/room_card.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::roomwin {
namespace {

namespace rc = uagfx::roomcard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the room window (where the manager keeps it, its class's vtable, its SendMsg; its place, title, open
// flag, count, places, edit, list boxes - their words' and colours' firsts and pasts); an edit's words, the manager's
// keys and SetFocus; the game mode (its messages: 0x34 kick, 0x33 the owner; its room's settings at + 0x394);
// MakeWindow; the manager's «add a friend»
constexpr uint32_t kWindowMgr = 0x727ED8, kRoomAt = kWindowMgr + 0x104, kRoomClass = 0x697E10, kRoomSend = 0x460DD0;
constexpr uint32_t kSendMsgSlot = 0x94, kRect = 0x14, kTitle = 0x64, kOpen = 0x80, kCount = 0x84, kLimit = 0x88;
constexpr uint32_t kEdit = 0x94, kLinesBox = 0x98, kMembersBox = 0x9C, kWords = 0x58, kColours = 0x94;
constexpr uint32_t kGetText = 0x443300, kFocus = kWindowMgr + 0x6C, kSetFocus = 0x4CE510;
constexpr uint32_t kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0, kRoomSetup = 0x394;
constexpr uint32_t kMakeWindow = 0x4D42D0, kAddFriend = 0x4CEC50;
constexpr int kButtonMsg = 6, kLeave = 0x87, kKick = 0x34, kGiveOwner = 0x33, kSetupWindow = 0x1E, kSetupMsg = 0x22;
constexpr uint32_t kOwnerColour = 0x808000;
constexpr int kAway = -4000, kMostLines = 200, kMostMembers = 32;
constexpr ULONGLONG kBlinkMs = 530;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);
using GetTextFn = const char*(__attribute__((thiscall)) *)(uint32_t edit);
using SetFocusFn = void(__attribute__((thiscall)) *)(uint32_t manager, uint32_t control);
using MakeFn = uint32_t(__attribute__((thiscall)) *)(uint32_t manager, int id);
using FriendFn = void(__attribute__((thiscall)) *)(uint32_t manager, const char* name);

struct State {
    uisheet::Sheet sheet;  // 3.0.18-beta.32: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool on = false, shown = false, moved = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0, oldX = 0;
    uint32_t window = 0;
    std::vector<uint32_t> names;  // the members' words in the game's memory (its own strings), as the card lists them
    rc::Card card;
    rc::Show show;
    rc::Layout layout;
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

uint32_t gameRoom() {
    const uint32_t w = read32(kRoomAt);
    return w && read32(w) == kRoomClass && read32(kRoomClass + kSendMsgSlot) == kRoomSend ? w : 0;
}

std::string text(const State& s, uint32_t at) {
    char buf[128] = {};
    if (!at || !readGame(at, buf, sizeof buf - 1)) return "";
    buf[sizeof buf - 1] = 0;
    return utf8Of(widened(buf, s.codePage));
}

// The game's window out of sight while the card shows (its place kept; one opened at the place this left - the game
// keeps its rooms' place - put back where it can be seen)
void hold(State& s, uint32_t window, bool on) {
    if (!window) {
        s.moved = false;
        return;
    }
    const int32_t x = int32_t(read32(window + kRect));
    if (on && x != kAway) {
        s.oldX = x;
        write32(window + kRect, uint32_t(kAway));
        if (!s.moved) LOG_INFO("chat rooms: the game's room window off the screen for the new card (its line keeps the "
                               "keys)");
        s.moved = true;
    } else if (!on && (s.moved || x == kAway)) {
        write32(window + kRect, uint32_t(s.moved && s.oldX != kAway ? s.oldX : 40));
        s.moved = false;
    }
}

// A list box's words (and colours) - the game's strings' addresses kept for the members' actions
void readBox(uint32_t box, size_t most, std::vector<uint32_t>& words, std::vector<uint32_t>& colours) {
    words.clear();
    colours.clear();
    if (!box) return;
    const uint32_t from = read32(box + kWords), past = read32(box + kWords + 4);
    if (from && past > from && (past - from) / 16 <= 4096) {
        uint32_t first = from;
        if ((past - from) / 16 > most) first = past - uint32_t(most) * 16;
        for (uint32_t at = first; at < past; at += 16) words.push_back(read32(at + 4));
    }
    const uint32_t cf = read32(box + kColours), cp = read32(box + kColours + 4);
    if (cf && cp > cf && (cp - cf) / 4 <= 4096) {
        const size_t all = (cp - cf) / 4, skip = all > words.size() ? all - words.size() : 0;
        for (uint32_t at = cf + uint32_t(skip) * 4; at < cp; at += 4) colours.push_back(read32(at));
    }
}

void readCard(State& s, uint32_t window) {
    rc::Card& c = s.card;
    c.title = text(s, read32(window + kTitle));
    c.open = read32(window + kOpen) != 0;
    c.count = int(read32(window + kCount));
    c.limit = int(read32(window + kLimit));
    std::vector<uint32_t> words, colours;
    readBox(read32(window + kLinesBox), kMostLines, words, colours);
    c.lines.clear();
    for (size_t i = 0; i < words.size(); ++i)
        c.lines.push_back(rc::Line{text(s, words[i]), i < colours.size() && colours[i] ? rc::rgbOf(colours[i]) : 0});
    readBox(read32(window + kMembersBox), kMostMembers, s.names, colours);
    c.members.clear();
    const std::string me = utf8Of(widened(readOwnName(), s.codePage));
    c.mine = false;
    for (size_t i = 0; i < s.names.size(); ++i) {
        const bool owner = i < colours.size() && colours[i] == kOwnerColour;
        c.members.push_back(rc::Member{text(s, s.names[i]), owner});
        if (owner && !me.empty() && c.members.back().name == me) c.mine = true;
    }
    if (c.chosen >= int(c.members.size())) c.chosen = -1;
    const uint32_t edit = read32(window + kEdit);
    c.input.clear();
    if (edit) {
        const char* t = reinterpret_cast<GetTextFn>(uintptr_t(kGetText))(edit);
        c.input = text(s, uint32_t(reinterpret_cast<uintptr_t>(t)));
    }
    c.focus = edit && read32(kFocus) == edit;
    c.caret = (GetTickCount64() / kBlinkMs) % 2 == 0;
}

rc::Texts texts() {
    rc::Texts t;
    t.open = say(settings::Text::RoomOpen);
    t.locked = say(settings::Text::RoomLocked);
    t.members = say(settings::Text::RoomMembers);
    t.setup = say(settings::Text::RoomSetup);
    t.leave = say(settings::Text::RoomLeave);
    t.kick = say(settings::Text::RoomKick);
    t.owner = say(settings::Text::RoomOwner);
    t.friendAdd = say(settings::Text::RoomFriend);
    return t;
}

std::vector<Words> gdiWords(const std::vector<rc::Words>& in) {
    std::vector<Words> out;
    for (const rc::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == rc::Align::Right    ? DT_RIGHT
                                 : w.align == rc::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    if (uaui::ready()) {  // 3.0.18-beta.32: uaUI's window says it all again, no picture
        ++s.sheetGen;
        return;
    }
    const uicost::Paint cost("the chat room", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    rc::paint(s.picture, s.layout, s.card, s.show);
    drawWords(s.picture, gdiWords(rc::words(s.layout, s.card, s.show, texts())));
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
    uapad::gamewindow::setBlocked(Blocker::Room, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Room, nullptr, nullptr, false);
    s.shown = false;
}

uint32_t gameMode() {
    const uint32_t mode = read32(kModeNow);
    return mode && read32(read32(mode) + kModeMsgSlot) == kModeMsg ? mode : 0;
}

void act(State& s, const rc::Hit& h) {
    const uint32_t w = gameRoom();
    if (!w) return;
    rc::Card& c = s.card;
    const int chosen = c.chosen;
    const uint32_t name = chosen >= 0 && chosen < int(s.names.size()) ? s.names[size_t(chosen)] : 0;
    switch (h.part) {
        case rc::Part::Member: c.chosen = h.index == c.chosen ? -1 : h.index; break;
        case rc::Part::Input: {
            const uint32_t edit = read32(w + kEdit);
            if (edit) reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, edit);
            break;
        }
        case rc::Part::Leave:
        case rc::Part::Close:
            if (s.told++ < 30) LOG_INFO("chat rooms: left the room (the window's 0x87)");
            reinterpret_cast<SendMsgFn>(uintptr_t(kRoomSend))(w, 0, kButtonMsg, kLeave, 0, 0);
            break;
        case rc::Part::Setup: {
            const uint32_t mode = gameMode();
            if (!mode || !c.mine) break;
            const uint32_t setup = reinterpret_cast<MakeFn>(uintptr_t(kMakeWindow))(kWindowMgr, kSetupWindow);
            if (!setup) break;
            if (s.told++ < 30) LOG_INFO("chat rooms: the room's settings opened (UI window 0x1E)");
            reinterpret_cast<SendMsgFn>(uintptr_t(read32(read32(setup) + kSendMsgSlot)))(
                setup, 0, kSetupMsg, int(mode + kRoomSetup), 0, 0);
            break;
        }
        case rc::Part::Kick:
        case rc::Part::Owner: {
            const uint32_t mode = gameMode();
            if (!mode || !name || !c.mine) break;
            const bool kick = h.part == rc::Part::Kick;
            if (s.told++ < 30) LOG_INFO("chat rooms: %s", kick ? "a member kicked out" : "the owner given");
            reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, kick ? kKick : kGiveOwner, int(name), 0, 0);
            c.chosen = -1;
            break;
        }
        case rc::Part::Friend:
            if (!name) break;
            if (s.told++ < 30) LOG_INFO("chat rooms: a member asked to be a friend");
            reinterpret_cast<FriendFn>(uintptr_t(kAddFriend))(kWindowMgr,
                                                              reinterpret_cast<const char*>(uintptr_t(name)));
            c.chosen = -1;
            break;
        case rc::Part::None: break;
    }
    s.key.clear();
}

}  // namespace

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    s.on = on;
    const bool world = inGameWorld();
    const uint32_t window = world ? gameRoom() : 0;
    if (world) hold(s, window, on);
    if (window != s.window) {
        s.window = window;
        s.card.chosen = -1;
    }
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) LOG_INFO("chat rooms: the game's room window - the new card shows it");
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    readCard(s, window);
    s.layout = rc::layout(s.card, targetWidth, targetHeight, s.scale);
    s.show = rc::Show{};
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy))
        s.show.hover = rc::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                   float(cy) * float(k) - float(s.layout.y));
    const rc::Card& c = s.card;
    std::string key = c.title + ';' + std::to_string(c.open) + ',' + std::to_string(c.count) + '/' +
                      std::to_string(c.limit) + ';' + c.input + (c.focus ? "F" : "") + (c.caret ? "|" : "") + ';' +
                      std::to_string(c.mine) + ',' + std::to_string(c.chosen) + ';' +
                      std::to_string(int(s.show.hover.part)) + ',' + std::to_string(s.show.hover.index) + ';' +
                      std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + '#' +
                      std::to_string(uistyle::generation()) + ';' + std::to_string(c.lines.size());
    if (!c.lines.empty()) key += c.lines.back().text;
    for (const rc::Member& m : c.members) key += ';' + m.name + (m.owner ? "*" : "");
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.32
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.32 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the chat room in RmlUi's
// markup, line by line as core roomcard paints it - its glass, ✕, the open/closed badge, the lines' ground, the
// members (chosen or pointed at gold), the input, «Налаштування», «Вийти», «Вигнати», «Власник», «У друзі»; the words
// where core roomcard places them
static void fillSheet(State& s, uisheet::Content& c) {
    const rc::Layout& L = s.layout;
    const rc::Card& d = s.card;
    const rc::Show& sh = s.show;
    const float sc = L.s;
    constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kBlack = 0x000000, kGreen = 0x4CAF7A;
    constexpr uint32_t kRed = 0xD9534F;
    auto& u = c.under;
    c.w = float(L.w), c.h = float(L.h);
    c.glass = uisheet::Box{L.glass.x, L.glass.y, L.glass.w, L.glass.h, 12 * sc, 1.0f * sc, 1, "glass"};
    c.ground = uisheet::Sheet::ground(kCard, uistyle::window());
    const bool closeHot = sh.hover.part == rc::Part::Close;
    c.rect(u, L.close, 6 * sc, kWhite, closeHot ? 0.14f : 0.0f, kWhite, closeHot ? 0.3f : 0.0f, 1.0f * sc);
    c.rect(u, L.badge, L.badge.h / 2, d.open ? kGreen : kGold, 0.22f, d.open ? kGreen : kGold, 0.7f, 1.0f * sc);
    c.rect(u, L.lines, 8 * sc, kBlack, 0.25f, kWhite, 0.06f, 1.0f * sc);
    for (size_t i = 0; i < L.memberRows.size(); ++i) {
        const bool chosen = int(i) == d.chosen, hot = sh.hover.part == rc::Part::Member && sh.hover.index == int(i);
        if (chosen || hot)
            c.rect(u, L.memberRows[i], 6 * sc, kGold, chosen ? 0.22f : 0.10f, kGold, chosen ? 0.6f : 0.0f, 1.0f * sc);
    }
    c.rect(u, L.input, 8 * sc, kBlack, 0.35f, d.focus ? kGold : kWhite, d.focus ? 0.8f : 0.18f, 1.0f * sc);
    const auto button = [&](const panel::Box& b, rc::Part p, uint32_t rgb, float a) {
        c.rect(u, b, 7 * sc, rgb, sh.hover.part == p ? a + 0.10f : a, rgb, 0.5f, 1.0f * sc);
    };
    button(L.setup, rc::Part::Setup, kGold, 0.16f);
    button(L.leave, rc::Part::Leave, kWhite, 0.06f);
    button(L.kick, rc::Part::Kick, kRed, 0.16f);
    button(L.owner, rc::Part::Owner, kGold, 0.14f);
    button(L.friendAdd, rc::Part::Friend, kWhite, 0.06f);
    c.say(rc::words(L, d, sh, texts()));
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    s.sheetShown = s.sheet.frame("room", s.shown, true, s.layout.x, s.layout.y, s.sheetGen,
                                 [&](uisheet::Content& c) { fillSheet(s, c); });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    s.sheet.raise();  // 3.0.18-beta.43: at its place in the windows' stack
    if (!s.shown || s.k <= 0 || ((s.picture.width <= 0 || s.picture.height <= 0) && !s.sheetShown)) return;
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;
    // 3.0.18-beta.43: the shadow the driver's, under uaUI's window too
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h),
                  12 * s.scale, s.scale, s.k);
    if (!s.sheetShown) {  // 3.0.18-beta.32: uaUI draws the window (with the windows' layer)
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
    if (m == WM_LBUTTONDOWN) act(s, rc::hitTest(s.layout, lx, ly));
    return true;
}

bool shown() { return state().shown; }

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, gameRoom(), true);
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

}  // namespace uagfx::win::roomwin
