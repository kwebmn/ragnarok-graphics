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
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"

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
    uisheet::Sheet sheet;  // 3.0.18-beta.37: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
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

void render(State& s) {
    ++s.sheetGen;
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

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
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
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.37
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.37 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): this window in RmlUi's
// markup, line by line as core partysetup paints it (its boxes, icons, circles); the words where it places them
static void fillSheet(State& s, uisheet::Content& ui) {
    using namespace uagfx::partysetup;
    const auto& L = s.layout;
    const auto& sh = s.show;
    [[maybe_unused]] constexpr uint32_t kCard = 0x10141C;
    [[maybe_unused]] constexpr uint32_t kWhite = 0xFFFFFF;
    [[maybe_unused]] constexpr uint32_t kGold = 0xC9A55B;
    [[maybe_unused]] constexpr uint32_t kText = 0xEEF2F8;
    [[maybe_unused]] constexpr uint32_t kMuted = 0x9AA3B5;
    [[maybe_unused]] constexpr float kW = 420;
    [[maybe_unused]] constexpr float kPad = 18;
    [[maybe_unused]] constexpr float kTitleH = 32;
    [[maybe_unused]] constexpr float kLabelH = 20;
    [[maybe_unused]] constexpr float kChoiceH = 34;
    [[maybe_unused]] constexpr float kButtonH = 38;
    [[maybe_unused]] constexpr float kGap = 10;
    ui.w = float(L.w), ui.h = float(L.h);
    const auto put = [&](const panel::Picture& p, const panel::Box& box, float k = 1.0f) {
        if (p.width > 0)
            ui.icons.push_back(uisheet::Icon{std::round(box.x + (box.w - float(p.width)) / 2),
                                            std::round(box.y + (box.h - float(p.height)) / 2), float(p.width),
                                            float(p.height), k, s.sheet.icon(&p)});
    };
    [[maybe_unused]] const auto circle = [&](float cx, float cy, float r, uint32_t rgb, float a) {
        ui.rect(ui.under, panel::Box{cx - r, cy - r, 2 * r, 2 * r}, r, rgb, a);
    };
    [[maybe_unused]] const auto icon = [&](uiicons::Icon which, float x, float y, float side, uint32_t rgb, float a,
                                           float stroke) {
        const int px = std::max(4, int(std::ceil(side)));
        const std::string key = "u" + std::to_string(int(which)) + ':' + std::to_string(px) + ':' +
                                std::to_string(rgb) + ':' + std::to_string(int(a * 100));
        const std::string src = s.sheet.made(key, px, px, [&](panel::Picture& p) {
            uiicons::draw(p, which, 0, 0, float(px), rgb, a, stroke);
        });
        ui.icons.push_back(uisheet::Icon{x, y, float(px), float(px), 1, src});
    };
    (void)put;

    const float sc = L.s;
    ui.rect(ui.under, L.glass, 12 * sc, kCard, uistyle::window(), kWhite, 0.12f, 1.0f * sc);
    for (int r = 0; r < 3; ++r) {
        if (!sh.present[r] || L.choices[r][0].w <= 0) continue;
        if (sh.focus == r)
            ui.rect(ui.under,
                             panel::Box{L.choices[r][0].x - 4 * sc, L.choices[r][0].y - 3 * sc,
                                        L.choices[r][1].x + L.choices[r][1].w - L.choices[r][0].x + 8 * sc,
                                        L.choices[r][0].h + 6 * sc},
                             10 * sc, kGold, 0.0f, kGold, 0.7f, 1.5f * sc);
        for (int c = 0; c < 2; ++c) {
            const bool can = changeable(sh, r);
            const bool on = sh.values[r] == c, hot = sh.hoverRow == r && sh.hoverChoice == c && can;
            ui.rect(ui.under, L.choices[r][c], 8 * sc, on ? kGold : kWhite,
                    on ? (can ? 0.26f : 0.14f) : hot ? 0.10f : 0.04f, on ? kGold : kWhite,
                    on ? (can ? 0.85f : 0.45f) : 0.14f, 1.0f * sc);
        }
    }
    ui.rect(ui.under, L.ok, 8 * sc, kGold, sh.hoverPart == int(Part::Ok) ? 0.34f : 0.22f, kGold, 0.8f, 1.0f * sc);
    ui.rect(ui.under, L.cancel, 8 * sc, kWhite, sh.hoverPart == int(Part::Cancel) ? 0.12f : 0.06f, kWhite, 0.16f,
            1.0f * sc);
    ui.say(uagfx::partysetup::words(s.layout, s.show, texts()));
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    s.sheetShown = s.sheet.frame("psetup", s.shown, true, s.layout.x, s.layout.y, s.sheetGen,
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
