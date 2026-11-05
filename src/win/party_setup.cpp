#include "win/party_setup.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/party_setup.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/card_drag.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"

namespace uagfx::win::partysetupwin {
namespace {

namespace ps = uagfx::partysetup;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the window (where the manager keeps it, its class's vtable, its SendMsg), a window's shown flag; the
// session's party (+0xFB0: 0 while one is being made) and the leader's flag (the 0x0101/0x00fb the game keeps)
constexpr uint32_t kWindowMgr = 0x727ED8, kSetupAt = kWindowMgr + 0x158, kSetupClass = 0x699B30, kSetupSend = 0x485650;
constexpr uint32_t kShown = 0x38, kSendMsgSlot = 0x94, kParty = 0x771148 + 0xFB0, kLeader = 0x772D34;
constexpr int kButtonMsg = 6;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

struct State {
    WindowShadow shadow;
    CardDrag drag;  // 3.0.11-beta.270
    bool on = false, shown = false;
    uint32_t hidden = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    ps::Show show;
    ps::Layout layout;
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

uint32_t gameSetup() {
    const uint32_t w = read32(kSetupAt);
    return w && read32(w) == kSetupClass && read32(read32(w) + kSendMsgSlot) == kSetupSend ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (on && window) {
        if (read32(window + kShown) && write32(window + kShown, 0) && s.hidden != window) {
            LOG_INFO("party setup: the game's «Config Party» hidden for the new card (its shown flag 0)");
            s.hidden = window;
        }
    } else if (window && s.hidden == window) {
        write32(window + kShown, 1);
        s.hidden = 0;
    } else if (!window) {
        s.hidden = 0;
    }
}

ps::Texts texts() {
    ps::Texts t;
    t.title = say(settings::Text::PartySetupTitle);
    t.rows[0] = say(settings::Text::PartyExp);
    t.rows[1] = say(settings::Text::PartyPickup);
    t.rows[2] = say(settings::Text::PartyShare);
    t.choices[0][0] = say(settings::Text::PartyEach);
    t.choices[0][1] = say(settings::Text::PartyEven);
    for (int r = 1; r < 3; ++r) {
        t.choices[r][0] = say(settings::Text::PartyEach);
        t.choices[r][1] = say(settings::Text::PartyShared);
    }
    t.ok = say(settings::Text::AskOk);
    t.cancel = say(settings::Text::MsgCancel);
    t.leaderOnly = say(settings::Text::PartyLeaderOnly);
    t.thingsFixed = say(settings::Text::PartyThingsFixed);  // 3.0.11-beta.200
    return t;
}

std::vector<Words> gdiWords(const std::vector<ps::Words>& in) {
    std::vector<Words> out;
    for (const ps::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == ps::Align::Right    ? DT_RIGHT
                                 : w.align == ps::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("the party's settings", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    ps::paint(s.picture, s.layout, s.show);
    drawWords(s.picture, gdiWords(ps::words(s.layout, s.show, texts())));
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
    uapad::gamewindow::setBlocked(Blocker::PartySetup, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::PartySetup, nullptr, nullptr, false);
    s.shown = false;
}

void send(State& s, uint32_t sender, int id, const char* what) {
    const uint32_t window = gameSetup();
    if (!window) return;
    if (s.told++ < 30) LOG_INFO("party setup: %s - the game's 0x%X", what, unsigned(id));
    reinterpret_cast<SendMsgFn>(uintptr_t(kSetupSend))(window, sender, kButtonMsg, id, 0, 0);
    s.key.clear();
}

void choose(State& s, int row, int choice) {
    const uint32_t window = gameSetup();
    if (!window || row < 0 || row > 2 || choice < 0 || choice > 1) return;
    if (!ps::changeable(s.show, row)) return;  // 3.0.11-beta.200: the things' as the party was made
    const uint32_t button = read32(window + ps::kRows[row].buttons + 4 * uint32_t(choice));
    if (button) send(s, button, ps::kChooseMsg, "a choice pressed (its radio button)");
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    (void)codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("party setup: [interface] party_setup = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameSetup() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) {
        s.show = ps::Show{};
        LOG_INFO("party setup: the game's «Config Party» - the new card shows it");
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    for (int r = 0; r < 3; ++r) {
        s.show.values[r] = read32(window + ps::kRows[r].value) ? 1 : 0;
        s.show.present[r] = read32(window + ps::kRows[r].buttons) && read32(window + ps::kRows[r].buttons + 4);
    }
    s.show.canChange = read32(kParty) == 0 || read32(kLeader) != 0;
    s.show.thingsFixed = read32(kParty) != 0;  // 3.0.11-beta.200: a party already - its experience alone sent
    s.layout = ps::layout(targetWidth, targetHeight, s.scale, s.show.thingsFixed ? 1 : 3);  // .270
    s.drag.apply(s.layout.x, s.layout.y, s.layout.w, s.layout.h, targetWidth, targetHeight, k,
                 48 * s.scale);  // .270: moved by its empty part
    s.show.hoverRow = s.show.hoverChoice = -1;
    s.show.hoverPart = 0;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const ps::Hit h = ps::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        s.show.hoverRow = h.row;
        s.show.hoverChoice = h.choice;
        s.show.hoverPart = int(h.part);
    }
    std::string key = std::to_string(s.show.hoverRow) + ',' + std::to_string(s.show.hoverChoice) + ',' +
                      std::to_string(s.show.hoverPart) + ',' + std::to_string(s.show.focus) + ';' +
                      (s.show.canChange ? "c" : "") + ';' + std::to_string(s.layout.x) + ',' +
                      std::to_string(s.layout.y) + '#' + std::to_string(uistyle::generation());
    for (int r = 0; r < 3; ++r) key += std::to_string(s.show.values[r]) + (s.show.present[r] ? "p" : "");
    if (key != s.key) {
        s.key = key;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h), 12 * s.scale,
                  s.scale, s.k);
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
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
    if (m == WM_LBUTTONUP) s.drag.end();
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    const ps::Hit h = ps::hitTest(s.layout, lx, ly);
    switch (h.part) {
        case ps::Part::Choice: choose(s, h.row, h.choice); break;
        case ps::Part::Ok: send(s, 0, ps::kOkButton, "OK"); break;
        case ps::Part::Cancel: send(s, 0, ps::kCancelButton, "cancel"); break;
        case ps::Part::None: s.drag.begin(tx, ty); break;  // .270: dragged by its empty part
    }
    return true;
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    if (!s.shown) return false;
    send(s, 0, ps::kCancelButton, "Esc - cancel");
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, gameSetup(), true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    int& row = s.show.focus;
    switch (c) {
        case PadCommand::Up: row = row <= 0 ? 2 : row - 1; break;
        case PadCommand::Down: row = row < 0 || row >= 2 ? 0 : row + 1; break;
        case PadCommand::Left:
        case PadCommand::Right:
            if (row < 0) row = 0;
            choose(s, row, c == PadCommand::Right ? 1 : 0);
            break;
        case PadCommand::Ok: send(s, 0, ps::kOkButton, "the controller's OK"); break;
        case PadCommand::Cancel: send(s, 0, ps::kCancelButton, "the controller's cancel"); break;
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

}  // namespace uagfx::win::partysetupwin
