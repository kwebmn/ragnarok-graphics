#include "win/chatroom_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_stack.hpp"
#include "core/chatroom_card.hpp"
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

namespace uagfx::win::chatroomwin {
namespace {

namespace rc = uagfx::chatroomcard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the making window (where the manager keeps it, its class's vtable, its SendMsg; its place, its edits,
// radios, the chosen radio, the combo the message 0x27 picks for, the chosen places, the places' list); an edit's
// words and length (thiscall), the manager's control with the keys and its SetFocus
constexpr uint32_t kWindowMgr = 0x727ED8, kMakeAt = kWindowMgr + 0xFC, kMakeClass = 0x699968, kMakeSend = 0x482220;
// 3.0.11-beta.208: the room's settings (UI window 0x1E, its owner's): the making window's kind with its own SendMsg
// (its OK changes the room: the mode's message 0x31)
constexpr uint32_t kSetupAt = kWindowMgr + 0x100, kSetupClass = 0x699A00, kSetupSend = 0x482A20;
constexpr uint32_t kSendMsgSlot = 0x94, kRect = 0x14, kTitleEdit = 0x70, kPasswordEdit = 0x74, kRadios = 0x78;
constexpr uint32_t kRadioChosen = 0x8C, kComboFor = 0x90, kPlaces = 0x94, kPlacesList = 0xC0;
constexpr uint32_t kGetText = 0x443300, kGetLength = 0x443330, kFocus = kWindowMgr + 0x6C, kSetFocus = 0x4CE510;
constexpr int kButtonMsg = 6, kOk = 0x76, kCancel = 0x77, kRadioMsg = 0x95, kPickMsg = 0x27, kAway = -4000;
constexpr ULONGLONG kBlinkMs = 530;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using GetTextFn = const char*(__attribute__((thiscall)) *)(uint32_t edit);
using GetLengthFn = int(__attribute__((thiscall)) *)(uint32_t edit);
using SetFocusFn = void(__attribute__((thiscall)) *)(uint32_t manager, uint32_t control);

struct State {
    uisheet::Sheet sheet;  // 3.0.18-beta.32: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool on = false, shown = false, moved = false, setup = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0, oldX = 0;
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

// The making window, or the room's settings (`setup` set) - the one up
uint32_t gameMaking(bool* setup = nullptr) {
    const uint32_t w = read32(kMakeAt);
    if (setup) *setup = false;
    if (w && read32(w) == kMakeClass && read32(kMakeClass + kSendMsgSlot) == kMakeSend) return w;
    const uint32_t v = read32(kSetupAt);
    if (!v || read32(v) != kSetupClass || read32(kSetupClass + kSendMsgSlot) != kSetupSend) return 0;
    if (setup) *setup = true;
    return v;
}

uint32_t sendOf(uint32_t window) { return read32(read32(window) + kSendMsgSlot); }

void hold(State& s, uint32_t window, bool on) {
    if (!window) {
        s.moved = false;
        return;
    }
    const int32_t x = int32_t(read32(window + kRect));
    if (on && x != kAway) {
        s.oldX = x;
        write32(window + kRect, uint32_t(kAway));
        if (!s.moved) LOG_INFO("chat rooms: the game's making window off the screen for the new card (its fields keep "
                               "the keys)");
        s.moved = true;
    } else if (!on && s.moved) {
        write32(window + kRect, uint32_t(s.oldX));
        s.moved = false;
    }
}

void readCard(State& s, uint32_t window) {
    rc::Card& c = s.card;
    const uint32_t title = read32(window + kTitleEdit), password = read32(window + kPasswordEdit);
    const uint32_t focus = read32(kFocus);
    c.title.clear();
    if (title) {
        char buf[64] = {};
        const char* t = reinterpret_cast<GetTextFn>(uintptr_t(kGetText))(title);
        if (t && readGame(uint32_t(reinterpret_cast<uintptr_t>(t)), buf, sizeof buf - 1)) {
            buf[sizeof buf - 1] = 0;
            c.title = utf8Of(widened(buf, s.codePage));
        }
    }
    c.passwordLength = password ? size_t(std::clamp(reinterpret_cast<GetLengthFn>(uintptr_t(kGetLength))(password),
                                                    0, 16))
                                : 0;
    c.focus = focus && focus == title ? rc::Field::Title : focus && focus == password ? rc::Field::Password
                                                                                      : rc::Field::None;
    c.caret = (GetTickCount64() / kBlinkMs) % 2 == 0;
    c.limits.clear();
    const uint32_t from = read32(window + kPlacesList), past = read32(window + kPlacesList + 4);
    if (from && past > from && (past - from) / 4 <= 16)
        for (uint32_t at = from; at < past; at += 4) c.limits.push_back(int(read32(at)));
    c.limit = int(read32(window + kPlaces));
    c.closed = read32(window + kRadioChosen) == 1;
}

rc::Texts texts(const State& s) {
    rc::Texts t;
    t.heading = say(s.setup ? settings::Text::RoomSetupTitle : settings::Text::RoomNewTitle);
    t.titleLabel = say(settings::Text::RoomName);
    t.limitLabel = say(settings::Text::RoomPlaces);
    t.open = say(settings::Text::RoomOpen);
    t.closed = say(settings::Text::RoomLocked);
    t.passwordLabel = say(settings::Text::RoomPassword);
    t.create = say(s.setup ? settings::Text::RoomSave : settings::Text::RoomCreate);
    t.cancel = say(settings::Text::RoomCancel);
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
    const uicost::Paint cost("the chat room making", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    rc::paint(s.picture, s.layout, s.card, s.show);
    drawWords(s.picture, gdiWords(rc::words(s.layout, s.card, s.show, texts(s))));
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
    uapad::gamewindow::setBlocked(Blocker::ChatRoom, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::ChatRoom, nullptr, nullptr, false);
    s.shown = false;
}

void send(State& s, uint32_t sender, int msg, int a, const char* what) {
    const uint32_t w = gameMaking();
    if (!w) return;
    if (s.told++ < 30) LOG_INFO("chat rooms: %s%s", s.setup ? "the room's settings - " : "", what);
    reinterpret_cast<SendMsgFn>(uintptr_t(sendOf(w)))(w, sender, msg, a, 0, 0);
    s.key.clear();
}

void focusEdit(State& s, uint32_t field) {
    const uint32_t w = gameMaking();
    const uint32_t edit = w ? read32(w + field) : 0;
    if (edit) reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, edit);
    s.key.clear();
}

void pick(State& s, const rc::Hit& h) {
    const uint32_t w = gameMaking();
    if (!w) return;
    switch (h.part) {
        case rc::Part::Title: focusEdit(s, kTitleEdit); break;
        case rc::Part::Password:
            if (s.card.closed) focusEdit(s, kPasswordEdit);
            break;
        case rc::Part::Limit:
            write32(w + kComboFor, 0);  // the places' combo, as its own pick
            send(s, 0, kPickMsg, h.index, "the places picked");
            break;
        case rc::Part::Open:
        case rc::Part::Closed: {
            const bool closed = h.part == rc::Part::Closed;
            send(s, read32(w + kRadios + (closed ? 4 : 0)), kButtonMsg, kRadioMsg,
                 closed ? "with a password" : "open");
            if (closed) focusEdit(s, kPasswordEdit);
            break;
        }
        case rc::Part::Create: send(s, 0, kButtonMsg, kOk, "«Створити» - the window's OK (the game checks and sends)");
            break;
        case rc::Part::Cancel:
        case rc::Part::Close: send(s, 0, kButtonMsg, kCancel, "cancelled (the window's 0x77)"); break;
        case rc::Part::None: break;
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
        LOG_INFO("chat rooms: [interface] chat_rooms = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    bool setup = false;
    const uint32_t window = world ? gameMaking(&setup) : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown || setup != s.setup)
        LOG_INFO("chat rooms: the game's %s - the new card shows it",
                 setup ? "room settings (UI window 0x1E)" : "making window (Alt+C)");
    s.setup = setup;
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    readCard(s, window);
    s.layout = rc::layout(s.card.limits.size(), targetWidth, targetHeight, s.scale);
    s.show = rc::Show{};
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy))
        s.show.hover = rc::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                   float(cy) * float(k) - float(s.layout.y));
    const rc::Card& c = s.card;
    const std::string key = std::string(s.setup ? "S" : "M") + c.title + ';' + std::to_string(c.passwordLength) +
                            ';' + std::to_string(int(c.focus)) +
                            (c.caret ? "|" : "") + ';' + std::to_string(c.limit) + (c.closed ? "c" : "o") + ';' +
                            std::to_string(int(s.show.hover.part)) + ',' + std::to_string(s.show.hover.index) + ';' +
                            std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + '#' +
                            std::to_string(uistyle::generation());
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.32
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.32 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): a chat room's making in
// RmlUi's markup, line by line as core chatroomcard paints it - its glass, ✕, the title's and the password's fields
// (the password dim while open), the limits' and open/closed chips (the chosen gold), «Створити», «Скасувати»; the
// words where core chatroomcard places them
static void fillSheet(State& s, uisheet::Content& c) {
    const rc::Layout& L = s.layout;
    const rc::Card& d = s.card;
    const rc::Show& sh = s.show;
    const float sc = L.s;
    constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kBlack = 0x000000;
    auto& u = c.under;
    c.w = float(L.w), c.h = float(L.h);
    c.glass = uisheet::Box{L.glass.x, L.glass.y, L.glass.w, L.glass.h, 12 * sc, 1.0f * sc, 1, "glass"};
    c.ground = uisheet::Sheet::ground(kCard, uistyle::window());
    const bool closeHot = sh.hover.part == rc::Part::Close;
    c.rect(u, L.close, 6 * sc, kWhite, closeHot ? 0.14f : 0.0f, kWhite, closeHot ? 0.3f : 0.0f, 1.0f * sc);
    const auto field = [&](const panel::Box& b, bool focused, bool dim) {
        c.rect(u, b, 8 * sc, kBlack, dim ? 0.15f : 0.35f, focused ? kGold : kWhite,
               focused ? 0.8f : (dim ? 0.08f : 0.18f), 1.0f * sc);
    };
    field(L.title, d.focus == rc::Field::Title, false);
    field(L.password, d.focus == rc::Field::Password, !d.closed);
    const auto chip = [&](const panel::Box& b, bool on, bool hot) {
        c.rect(u, b, 7 * sc, on ? kGold : kWhite, on ? 0.26f : (hot ? 0.12f : 0.05f), on ? kGold : kWhite,
               on ? 0.8f : 0.14f, 1.0f * sc);
    };
    for (size_t i = 0; i < L.limits.size(); ++i)
        chip(L.limits[i], i < d.limits.size() && d.limits[i] == d.limit,
             sh.hover.part == rc::Part::Limit && sh.hover.index == int(i));
    chip(L.open, !d.closed, sh.hover.part == rc::Part::Open);
    chip(L.closed, d.closed, sh.hover.part == rc::Part::Closed);
    c.rect(u, L.create, 8 * sc, kGold, sh.hover.part == rc::Part::Create ? 0.34f : 0.22f, kGold, 0.8f, 1.0f * sc);
    c.rect(u, L.cancel, 8 * sc, kWhite, sh.hover.part == rc::Part::Cancel ? 0.12f : 0.06f, kWhite, 0.16f, 1.0f * sc);
    c.say(rc::words(L, d, sh, texts(s)));
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    s.sheetShown = s.sheet.frame("chatroom", s.shown, true, s.layout.x, s.layout.y, s.sheetGen,
                                 [&](uisheet::Content& c) { fillSheet(s, c); });
    if (s.sheetShown) {  // 3.0.18-beta.60: the pad walks its boxes (A clicks one)
        std::vector<panel::Box> targets;
        for (const auto& part : std::initializer_list<std::vector<panel::Box>>{s.layout.limits, {s.layout.title, s.layout.open, s.layout.closed, s.layout.password, s.layout.create, s.layout.cancel, s.layout.close}})
            targets.insert(targets.end(), part.begin(), part.end());
        s.sheet.nav(int(winstack::Win::ChatRoom), std::move(targets));
    }
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
    if (m == WM_LBUTTONDOWN) pick(s, rc::hitTest(s.layout, lx, ly));
    return true;
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    if (!s.shown || !gameMaking()) return false;
    send(s, 0, kButtonMsg, kCancel, "Esc - cancelled (the window's 0x77)");
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, gameMaking(), true);
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

}  // namespace uagfx::win::chatroomwin
