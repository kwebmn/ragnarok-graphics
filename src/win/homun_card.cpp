#include "win/card_drag.hpp"
#include "win/homun_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/companion_skills.hpp"
#include "core/homun_card.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/companion_skills.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "uikit/companion_live.hpp"
#include "win/live_part.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::homuncardwin {
namespace {

namespace hc = uagfx::homuncard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the homunculus window (where the manager keeps it, its class's vtable, its SendMsg), a window's shown
// flag; the homunculus as the game keeps it (the server's 0x022e): its name (24 bytes), level, the stats (ATK MATK HIT
// CRIT DEF MDEF FLEE, ints), the attack delay, HP, its most, SP, its most, the experience, its most, fullness,
// closeness (the level -1 while nothing is known)
constexpr uint32_t kWindowMgr = 0x727ED8, kHomunAt = kWindowMgr + 0x1E4, kHomunClass = 0x6981A0;
constexpr uint32_t kHomunSend = 0x467210, kShown = 0x38, kSendMsgSlot = 0x94;
constexpr uint32_t kName = 0x772E38, kStats = 0x772E58, kDelay = 0x772E74, kKnown = 0x772E78, kLevel = 0x772E7C;
constexpr uint32_t kHp = 0x772E80, kMaxHp = 0x772E84, kSp = 0x772E88, kMaxSp = 0x772E8C, kExp = 0x772E90;
constexpr uint32_t kMaxExp = 0x772E94, kHunger = 0x772E98, kIntimacy = 0x772EA0;
constexpr int kButtonMsg = 6;
// 3.0.11-beta.221: its skills as the game keeps them (the server's 0x0235, FUN_005891e0: a std::list at 0x771438, a
// node's record at +8 - 1, the id, its kind, level, SP, can rise, range, its name), its skill points (0x022e's at 67);
// the game mode's SendMsg - 0x4A raises a skill (0x0112), 0x7E uses one (a record, the level: as the game's own skills
// window does for a homunculus' FUN_004bdd90)
constexpr uint32_t kSkillPoints = 0x771FAC;  // 3.0.16-beta.31: the list read by win/companion_skills
constexpr uint32_t kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0;
constexpr int kRaiseMsg = 0x4A;
constexpr size_t kMostSkills = 20;
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

struct State {
    CardDrag move;  // 3.0.18-beta.92: moved by its empty part
    // 3.0.18-beta.98 (docs/superpowers/plans/2026-11-10-uaui-live-drag.md): live - laid out by RmlUi
    LivePart<uikit::companion::Model> live;
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool on = false, shown = false;
    uint32_t hidden = 0;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    hc::Info info;
    hc::Show show;
    hc::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
    std::vector<uint32_t> records;  // 3.0.11-beta.221: the game's records of its skills, the list's order
    std::vector<int> ids;
    // 3.0.16-beta.31: its skills as the game keeps them (win/companion_skills); a press on a skill's row - a click
    // uses it at its use level, a drag lets it go onto the hotbar (the target's pixels where it began)
    std::vector<companionwin::GameSkill> game;
    int pressRow = -1;
    float pressX = 0, pressY = 0;
    bool dragging = false;
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

uint32_t gameHomun() {
    const uint32_t w = read32(kHomunAt);
    return w && read32(w) == kHomunClass && read32(read32(w) + kSendMsgSlot) == kHomunSend ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (on && window) {
        if (read32(window + kShown) && write32(window + kShown, 0) && s.hidden != window) {
            LOG_INFO("homun: the game's homunculus window hidden for the new card (its shown flag 0)");
            s.hidden = window;
        }
    } else if (window && s.hidden == window) {
        write32(window + kShown, 1);
        s.hidden = 0;
    } else if (!window) {
        s.hidden = 0;
    }
}

hc::Info infoOf(const State& s) {
    hc::Info i;
    char name[25] = {};
    readGame(kName, name, 24);
    i.name = utf8Of(widened(name, s.codePage));
    i.level = int32_t(read32(kLevel));
    int32_t stats[7] = {};
    if (readGame(kStats, stats, sizeof stats))
        for (int k = 0; k < 7; ++k) i.stats[k] = stats[k];
    i.aspd = int32_t(read32(kDelay));
    i.hp = int32_t(read32(kHp));
    i.maxHp = int32_t(read32(kMaxHp));
    i.sp = int32_t(read32(kSp));
    i.maxSp = int32_t(read32(kMaxSp));
    i.exp = int32_t(read32(kExp));
    i.maxExp = int32_t(read32(kMaxExp));
    i.hunger = int32_t(read32(kHunger));
    i.intimacy = int32_t(read32(kIntimacy));
    i.points = int32_t(read32(kSkillPoints));  // 3.0.11-beta.221
    return i;
}

// 3.0.11-beta.221: its skills from the game's list
void readSkills(State& s) {
    s.records.clear();
    s.ids.clear();
    s.info.skills.clear();
    s.game = companionwin::read(companionwin::Who::Homun);  // 3.0.16-beta.31
    if (s.game.size() > kMostSkills) s.game.resize(kMostSkills);
    for (const companionwin::GameSkill& k : s.game) {
        const std::string name = hc::skillName(k.id);
        s.records.push_back(k.node + 8);
        s.ids.push_back(k.id);
        hc::SkillRow row{name.empty() ? "#" + std::to_string(k.id) : name, k.level, k.up};
        row.use = companion::useOf(companionwin::storedUse(companionwin::Who::Homun, k.id), k.level);
        s.info.skills.push_back(row);
    }
}

bool modeMsg(int msg, int a, int b, int c) {
    const uint32_t mode = read32(kModeNow);
    if (!mode || !inGameWorld() || read32(read32(mode) + kModeMsgSlot) != kModeMsg) return false;
    reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, msg, a, b, c);
    return true;
}

void raiseSkill(State& s, int row) {
    if (row < 0 || size_t(row) >= s.ids.size() || !s.info.skills[size_t(row)].up || s.info.points <= 0) return;
    const bool sent = modeMsg(kRaiseMsg, s.ids[size_t(row)], 0, 0);
    if (s.told++ < 30) LOG_INFO("homun: its skill %d +1 %s", s.ids[size_t(row)], sent ? "asked (0x4A)" : "not asked");
    s.key.clear();
}

void useSkill(State& s, int row) {
    if (row < 0 || size_t(row) >= s.game.size() || s.info.skills[size_t(row)].level <= 0) return;
    const int level = s.info.skills[size_t(row)].use;  // 3.0.16-beta.31: at its use level
    const bool sent = companionwin::use(companionwin::Who::Homun, s.game[size_t(row)], level);
    if (s.told++ < 30)
        LOG_INFO("homun: its skill %d used at level %d %s", s.ids[size_t(row)], level,
                 sent ? "(0x7E, as the game's own skills window)" : "- not sent");
}

// 3.0.16-beta.31: its use level one down or up (kept where the game's own window keeps it)
void changeUse(State& s, int row, int by) {
    if (row < 0 || size_t(row) >= s.game.size()) return;
    const hc::SkillRow& r = s.info.skills[size_t(row)];
    const int to = companion::stepped(r.use, r.level, by);
    if (to == r.use || !companionwin::setUse(companionwin::Who::Homun, s.ids[size_t(row)], to)) return;
    if (s.told++ < 30) LOG_INFO("homun: its skill %d used at level %d now", s.ids[size_t(row)], to);
    s.key.clear();
}

// 3.0.16-beta.31: a press on a skill's row ended - let go onto the hotbar when dragged off the card, else used
void endPress(State& s, float tx, float ty) {
    const int row = s.pressRow;
    s.pressRow = -1;
    if (row < 0 || size_t(row) >= s.game.size()) return;
    const bool off = !s.layout.glass.contains(tx - float(s.layout.x), ty - float(s.layout.y));
    if (s.dragging) {
        s.dragging = false;
        if (!off) return;
        const char* what = companionwin::drop(companionwin::Who::Homun, s.game[size_t(row)],
                                              s.info.skills[size_t(row)].use);
        if (s.told++ < 30) LOG_INFO("homun: its skill %d dragged off the card - %s", s.ids[size_t(row)], what);
        return;
    }
    useSkill(s, row);
}

hc::Texts texts() {
    hc::Texts t;
    t.title = say(settings::Text::HomunTitle);
    t.level = say(settings::Text::PetLevel);
    t.hunger = say(settings::Text::PetHunger);
    t.intimacy = say(settings::Text::PetIntimacy);
    const settings::Text close[8] = {settings::Text::HomunClose0, settings::Text::HomunClose1,
                                     settings::Text::HomunClose2, settings::Text::PetClose1,
                                     settings::Text::PetClose2,   settings::Text::PetClose3,
                                     settings::Text::PetClose4,   settings::Text::HomunClose7};
    for (int i = 0; i < 8; ++i) t.intimacyStates[i] = say(close[i]);
    t.feed = say(settings::Text::PetFeed);
    t.skills = say(settings::Text::HomunSkills);
    t.remove = say(settings::Text::HomunDelete);
    t.skillsHead = say(settings::Text::HomunSkillsHead);  // 3.0.11-beta.221
    t.skillLevel = say(settings::Text::MercLevelShort);
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
    uapad::gamewindow::setBlocked(Blocker::Homun, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Homun, nullptr, nullptr, false);
    s.shown = false;
    s.pressRow = -1;  // 3.0.16-beta.31
    s.dragging = false;
}

void press(State& s, hc::Button b) {
    const uint32_t window = gameHomun();
    if (!window) return;
    const char* const names[4] = {"feed", "its skills", "delete (the game asks first)", "close"};
    if (s.told++ < 30) LOG_INFO("homun: %s - the window's 0x%X", names[int(b)], unsigned(hc::gameButton(b)));
    reinterpret_cast<SendMsgFn>(uintptr_t(kHomunSend))(window, 0, kButtonMsg, hc::gameButton(b), 0, 0);
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
        LOG_INFO("homun: [interface] homun = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameHomun() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || int32_t(read32(kKnown)) == -1 || !game || k <= 0 || targetWidth <= 0 ||
        targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) {
        s.show = hc::Show{};
        LOG_INFO("homun: the game's homunculus window - the new card shows it");
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.info = infoOf(s);
    readSkills(s);
    s.layout = hc::layout(targetWidth, targetHeight, s.scale, int(s.info.skills.size()));
    if (s.live.shown()) uikit::companion::refitHomun(s.live.doc.doc(), s.layout);  // 3.0.18-beta.98
    // 3.0.18-beta.92 (the owner: «не все окна перетаскиваются»): moved by a press on its empty part
    s.move.apply(s.layout.x, s.layout.y, s.layout.w, s.layout.h, targetWidth, targetHeight, k, 48 * s.scale);
    s.show.hover = -1;
    s.show.hoverPlus = s.show.hoverSkill = s.show.hoverLess = s.show.hoverMore = -1;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy)) {
        const hc::Hit h = hc::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == hc::Part::Button) s.show.hover = int(h.button);
        if (h.part == hc::Part::Plus) s.show.hoverPlus = h.index;
        if (h.part == hc::Part::Skill) s.show.hoverSkill = h.index;
        if (h.part == hc::Part::Less) s.show.hoverLess = h.index;  // 3.0.16-beta.31
        if (h.part == hc::Part::More) s.show.hoverMore = h.index;
        if (s.pressRow >= 0) {  // 3.0.16-beta.31: a press on a row moved far - a drag; its button up - its end
            const float tx = float(cx) * float(k), ty = float(cy) * float(k);
            if (!s.dragging && companion::dragged(s.pressX, s.pressY, tx, ty, s.scale)) s.dragging = true;
            const int button = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
            if (GetAsyncKeyState(button) >= 0) endPress(s, tx, ty);
            if (s.dragging) s.show.hoverSkill = s.pressRow;
        }
    }
    const hc::Info& i = s.info;
    std::string key = i.name + ';' + std::to_string(i.level) + ',' + std::to_string(i.hp) + '/' +
                      std::to_string(i.maxHp) + ',' + std::to_string(i.sp) + '/' + std::to_string(i.maxSp) + ',' +
                      std::to_string(i.exp) + '/' + std::to_string(i.maxExp) + ',' + std::to_string(i.hunger) + ',' +
                      std::to_string(i.intimacy) + ',' + std::to_string(i.aspd) + ';' + std::to_string(s.show.hover) +
                      ',' + std::to_string(s.show.focus) + ';' + std::to_string(s.layout.x) + ',' +
                      std::to_string(s.layout.y) + '#' + std::to_string(uistyle::generation());
    for (int st : i.stats) key += ',' + std::to_string(st);
    key += ";p" + std::to_string(i.points) + ',' + std::to_string(s.show.hoverPlus) + ',' +
           std::to_string(s.show.hoverSkill);
    for (const hc::SkillRow& r : i.skills)
        key += ';' + r.name + std::to_string(r.level) + '/' + std::to_string(r.use) + (r.up ? "+" : "");
    key += ';' + std::to_string(s.show.hoverLess) + ',' + std::to_string(s.show.hoverMore) + ',' +
           std::to_string(s.show.padRow);
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.34
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    namespace cl = uikit::companion;
    s.sheetShown = s.live.frame("homun", s.shown, s.layout.x, s.layout.y, s.sheetGen, cl::rml(), cl::bind,
                                [&](cl::Model& m) {
                                    m.ground = s.live.ground();
                                    cl::fillHomun(m, s.info, s.show, texts(), s.layout.s);
                                });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    s.live.raise();  // 3.0.18-beta.43: at its place in the windows' stack
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
    if (m == WM_LBUTTONUP) s.move.end();  // 3.0.18-beta.92
    (void)wp;
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (m == WM_LBUTTONUP && s.pressRow >= 0) endPress(s, tx, ty);  // 3.0.16-beta.31
    if (!s.layout.glass.contains(lx, ly)) return false;
    result = 0;
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    const hc::Hit h = hc::hitTest(s.layout, lx, ly);
    if (h.part == hc::Part::None && m == WM_LBUTTONDOWN) s.move.begin(tx, ty);  // 3.0.18-beta.92
    if (h.part == hc::Part::Button) press(s, h.button);
    if (h.part == hc::Part::Plus) raiseSkill(s, h.index);  // 3.0.11-beta.221
    if (h.part == hc::Part::Less) changeUse(s, h.index, -1);  // 3.0.16-beta.31
    if (h.part == hc::Part::More) changeUse(s, h.index, +1);
    if (h.part == hc::Part::Skill && m == WM_LBUTTONDOWN) {  // 3.0.16-beta.31: used at the button's up, or dragged
        s.pressRow = h.index;
        s.pressX = tx;
        s.pressY = ty;
        s.dragging = false;
    }
    return true;
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    if (!s.shown) return false;
    press(s, hc::Button::Close);
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, gameHomun(), true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    int& f = s.show.focus;  // feed or its skills
    int& row = s.show.padRow;  // 3.0.16-beta.39: or a skill's row of the card's list
    const int rows = int(std::min(s.info.skills.size(), s.layout.skillRows.size()));
    if (rows <= 0) row = -1;
    if (row < 0 && f != int(hc::Button::Feed) && f != int(hc::Button::Skills)) f = int(hc::Button::Feed);
    switch (c) {
        case PadCommand::Up:
            if (row > 0) --row;
            else if (row == 0) row = -1, f = int(hc::Button::Feed);
            break;
        case PadCommand::Down:
            if (rows > 0) row = std::min(rows - 1, row + 1), f = -1;
            break;
        case PadCommand::Left:
            if (row >= 0) changeUse(s, row, -1);
            else f = int(hc::Button::Feed);
            break;
        case PadCommand::Right:
            if (row >= 0) changeUse(s, row, +1);
            else if (rows <= 0) f = int(hc::Button::Skills);
            break;
        case PadCommand::Press:
            if (row >= 0) useSkill(s, row);
            else press(s, hc::Button(f));
            break;
        case PadCommand::Raise:
            if (row >= 0) raiseSkill(s, row);
            break;
        case PadCommand::Close: press(s, hc::Button::Close); break;
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

}  // namespace uagfx::win::homuncardwin
