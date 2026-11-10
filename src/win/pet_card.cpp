#include "win/pet_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/pet_card.hpp"
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

namespace uagfx::win::petcardwin {
namespace {

namespace pt = uagfx::petcard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the pet window (where the manager keeps it, its class's vtable, its SendMsg), a window's shown flag;
// the pet as the game keeps it (0x01a2's): its name (24 bytes), level, fullness, closeness, the accessory worn
constexpr uint32_t kWindowMgr = 0x727ED8, kPetAt = kWindowMgr + 0x1B4, kPetClass = 0x697FD8, kPetSend = 0x464C50;
constexpr uint32_t kShown = 0x38, kSendMsgSlot = 0x94;
constexpr uint32_t kName = 0x771A78, kLevel = 0x771AA0, kHunger = 0x771AA4, kIntimacy = 0x771AA8;
constexpr uint32_t kAccessory = 0x771A98;
constexpr int kButtonMsg = 6;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

struct State {
    uisheet::Sheet sheet;  // 3.0.18-beta.34: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool on = false, shown = false;
    uint32_t hidden = 0;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    pt::Info info;
    pt::Show show;
    pt::Layout layout;
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

uint32_t gamePet() {
    const uint32_t w = read32(kPetAt);
    return w && read32(w) == kPetClass && read32(read32(w) + kSendMsgSlot) == kPetSend ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (on && window) {
        if (read32(window + kShown) && write32(window + kShown, 0) && s.hidden != window) {
            LOG_INFO("pet: the game's pet window hidden for the new card (its shown flag 0)");
            s.hidden = window;
        }
    } else if (window && s.hidden == window) {
        write32(window + kShown, 1);
        s.hidden = 0;
    } else if (!window) {
        s.hidden = 0;
    }
}

pt::Info infoOf(const State& s) {
    pt::Info i;
    char name[25] = {};
    readGame(kName, name, 24);
    i.name = utf8Of(widened(name, s.codePage));
    i.level = int32_t(read32(kLevel));
    i.hunger = int32_t(read32(kHunger));
    i.intimacy = int32_t(read32(kIntimacy));
    i.accessory = read32(kAccessory) != 0;
    return i;
}

pt::Texts texts() {
    pt::Texts t;
    t.title = say(settings::Text::PetTitle);
    t.level = say(settings::Text::PetLevel);
    t.hunger = say(settings::Text::PetHunger);
    t.intimacy = say(settings::Text::PetIntimacy);
    const settings::Text hunger[5] = {settings::Text::PetHungry0, settings::Text::PetHungry1,
                                      settings::Text::PetHungry2, settings::Text::PetHungry3,
                                      settings::Text::PetHungry4};
    const settings::Text close[5] = {settings::Text::PetClose0, settings::Text::PetClose1, settings::Text::PetClose2,
                                     settings::Text::PetClose3, settings::Text::PetClose4};
    for (int i = 0; i < 5; ++i) {
        t.hungerStates[i] = say(hunger[i]);
        t.intimacyStates[i] = say(close[i]);
    }
    t.commands[0] = say(settings::Text::PetFeed);
    t.commands[1] = say(settings::Text::PetPerform);
    t.commands[2] = say(settings::Text::PetUnequip);
    t.commands[3] = say(settings::Text::PetEgg);
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
    uapad::gamewindow::setBlocked(Blocker::Pet, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Pet, nullptr, nullptr, false);
    s.shown = false;
}

void command(State& s, int index) {
    const uint32_t window = gamePet();
    if (!window || index < 0 || index > 3 || !pt::allowed(s.info, pt::Command(index))) return;
    const char* const names[4] = {"feed", "perform", "the accessory off", "back into the egg"};
    if (s.told++ < 30) LOG_INFO("pet: %s - the window's menu choice %d (its message 0x27)", names[index], index);
    reinterpret_cast<SendMsgFn>(uintptr_t(kPetSend))(window, 0, pt::kMenuMsg, pt::gameChoice(pt::Command(index)), 0, 0);
    s.key.clear();
}

void closeIt(State& s) {
    const uint32_t window = gamePet();
    if (!window) return;
    if (s.told++ < 30) LOG_INFO("pet: closed (the window's 0x87)");
    reinterpret_cast<SendMsgFn>(uintptr_t(kPetSend))(window, 0, kButtonMsg, pt::kCloseButton, 0, 0);
    s.key.clear();
}

}  // namespace

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("pet: [interface] pet = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gamePet() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) {
        s.show = pt::Show{};
        LOG_INFO("pet: the game's pet window - the new card shows it");
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.info = infoOf(s);
    s.layout = pt::layout(targetWidth, targetHeight, s.scale);
    s.show.hover = -1;
    s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const pt::Hit h = pt::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == pt::Part::Button) s.show.hover = h.index;
        s.show.hoverClose = h.part == pt::Part::Close;
    }
    const pt::Info& i = s.info;
    const std::string key = i.name + ';' + std::to_string(i.level) + ',' + std::to_string(i.hunger) + ',' +
                            std::to_string(i.intimacy) + (i.accessory ? "a" : "") + ';' +
                            std::to_string(s.show.hover) + (s.show.hoverClose ? "x" : "") + ',' +
                            std::to_string(s.show.focus) + ';' + std::to_string(s.layout.x) + ',' +
                            std::to_string(s.layout.y) + '#' + std::to_string(uistyle::generation());
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.34
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.34 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): this window in RmlUi's
// markup, line by line as core petcard paints it (its boxes, icons, circles); the words where it places them
static void fillSheet(State& s, uisheet::Content& ui) {
    using namespace uagfx::petcard;
    const auto& L = s.layout;
    const auto& i = s.info;
    const auto& sh = s.show;
    [[maybe_unused]] constexpr uint32_t kCard = 0x10141C;
    [[maybe_unused]] constexpr uint32_t kWhite = 0xFFFFFF;
    [[maybe_unused]] constexpr uint32_t kGold = 0xC9A55B;
    [[maybe_unused]] constexpr uint32_t kGold2 = 0xE6CB8A;
    [[maybe_unused]] constexpr uint32_t kText = 0xEEF2F8;
    [[maybe_unused]] constexpr uint32_t kMuted = 0x9AA3B5;
    [[maybe_unused]] constexpr uint32_t kFull = 0x4CC777;
    [[maybe_unused]] constexpr uint32_t kLove = 0xF06B8B;
    [[maybe_unused]] constexpr float kW = 300;
    [[maybe_unused]] constexpr float kPad = 16;
    [[maybe_unused]] constexpr float kTitleH = 30;
    [[maybe_unused]] constexpr float kNameH = 24;
    [[maybe_unused]] constexpr float kRowH = 18;
    [[maybe_unused]] constexpr float kBarH = 8;
    [[maybe_unused]] constexpr float kButtonH = 34;
    [[maybe_unused]] constexpr float kGap = 8;
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
    ui.rect(ui.under, L.close, 6 * sc, kWhite, sh.hoverClose ? 0.14f : 0.06f);
    const auto bar = [&](const panel::Box& b, float share, uint32_t rgb) {
        ui.rect(ui.under, b, b.h / 2, kWhite, 0.08f);
        if (share > 0)
            ui.rect(ui.under, panel::Box{b.x, b.y, b.w * std::min(1.0f, share), b.h}, b.h / 2, rgb, 0.9f);
    };
    bar(L.hungerBar, float(std::clamp(i.hunger, 0, 100)) / 100.0f, kFull);
    bar(L.intimacyBar, float(std::clamp(i.intimacy, 0, 1000)) / 1000.0f, kLove);
    for (int k = 0; k < 4; ++k) {
        const bool can = pt::allowed(i, pt::Command(k)), hot = sh.hover == k || sh.focus == k;
        ui.rect(ui.under, L.buttons[k], 8 * sc, k == 0 ? kGold : kWhite,
                !can ? 0.02f : k == 0 ? (hot ? 0.34f : 0.22f) : (hot ? 0.12f : 0.06f),
                sh.focus == k ? kGold : k == 0 ? kGold : kWhite,
                sh.focus == k ? 0.95f : k == 0 ? 0.8f : 0.16f, (sh.focus == k ? 1.5f : 1.0f) * sc);
    }
    ui.say(pt::words(s.layout, s.info, s.show, texts()));
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    s.sheetShown = s.sheet.frame("pet", s.shown, true, s.layout.x, s.layout.y, s.sheetGen,
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
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    const pt::Hit h = pt::hitTest(s.layout, lx, ly);
    if (h.part == pt::Part::Button) command(s, h.index);
    else if (h.part == pt::Part::Close) closeIt(s);
    return true;
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    if (!s.shown) return false;
    closeIt(s);
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, gamePet(), true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    int& f = s.show.focus;
    if (f < 0) f = 0;
    switch (c) {
        case PadCommand::Left: f = f % 2 == 1 ? f - 1 : f; break;
        case PadCommand::Right: f = f % 2 == 0 ? f + 1 : f; break;
        case PadCommand::Up: f = f >= 2 ? f - 2 : f; break;
        case PadCommand::Down: f = f < 2 ? f + 2 : f; break;
        case PadCommand::Press: command(s, f); break;
        case PadCommand::Close: closeIt(s); break;
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

}  // namespace uagfx::win::petcardwin
