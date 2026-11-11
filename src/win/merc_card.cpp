#include "win/card_drag.hpp"
#include "win/merc_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "core/merc_block.hpp"
#include "core/companion_skills.hpp"
#include "core/merc_card.hpp"
#include "core/merc_packet.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/companion_skills.hpp"
#include "win/char_block.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/party_panel.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "uikit/companion_live.hpp"
#include "win/live_part.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::merccardwin {
namespace {

namespace mc = uagfx::merccard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the mercenary window (where the manager keeps it, its class's vtable, its SendMsg), a window's shown
// flag; the mercenary as the game keeps it: its name (24 bytes), the stats (ATK MATK HIT CRIT DEF MDEF FLEE, ints), the
// attack delay, its class (-1 while nothing is known), level, HP, its most, SP, its most, loyalty, summons, kills, the
// contract's end (a 32-bit time_t)
constexpr uint32_t kWindowMgr = 0x727ED8, kMercAt = kWindowMgr + 0x1EC, kMercClass = 0x6982D0;
constexpr uint32_t kMercSend = 0x468100, kShown = 0x38, kSendMsgSlot = 0x94;
constexpr uint32_t kName = 0x772EE0, kStats = 0x772F00, kDelay = 0x772F1C, kClass = 0x772F20, kLevel = 0x772F24;
constexpr uint32_t kHp = 0x772F28, kMaxHp = 0x772F2C, kSp = 0x772F30, kMaxSp = 0x772F34, kLoyalty = 0x772F38;
constexpr uint32_t kSummons = 0x772F3C, kKills = 0x772F40, kEnds = 0x772F48;
constexpr int kButtonMsg = 6;
// 3.0.11-beta.216: the game's «a mercenary hired» flag (its Ctrl+R looks at it), the window's number, the manager's
// DeleteWindow / MakeWindow (thiscall, the window's number); the mercenaries' classes
constexpr uint32_t kHired = 0x772F64, kDeleteWindow = 0x4D1E60, kMakeWindow = 0x4D42D0;
constexpr int kMercWindow = 0x84, kFirstClass = 6017, kLastClass = 6046;
using DeleteFn = bool(__attribute__((thiscall)) *)(uint32_t manager, int id);
using MakeFn = uint32_t(__attribute__((thiscall)) *)(uint32_t manager, int id);
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
    mc::Info info;
    mc::Show show;
    mc::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
    // 3.0.16-beta.31: its skills as the game keeps them (win/companion_skills: the rows when it has them - used,
    // their use level chosen, dragged onto the hotbar); a press on a skill's row (as win/homun_card)
    std::vector<companionwin::GameSkill> game;
    int pressRow = -1;
    float pressX = 0, pressY = 0;
    bool dragging = false;
};

State& state() {
    static State s;
    return s;
}

std::vector<mercpacket::Skill>& seenSkills() {  // 3.0.11-beta.218
    static std::vector<mercpacket::Skill> v;
    return v;
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

uint32_t gameMerc() {
    const uint32_t w = read32(kMercAt);
    return w && read32(w) == kMercClass && read32(read32(w) + kSendMsgSlot) == kMercSend ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (on && window) {
        if (read32(window + kShown) && write32(window + kShown, 0) && s.hidden != window) {
            LOG_INFO("merc: the game's mercenary window hidden for the new card (its shown flag 0)");
            s.hidden = window;
        }
    } else if (window && s.hidden == window) {
        write32(window + kShown, 1);
        s.hidden = 0;
    } else if (!window) {
        s.hidden = 0;
    }
}

mc::Info infoOf(const State& s) {
    mc::Info i;
    char name[25] = {};
    readGame(kName, name, 24);
    i.name = utf8Of(widened(name, s.codePage));
    i.kind = mc::kindOf(int32_t(read32(kClass)));
    i.level = int32_t(read32(kLevel));
    int32_t stats[7] = {};
    if (readGame(kStats, stats, sizeof stats))
        for (int k = 0; k < 7; ++k) i.stats[k] = stats[k];
    i.aspd = int32_t(read32(kDelay));
    i.hp = int32_t(read32(kHp));
    i.maxHp = int32_t(read32(kMaxHp));
    i.sp = int32_t(read32(kSp));
    i.maxSp = int32_t(read32(kMaxSp));
    i.loyalty = int32_t(read32(kLoyalty));
    i.summons = int32_t(read32(kSummons));
    i.kills = int32_t(read32(kKills));
    for (const mercpacket::Skill& k : seenSkills()) {  // 3.0.11-beta.218
        const std::string name = mercpacket::skillName(k.id);
        i.skills.push_back(mc::SkillRow{name.empty() ? "#" + std::to_string(k.id) : name, k.lv});
    }
    const time_t ends = time_t(int32_t(read32(kEnds)));
    char when[32] = {};
    const std::tm* lt = ends > 0 ? std::localtime(&ends) : nullptr;
    if (lt) std::strftime(when, sizeof when, "%d.%m %H:%M", lt);
    i.ends = when;
    return i;
}

mc::Texts texts() {
    mc::Texts t;
    t.kinds[0] = say(settings::Text::MercArcher);
    t.kinds[1] = say(settings::Text::MercSpearman);
    t.kinds[2] = say(settings::Text::MercSwordsman);
    t.level = say(settings::Text::PetLevel);
    t.facts[0] = say(settings::Text::MercEnds);
    t.facts[1] = say(settings::Text::MercLoyalty);
    t.facts[2] = say(settings::Text::MercSummons);
    t.facts[3] = say(settings::Text::MercKills);
    t.skills = say(settings::Text::HomunSkills);
    t.dismiss = say(settings::Text::MercDismiss);
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
    uapad::gamewindow::setBlocked(Blocker::Merc, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Merc, nullptr, nullptr, false);
    s.shown = false;
    s.pressRow = -1;  // 3.0.16-beta.31
    s.dragging = false;
}

// 3.0.16-beta.31: the rows from the game's own list when it has them (0x029d's as the game keeps them), each with
// the level it is used at
void readSkills(State& s) {
    s.game = companionwin::read(companionwin::Who::Merc);
    if (s.game.empty()) return;  // the packet's seen rows (not used, not dragged)
    s.info.skills.clear();
    for (const companionwin::GameSkill& k : s.game) {
        const std::string name = mercpacket::skillName(k.id);
        mc::SkillRow r{name.empty() ? "#" + std::to_string(k.id) : name, k.level};
        r.use = companion::useOf(companionwin::storedUse(companionwin::Who::Merc, k.id), k.level);
        r.usable = true;
        s.info.skills.push_back(r);
    }
}

void useSkill(State& s, int row) {
    if (row < 0 || size_t(row) >= s.game.size() || size_t(row) >= s.info.skills.size()) return;
    const int level = s.info.skills[size_t(row)].use;
    const bool sent = companionwin::use(companionwin::Who::Merc, s.game[size_t(row)], level);
    if (s.told++ < 30)
        LOG_INFO("merc: its skill %d used at level %d %s", s.game[size_t(row)].id, level,
                 sent ? "(0x7E, as the game's own skills window)" : "- not sent");
}

void changeUse(State& s, int row, int by) {
    if (row < 0 || size_t(row) >= s.game.size() || size_t(row) >= s.info.skills.size()) return;
    const mc::SkillRow& r = s.info.skills[size_t(row)];
    const int to = companion::stepped(r.use, r.level, by);
    if (to == r.use || !companionwin::setUse(companionwin::Who::Merc, s.game[size_t(row)].id, to)) return;
    if (s.told++ < 30) LOG_INFO("merc: its skill %d used at level %d now", s.game[size_t(row)].id, to);
    s.key.clear();
}

void endPress(State& s, float tx, float ty) {
    const int row = s.pressRow;
    s.pressRow = -1;
    if (row < 0 || size_t(row) >= s.game.size() || size_t(row) >= s.info.skills.size()) return;
    const bool off = !s.layout.glass.contains(tx - float(s.layout.x), ty - float(s.layout.y));
    if (s.dragging) {
        s.dragging = false;
        if (!off) return;
        const char* what = companionwin::drop(companionwin::Who::Merc, s.game[size_t(row)],
                                              s.info.skills[size_t(row)].use);
        if (s.told++ < 30) LOG_INFO("merc: its skill %d dragged off the card - %s", s.game[size_t(row)].id, what);
        return;
    }
    useSkill(s, row);
}

void press(State& s, mc::Button b) {
    const uint32_t window = gameMerc();
    if (!window) return;
    const char* const names[3] = {"its skills", "dismiss (the game asks first)", "close"};
    if (s.told++ < 30) LOG_INFO("merc: %s - the window's 0x%X", names[int(b)], unsigned(mc::gameButton(b)));
    reinterpret_cast<SendMsgFn>(uintptr_t(kMercSend))(window, 0, kButtonMsg, mc::gameButton(b), 0, 0);
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
        LOG_INFO("merc: [interface] merc = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameMerc() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || int32_t(read32(kClass)) == -1 || !game || k <= 0 || targetWidth <= 0 ||
        targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) {
        s.show = mc::Show{};
        LOG_INFO("merc: the game's mercenary window - the new card shows it");
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.info = infoOf(s);
    readSkills(s);  // 3.0.16-beta.31
    s.layout = mc::layout(targetWidth, targetHeight, s.scale, int(s.info.skills.size()));
    if (s.live.shown()) uikit::companion::refitMerc(s.live.doc.doc(), s.layout);  // 3.0.18-beta.98
    // 3.0.18-beta.92 (the owner: «не все окна перетаскиваются»): moved by a press on its empty part
    s.move.apply(s.layout.x, s.layout.y, s.layout.w, s.layout.h, targetWidth, targetHeight, k, 48 * s.scale);
    s.show.hover = s.show.hoverSkill = s.show.hoverLess = s.show.hoverMore = -1;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy)) {
        const mc::Hit h = mc::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == mc::Part::Button) s.show.hover = int(h.button);
        if (h.part == mc::Part::Skill) s.show.hoverSkill = h.index;  // 3.0.16-beta.31
        if (h.part == mc::Part::Less) s.show.hoverLess = h.index;
        if (h.part == mc::Part::More) s.show.hoverMore = h.index;
        if (s.pressRow >= 0) {  // a press on a row moved far - a drag; its button up - its end
            const float tx = float(cx) * float(k), ty = float(cy) * float(k);
            if (!s.dragging && companion::dragged(s.pressX, s.pressY, tx, ty, s.scale)) s.dragging = true;
            const int button = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
            if (GetAsyncKeyState(button) >= 0) endPress(s, tx, ty);
            if (s.dragging) s.show.hoverSkill = s.pressRow;
        }
    }
    const mc::Info& i = s.info;
    std::string key = i.name + ';' + std::to_string(int(i.kind)) + ',' + std::to_string(i.level) + ',' +
                      std::to_string(i.hp) + '/' + std::to_string(i.maxHp) + ',' + std::to_string(i.sp) + '/' +
                      std::to_string(i.maxSp) + ',' + i.ends + ',' + std::to_string(i.loyalty) + ',' +
                      std::to_string(i.summons) + ',' + std::to_string(i.kills) + ',' + std::to_string(i.aspd) + ';' +
                      std::to_string(s.show.hover) + (s.show.focus ? "f" : "") + ';' + std::to_string(s.layout.x) +
                      ',' + std::to_string(s.layout.y) + '#' + std::to_string(uistyle::generation());
    for (int st : i.stats) key += ',' + std::to_string(st);
    for (const mc::SkillRow& r : i.skills)
        key += ';' + r.name + std::to_string(r.level) + '/' + std::to_string(r.use) + (r.usable ? "u" : "");
    key += ';' + std::to_string(s.show.hoverSkill) + ',' + std::to_string(s.show.hoverLess) + ',' +
           std::to_string(s.show.hoverMore) + ',' + std::to_string(s.show.padRow);
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
    s.sheetShown = s.live.frame("merc", s.shown, s.layout.x, s.layout.y, s.sheetGen, cl::rml(), cl::bind,
                                [&](cl::Model& m) {
                                    m.ground = s.live.ground();
                                    cl::fillMerc(m, s.info, s.show, texts(), s.layout.s);
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
    const mc::Hit h = mc::hitTest(s.layout, lx, ly);
    if (h.part == mc::Part::None && m == WM_LBUTTONDOWN) s.move.begin(tx, ty);  // 3.0.18-beta.92
    if (h.part == mc::Part::Button) press(s, h.button);
    const bool usable = h.index >= 0 && size_t(h.index) < s.info.skills.size() && s.info.skills[size_t(h.index)].usable;
    if (usable && h.part == mc::Part::Less) changeUse(s, h.index, -1);  // 3.0.16-beta.31
    if (usable && h.part == mc::Part::More) changeUse(s, h.index, +1);
    if (usable && h.part == mc::Part::Skill && m == WM_LBUTTONDOWN) {  // used at the button's up, or dragged
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
    press(s, mc::Button::Close);
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, gameMerc(), true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    s.show.focus = true;
    if (c == PadCommand::Close) press(s, mc::Button::Close);  // 3.0.11-beta.218: its skills listed in it, no button
    // 3.0.16-beta.39: the usable rows (a gold edge) - Up / Down, Left / Right the use level, Press uses
    const int rows = int(std::min({s.info.skills.size(), s.layout.skillRows.size(), s.game.size()}));
    const auto usable = [&](int r) { return r >= 0 && r < rows && s.info.skills[size_t(r)].usable; };
    int& row = s.show.padRow;
    if (!usable(row)) {
        row = -1;
        for (int r = 0; r < rows && row < 0; ++r)
            if (usable(r)) row = r;
    }
    const auto step = [&](int by) {
        for (int r = row + by; r >= 0 && r < rows; r += by)
            if (usable(r)) return r;
        return row;
    };
    if (row >= 0) switch (c) {
            case PadCommand::Up: row = step(-1); break;
            case PadCommand::Down: row = step(+1); break;
            case PadCommand::Left: changeUse(s, row, -1); break;
            case PadCommand::Right: changeUse(s, row, +1); break;
            case PadCommand::Press: useSkill(s, row); break;
            default: break;
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

// 3.0.11-beta.216: the mercenary's block
namespace {

namespace mb = uagfx::mercblock;

struct BlockState {
    uisheet::Sheet sheet;  // 3.0.18-beta.39: the block drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    std::vector<Words> words;
    bool on = false, shown = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    mb::Info info;
    mb::Show show;
    mb::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false, told = false;
};

BlockState& blockState() {
    static BlockState s;
    return s;
}

mb::Info blockInfo(unsigned codePage) {
    mb::Info i;
    char name[25] = {};
    readGame(kName, name, 24);
    i.name = utf8Of(widened(name, codePage));
    i.kind = mc::kindOf(int32_t(read32(kClass)));
    i.level = int32_t(read32(kLevel));
    i.hp = int32_t(read32(kHp));
    i.maxHp = int32_t(read32(kMaxHp));
    i.sp = int32_t(read32(kSp));
    i.maxSp = int32_t(read32(kMaxSp));
    i.loyalty = int32_t(read32(kLoyalty));
    const int64_t ends = int32_t(read32(kEnds));
    i.secondsLeft = ends > 0 ? int(std::max<int64_t>(0, ends - int64_t(std::time(nullptr)))) : 0;
    return i;
}

void blockRender(BlockState& s) {
    const uicost::Paint cost("the mercenary's block", s.layout.w, s.layout.h);
    const bool byUaui = uaui::ready();  // 3.0.18-beta.39: uaUI's block - the words kept, no picture
    if (!byUaui && !panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    if (!byUaui) mb::paint(s.picture, s.layout, s.info, s.show);
    mb::Texts t;
    t.kinds[0] = say(settings::Text::MercArcher);
    t.kinds[1] = say(settings::Text::MercSpearman);
    t.kinds[2] = say(settings::Text::MercSwordsman);
    t.level = say(settings::Text::MercLevelShort);
    t.left = mb::leftText(s.info.secondsLeft, say(settings::Text::MercLeftMinutes),
                          say(settings::Text::MercLeftHours));
    t.loyalty = say(settings::Text::MercLoyalty);
    std::vector<Words> out;
    for (const mb::Words& w : mb::words(s.layout, s.info, t))
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == mb::Align::Right    ? DT_RIGHT
                                 : w.align == mb::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    if (byUaui) {
        s.words = std::move(out);
        ++s.sheetGen;
        return;
    }
    drawWords(s.picture, out);
    s.upload = true;
}

void blockHide(BlockState& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::MercBlock, nullptr, nullptr, false);
    s.shown = false;
}

// Under the party's panel, else under the character block, else at the top left
void blockPlace(BlockState& s) {
    int x = 0, y = 0, w = 0, h = 0;
    const int gap = int(std::lround(8 * s.scale));
    int width = 0;
    if (partywin::panelBox(x, y, w, h)) {
        width = w;
        y += h + gap;
    } else if (charpanel::panelBox(x, y, w, h)) {
        width = w;
        y += h + gap;
    } else {
        x = int(std::lround(10 * s.scale));
        y = int(std::lround(10 * s.scale));
    }
    s.layout = mb::layout(x, y, width, s.scale);
}

}  // namespace

void skillsSeen(const uint8_t* packet, size_t size) {
    seenSkills() = mercpacket::skills(packet, size);
    std::string list;
    for (const mercpacket::Skill& k : seenSkills()) list += ' ' + std::to_string(k.id) + '/' + std::to_string(k.lv);
    LOG_INFO("merc: its skills (0x029d):%s", list.empty() ? " none" : list.c_str());
    state().key.clear();
}

bool hired() {
    if (!inGameWorld() || !read32(kHired)) return false;
    const int cls = int32_t(read32(kClass));
    return cls >= kFirstClass && cls <= kLastClass;
}

bool toggleCard() {
    if (!inGameWorld()) return false;
    if (reinterpret_cast<DeleteFn>(uintptr_t(kDeleteWindow))(kWindowMgr, kMercWindow)) {
        LOG_INFO("merc: its card closed from the block (the game's window 0x84 deleted)");
        return true;
    }
    if (!read32(kHired)) return false;
    const uint32_t made = reinterpret_cast<MakeFn>(uintptr_t(kMakeWindow))(kWindowMgr, kMercWindow);
    LOG_INFO("merc: its card opened from the block (the game's window 0x84 %s)", made ? "made" : "not made");
    return made != 0;
}

static void blockFrameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                            int targetHeight) {
    BlockState& s = blockState();
    s.on = on;
    s.codePage = codePage;
    if (!on || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || !hired()) {
        blockHide(s);
        return;
    }
    if (!s.told) {
        s.told = true;
        LOG_INFO("merc: a mercenary hired - its block shows (class %d)", int32_t(read32(kClass)));
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.info = blockInfo(codePage);
    blockPlace(s);
    s.show.open = gameMerc() != 0;
    s.show.hover = false;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy))
        s.show.hover = mb::hit(s.layout, float(cx) * float(k) - float(s.layout.x), float(cy) * float(k) -
                                                                                  float(s.layout.y));
    const mb::Info& i = s.info;
    const std::string key = i.name + ';' + std::to_string(int(i.kind)) + ',' + std::to_string(i.level) + ',' +
                            std::to_string(i.hp) + '/' + std::to_string(i.maxHp) + ',' + std::to_string(i.sp) + '/' +
                            std::to_string(i.maxSp) + ',' + std::to_string(i.loyalty) + ',' +
                            std::to_string((i.secondsLeft + 59) / 60) + (mb::ending(i.secondsLeft) ? "e" : "") +
                            (s.show.open ? "o" : "") + (s.show.hover ? "h" : "") + ';' + std::to_string(s.layout.w) +
                            '#' + std::to_string(uistyle::generation()) + (uaui::ready() ? "u" : "");
    if (key != s.key) {
        s.key = key;
        blockRender(s);
    }
    s.shown = true;
    RECT client{};
    if (!GetClientRect(game, &client)) return;
    const RECT r{LONG(int64_t(s.layout.x) * client.right / targetWidth),
                 LONG(int64_t(s.layout.y) * client.bottom / targetHeight),
                 LONG(int64_t(s.layout.x + s.layout.w) * client.right / targetWidth + 1),
                 LONG(int64_t(s.layout.y + s.layout.h) * client.bottom / targetHeight + 1)};
    uapad::gamewindow::setBlocked(Blocker::MercBlock, game, &r, false);
}

// 3.0.18-beta.39 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the mercenary's block in
// RmlUi's markup, line by line as core mercblock paints it - its glass (gold while its window is open), the icon, the
// HP and SP bars; the words where core mercblock places them
static void blockSheet(BlockState& s, uisheet::Content& c) {
    const mb::Layout& L = s.layout;
    const float sc = L.s;
    constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kHp = 0x5BC07A;
    constexpr uint32_t kSp = 0x5B8CE0;
    c.w = float(L.w), c.h = float(L.h);
    c.rect(c.under, L.glass, 12 * sc, kCard, std::max(0.85f, uistyle::panel()), s.show.open ? kGold2 : kWhite,
           s.show.open ? 0.9f : s.show.hover ? 0.22f : 0.10f, (s.show.open ? 1.6f : 1.0f) * sc);
    const uint32_t rgb = s.show.open || s.show.hover ? kGold2 : kGold;
    const int px = std::max(4, int(std::ceil(L.icon.w)));
    const std::string src = s.sheet.made("battle" + std::to_string(px) + ':' + std::to_string(rgb), px, px,
                                         [&](panel::Picture& p) {
                                             uiicons::draw(p, uiicons::Icon::Battle, 0, 0, float(px), rgb, 1.0f, 1.8f);
                                         });
    c.icons.push_back(uisheet::Icon{L.icon.x, L.icon.y, float(px), float(px), 1, src});
    const auto share = [](int now, int most) {
        return most > 0 ? std::clamp(float(now) / float(most), 0.0f, 1.0f) : 0.0f;
    };
    const auto bar = [&](const panel::Box& b, float part, uint32_t colour) {
        c.rect(c.under, b, b.h / 2, kWhite, 0.08f);
        if (part > 0) c.rect(c.under, panel::Box{b.x, b.y, std::max(b.h, b.w * part), b.h}, b.h / 2, colour, 0.92f);
    };
    bar(L.hp, share(s.info.hp, s.info.maxHp), kHp);
    bar(L.sp, share(s.info.sp, s.info.maxSp), kSp);
    uisheet::sayGdi(c, s.words);
}

void blockFrame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    blockFrameInner(game, on, codePage, k, targetWidth, targetHeight);
    BlockState& s = blockState();
    s.sheetShown = s.sheet.frame("merc block", s.shown, false, s.layout.x, s.layout.y, s.sheetGen,
                                 [&](uisheet::Content& c) { blockSheet(s, c); });
}

void blockDraw(FrameRecord& record, gpu::Renderer& renderer) {
    BlockState& s = blockState();
    if (!s.shown || s.sheetShown || s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool blockMouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    BlockState& s = blockState();
    (void)wp;
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    if (!mb::hit(s.layout, tx - float(s.layout.x), ty - float(s.layout.y))) return false;
    result = 0;  // the click is the block's, not the world's
    if (m == WM_LBUTTONDOWN) {
        toggleCard();
        s.key.clear();
    }
    return true;
}

}  // namespace uagfx::win::merccardwin
