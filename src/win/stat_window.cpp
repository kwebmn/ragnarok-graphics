#include "win/stat_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "core/char_status.hpp"
#include "core/layout.hpp"
#include "core/settings/strings.hpp"
#include "core/stat_math.hpp"
#include "core/stat_window.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/buff_overlay.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "uikit/stats_live.hpp"
#include "win/live_part.hpp"
#include "win/ui_sheet.hpp"
#include "win/window_stack.hpp"

namespace uagfx::win::statswin {
namespace {

namespace sw = uagfx::statwin;

// uaRO.exe's: the windows' manager's «Характеристики» (UI window 0xB, its class's vtable, its SendMsg FUN_004722a0 -
// message 6 with 0x87 its close button), a window's shown flag; the game mode's SendMsg (FUN_005707d0, vtable + 0x14)
// and its message 0x20 a stat's point (13 STR .. 18 LUK, 1)
constexpr uint32_t kWindowMgr = 0x727ED8, kStatsAt = kWindowMgr + 0x80, kStatsClass = 0x698700;
constexpr uint32_t kShown = 0x38, kSendMsgSlot = 0x94, kStatsSendMsg = 0x4722A0;
constexpr int kButtonMsg = 6, kCloseButton = 0x87;
constexpr uint32_t kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0;
constexpr int kRaiseMsg = 0x20, kFirstStat = 13;
constexpr ULONGLONG kReadMs = 200;

using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);

struct State {
    // 3.0.18-beta.101 (docs/superpowers/plans/2026-11-10-uaui-live-drag.md): live - its head by RmlUi, its body layered
    LivePart<uikit::stats::Model> live;
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;  // 3.0.11-beta.49
    WindowEdge edge;  // 3.0.11-beta.108: the top of two or more windows (win/window_stack)
    bool on = false, shown = false, hid = false, stateRead = false;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    float fx = 0.57f, fy = 0.2f;  // its place: a share of the free room to its left and above it (the concept's)
    sw::Data data;
    ULONGLONG readAt = 0;
    sw::Show show;
    int tab = 0, refJob = -1;  // 3.0.11-beta.267: the tab shown, the class «Довідка» tells of (-1 the character's)
    int planFor = -1;          // .268: the level and the free points the plan was last made from
    // 3.0.11-beta.296: the plans kept by character (uaro-graphics\stat-plans.txt), whose this one is, what was written
    std::map<std::string, sw::SavedPlan> plans;
    bool plansRead = false;
    std::string planOwner, plansWritten;
    int padRow = 0;  // .296: «Планування»'s row the controller is on - 0 the level, 1-6 the stats, 7 the ASPD aimed at,
                     // .297: 8-10 the buffs
    sw::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    bool moving = false;
    float grabX = 0, grabY = 0;
    int told = 0, pressesTold = 0;
};

constexpr int kPadRows = sw::kStats + 2 + sw::kPlanBuffs;  // .297

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

uint32_t gameWindow() {
    const uint32_t w = read32(kStatsAt);
    return w && read32(w) == kStatsClass ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (!window) {
        s.hid = false;
        return;
    }
    if (on) {
        if (read32(window + kShown) && write32(window + kShown, 0) && !s.hid) {
            LOG_INFO("stats: the game's «Характеристики» window hidden for the new one (its shown flag 0)");
            s.hid = true;
        }
    } else if (s.hid) {
        write32(window + kShown, 1);
        s.hid = false;
        LOG_INFO("stats: the game's «Характеристики» window back");
    }
}

void readState(State& s) {
    if (s.stateRead) return;
    s.stateRead = true;
    std::string text;
    float x = 0, y = 0;
    if (readTextFile(layout::join(gameDir(), layout::kStatsState), text) &&
        std::sscanf(text.c_str(), "at %f %f", &x, &y) == 2 && x >= 0 && x <= 1 && y >= 0 && y <= 1) {
        s.fx = x;
        s.fy = y;
    }
}

// 3.0.11-beta.296: the plans across the game's starts - read once, a character's taken when its name is known,
// written when one changed
void readPlans(State& s) {
    if (s.plansRead) return;
    s.plansRead = true;
    std::string text;
    if (readTextFile(layout::join(gameDir(), layout::kStatPlans), text)) s.plans = sw::parsePlans(text);
    s.plansWritten = sw::plansText(s.plans);
}

void keepPlans(State& s) {
    if (s.planOwner.empty()) return;
    s.plans[s.planOwner] = sw::SavedPlan{s.show.plan, s.show.aspdGoal};
    const std::string text = sw::plansText(s.plans);
    if (text == s.plansWritten) return;
    const std::string path = layout::join(gameDir(), layout::kStatPlans);
    if (writeFileAtomic(path, text))
        s.plansWritten = text;
    else if (s.told++ < 12)
        LOG_INFO("stats: %s cannot be written", path.c_str());
}

// .297, .298: the plan buffs' skills as learned, from the session's skill list (win/skill_window's: a node's record
// at +8 - the skill +4, its level +12)
constexpr uint32_t kSkillList = 0x771148 + 0x2D8;
void readLearned(sw::Data& d) {
    for (int& l : d.learned) l = 0;
    const uint32_t head = read32(kSkillList);
    if (!head) return;
    uint32_t node = read32(head);
    for (int n = 0; node && node != head && n < 600; ++n, node = read32(node)) {
        int32_t r[4] = {};
        if (!readGame(node + 8, r, sizeof r)) break;
        for (int i = 0; i < sw::kPlanBuffs; ++i)
            if (sw::kPlanBuffSpecs[i].skill && r[1] == sw::kPlanBuffSpecs[i].skill)
                d.learned[i] = std::clamp(int(r[3]), 0, 10);
    }
}

// .296: the map's monsters, the most spawned first, no bosses (the site's pack, as «Довідник»)
void readMapMobs(sw::Data& d) {
    std::string map = readGameMap();
    const size_t dot = map.rfind('.');
    if (dot != std::string::npos) map.resize(dot);
    const auto index = reference::index();
    if (map == d.map && (!d.mapMobs.empty() || !index)) return;
    d.map = map;
    d.mapMobs.clear();
    if (!index) return;
    const auto hints = index->maps.find(map);
    if (hints == index->maps.end()) return;
    for (const auto& on : hints->second.mobs) {
        const auto m = index->mobs.find(on.id);
        if (m == index->mobs.end() || m->second.boss) continue;
        d.mapMobs.push_back(sw::MapMob{on.id, m->second.lv, m->second.hp, m->second.name});
        if (int(d.mapMobs.size()) >= sw::kMobRows * 2) break;  // some may be none of the server's
    }
}

void keepState(const State& s) {
    char text[48];
    std::snprintf(text, sizeof text, "at %.4f %.4f\n", s.fx, s.fy);
    const std::string path = layout::join(gameDir(), layout::kStatsState);
    if (!writeFileAtomic(path, std::string(text))) LOG_INFO("stats: %s cannot be written", path.c_str());
}

// 3.0.11-beta.264 (the «Характеристики» concept, stage 1): the server's numbers against core/stat_math's from the
// stats alone - told the log once a change (20 at most): with nothing worn and no buffs they must be the same; a
// difference elsewhere is the things' and the buffs' (or a formula of ours to look at)
constexpr uint32_t kJobAt = 0x771F8C;
void serverCheck(const charstatus::Status& st) {
    namespace sm = uagfx::statmath;
    static std::string told;
    static int times = 0;
    const int job = int(read32(kJobAt)), lv = st.baseLevel;
    if (lv <= 0 || times >= 20) return;
    int t[sm::kStats];
    for (int i = 0; i < sm::kStats; ++i) t[i] = st.statBase[i] + st.statBonus[i];
    std::string aspdWeapons;  // the job's weapons that give the server's ASPD with these stats
    for (int w = 0; w < sm::kWeapons; ++w) {
        const int a = sm::amotion(job, w, t[sm::Agi], t[sm::Dex]);
        if (a > 0 && sm::aspd10(a) / 10 == st.aspd) aspdWeapons += (aspdWeapons.empty() ? "" : ",") + std::to_string(w);
    }
    char line[512];
    std::snprintf(line, sizeof line,
                  "job %d lv %d/%d: ATK %d (ours %d melee, %d ranged, the weapon's the rest), MATK %d~%d (%d~%d), "
                  "HIT %d (%d), FLEE %d (%d), "
                  "CRIT %d (%d.%d), MDEF+%d (%d), HP %d (%d), SP %d (%d), weight %d (%d), ASPD %d (weapons %s)",
                  job, lv, st.jobLevel, st.atk, sm::atk(t, 0), sm::atk(t, 11), st.matkMin, st.matkMax,
                  sm::matkMin(t[sm::Int]), sm::matkMax(t[sm::Int]), st.hit, sm::hit(lv, t[sm::Dex]), st.flee,
                  sm::flee(lv, t[sm::Agi]), st.crit, sm::crit10(t[sm::Luk]) / 10, sm::crit10(t[sm::Luk]) % 10, st.mdef2,
                  sm::softMdef(t[sm::Int], t[sm::Vit]), st.hpMost, sm::maxHp(job, lv, t[sm::Vit]), st.spMost,
                  sm::maxSp(job, lv, t[sm::Int]), st.weightMost, sm::weightMost(job, st.statBase[sm::Str]), st.aspd,
                  aspdWeapons.empty() ? "none" : aspdWeapons.c_str());
    if (line == told) return;
    told = line;
    ++times;
    LOG_INFO("stats: the server's against the formulas - %s", line);
}

void readData(State& s) {
    charstatus::Status st;
    if (!readOwnStatus(st)) return;
    sw::Data& d = s.data;
    for (int i = 0; i < sw::kStats; ++i) {
        d.base[i] = st.statBase[i];
        d.bonus[i] = st.statBonus[i];
        d.need[i] = st.statNeed[i];
    }
    d.free = st.statusPoints;
    d.atk = st.atk;
    d.atk2 = st.atk2;
    d.matkMin = st.matkMin;
    d.matkMax = st.matkMax;
    d.hit = st.hit;
    d.crit = st.crit;
    d.def = st.def;
    d.def2 = st.def2;
    d.mdef = st.mdef;
    d.mdef2 = st.mdef2;
    d.flee = st.flee;
    d.flee2 = st.flee2;
    d.aspd = st.aspd;
    d.job = int(read32(kJobAt));  // 3.0.11-beta.266: the panel «Як рахується»'s
    d.baseLevel = st.baseLevel;
    d.jobLevel = st.jobLevel;
    d.hpMost = st.hpMost;
    d.spMost = st.spMost;
    d.weight = st.weight;
    d.weightMost = st.weightMost;
    readMapMobs(d);  // 3.0.11-beta.296
    readLearned(d);  // 3.0.11-beta.297, .298: the buffs the plan can count
    for (int i = 0; i < sw::kPlanBuffs; ++i) d.buffNow[i] = buffoverlay::timers().on(sw::kPlanBuffSpecs[i].icon);
    serverCheck(st);  // 3.0.11-beta.264
}

sw::Texts texts() {
    sw::Texts t;
    t.title = say(settings::Text::StatsTitle);
    t.hotkey = "Alt+A";
    t.free = say(settings::Text::StatsFree);
    const settings::Text cols[5] = {settings::Text::StatsColumnStat, settings::Text::StatsColumnBase,
                                    settings::Text::StatsColumnBonus, settings::Text::StatsColumnTotal,
                                    settings::Text::StatsColumnCost};
    for (int i = 0; i < 5; ++i) t.columns[i] = say(cols[i]);
    t.battle = say(settings::Text::StatsBattle);
    t.cast = say(settings::Text::StatsCast);
    t.maxed = say(settings::Text::StatsMaxed);
    t.cost = say(settings::Text::StatsCost);
    t.shortBy = say(settings::Text::StatsShort);
    t.top = say(settings::Text::StatsTop);
    const settings::Text effects[sw::kStats] = {settings::Text::StatsEffectStr, settings::Text::StatsEffectAgi,
                                                settings::Text::StatsEffectVit, settings::Text::StatsEffectInt,
                                                settings::Text::StatsEffectDex, settings::Text::StatsEffectLuk};
    for (int i = 0; i < sw::kStats; ++i) t.effects[i] = say(effects[i]);
    t.hint = say(settings::Text::StatsHint);
    // 3.0.11-beta.266: stage 2's rows and the panel «Як рахується»
    t.delay = say(settings::Text::StatsRowDelay);
    t.regen = say(settings::Text::StatsRowRegen);
    t.weight = say(settings::Text::StatsRowWeight);
    t.perSecond = say(settings::Text::StatsPerSecond);
    t.how = say(settings::Text::StatsHow);
    t.howNone = say(settings::Text::StatsHowNone);
    t.howSame = say(settings::Text::StatsHowSame);
    t.howMore = say(settings::Text::StatsHowMore);
    t.howWeapon = say(settings::Text::StatsHowWeapon);
    t.howLevel = say(settings::Text::StatsHowLevel);
    t.howAspdWeapon = say(settings::Text::StatsHowAspdWeapon);
    t.howAspdUnknown = say(settings::Text::StatsHowAspdUnknown);
    t.howHits = say(settings::Text::StatsHowHits);
    t.howAspdGoal = say(settings::Text::StatsHowAspdGoal);
    t.howAspdNever = say(settings::Text::StatsHowAspdNever);
    t.howDef = say(settings::Text::StatsHowDef);
    t.howMdef = say(settings::Text::StatsHowMdef);
    t.howPerfect = say(settings::Text::StatsHowPerfect);
    t.howCast = say(settings::Text::StatsHowCast);
    t.howCastNone = say(settings::Text::StatsHowCastNone);
    t.howDelay = say(settings::Text::StatsHowDelay);
    t.howRegen = say(settings::Text::StatsHowRegen);
    t.howWeight = say(settings::Text::StatsHowWeight);
    t.howJob = say(settings::Text::StatsHowJob);
    t.howNext = say(settings::Text::StatsHowNext);
    const settings::Text uses[sw::kStats] = {settings::Text::StatsUsesStr, settings::Text::StatsUsesAgi,
                                             settings::Text::StatsUsesVit, settings::Text::StatsUsesInt,
                                             settings::Text::StatsUsesDex, settings::Text::StatsUsesLuk};
    for (int i = 0; i < sw::kStats; ++i) t.uses[i] = say(uses[i]);
    t.tabs[0] = say(settings::Text::StatsTabChar);  // 3.0.11-beta.267: stage 3
    t.tabs[1] = say(settings::Text::StatsTabPlan);  // .268
    t.tabs[2] = say(settings::Text::StatsTabRef);
    t.planLevel = say(settings::Text::StatsPlanLevel);
    t.planPoints = say(settings::Text::StatsPlanPoints);
    t.planShort = say(settings::Text::StatsPlanShort);
    t.planResult = say(settings::Text::StatsPlanResult);
    t.planReset = say(settings::Text::StatsPlanReset);
    t.planNote = say(settings::Text::StatsPlanNote);
    t.goals = say(settings::Text::StatsGoals);  // .269
    t.goalAspd = say(settings::Text::StatsGoalAspd);
    t.goalAspdNever = say(settings::Text::StatsGoalAspdNever);
    t.goalAspdNone = say(settings::Text::StatsGoalAspdNone);
    t.goalCast = say(settings::Text::StatsGoalCast);
    t.goalCastDone = say(settings::Text::StatsGoalCastDone);
    t.goalCastNever = say(settings::Text::StatsGoalCastNever);
    t.mobs = say(settings::Text::StatsMobs);  // 3.0.11-beta.296
    t.mobsHit = say(settings::Text::StatsMobsHit);
    t.mobsDodge = say(settings::Text::StatsMobsDodge);
    t.mobsHits = say(settings::Text::StatsMobsHits);
    t.mobsNone = say(settings::Text::StatsMobsNone);
    t.planBuffs = say(settings::Text::StatsPlanBuffs);  // 3.0.11-beta.297
    t.planBuffNow = say(settings::Text::StatsPlanBuffNow);
    t.refClass = say(settings::Text::StatsRefClass);
    t.refAspd = say(settings::Text::StatsRefAspd);
    t.refWeapon = say(settings::Text::StatsRefWeapon);
    t.refDelay = say(settings::Text::StatsRefDelay);
    t.refYourAgi = say(settings::Text::StatsRefYourAgi);
    t.refCost = say(settings::Text::StatsRefCost);
    t.refCostFrom = say(settings::Text::StatsRefCostFrom);
    t.refCostPoints = say(settings::Text::StatsRefCostPoints);
    t.refBonus = say(settings::Text::StatsRefBonus);
    t.refBonusNone = say(settings::Text::StatsRefBonusNone);
    t.refBonusNow = say(settings::Text::StatsRefBonusNow);
    t.decimal = language() == settings::Lang::En ? '.' : ',';
    return t;
}

float measure(const std::string& utf8Text, float px, bool bold) {
    return float(wordsWidth(utf8Wide(utf8Text), px, bold));
}

void render(State& s) {
    ++s.sheetGen;
}

void block(const State& s, HWND game) {  // the mouse's clicks over it are its own, not the world's
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Stats, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Stats, nullptr, nullptr, false);
    s.shown = false;
    s.moving = false;
}

bool raise(int stat) {
    const uint32_t mode = read32(kModeNow);
    if (!mode || !inGameWorld() || read32(read32(mode) + kModeMsgSlot) != kModeMsg) return false;
    reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, kRaiseMsg, kFirstStat + stat, 1, 0);
    return true;
}

void closeGameWindow() {
    const uint32_t window = gameWindow();
    if (window && read32(read32(window) + kSendMsgSlot) == kStatsSendMsg)
        reinterpret_cast<SendMsgFn>(uintptr_t(kStatsSendMsg))(window, 0, kButtonMsg, kCloseButton, 0, 0);
}

}  // namespace

static void frameInner(HWND game, bool on, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("stats: [interface] status = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameWindow() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    readState(s);
    // 3.0.18-beta.151: times its own size (its corner pulled)
    const float scale = uistyle::readingScale(k) * windowstack::sizeOf(winstack::Win::Stats);
    s.k = float(k);
    s.scale = scale;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    const ULONGLONG now = GetTickCount64();
    if (now - s.readAt >= kReadMs) {
        s.readAt = now;
        readData(s);
    }
    const sw::Texts t = texts();
    s.layout = sw::layout(0, 0, scale, s.data, t, measure, s.tab, s.refJob);
    if (s.live.shown()) uikit::stats::refit(s.live.doc.doc(), s.layout);  // 3.0.18-beta.101: its drawn boxes
    s.layout.x = int(std::lround(s.fx * float(std::max(0, targetWidth - s.layout.w))));
    s.layout.y = int(std::lround(s.fy * float(std::max(0, targetHeight - s.layout.h))));
    s.show.hoverRow = s.show.hoverPlus = s.show.hoverBattle = s.show.hoverTab = s.show.hoverArrow = -1;
    s.show.hoverPlan = s.show.hoverPlanIndex = -1;
    // 3.0.11-beta.296: the character's kept plan, once its name is known
    readPlans(s);
    if (const std::string own = readOwnName(); !own.empty() && own != s.planOwner && s.data.baseLevel > 0) {
        s.planOwner = own;
        const auto kept = s.plans.find(own);
        s.show.plan = kept != s.plans.end() ? kept->second.plan : sw::Plan{};
        s.show.aspdGoal = kept != s.plans.end() ? kept->second.aspdGoal : 0;
        s.planFor = -1;
        if (kept != s.plans.end() && s.told++ < 12)
            LOG_INFO("stats: the kept plan taken (level %d)", s.show.plan.level);
    }
    // 3.0.11-beta.268: the plan starts from the character as it is, and again after a level or a point spent
    if (s.show.plan.level < s.data.baseLevel || s.planFor != s.data.baseLevel * 1000 + s.data.free) {
        const bool fresh = s.show.plan.level == 0 || s.show.plan.level < s.data.baseLevel;
        sw::Plan p = sw::planFrom(s.data);
        if (!fresh) {  // the planned stats kept where they are still above the character's
            p.level = std::max(p.level, s.show.plan.level);
            for (int i = 0; i < sw::kStats; ++i) p.base[i] = std::max(p.base[i], s.show.plan.base[i]);
        }
        s.show.plan = p;
        s.planFor = s.data.baseLevel * 1000 + s.data.free;
    }
    keepPlans(s);  // .296: written when it changed
    s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy) && !s.moving) {
        const sw::Hit h = sw::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == sw::Part::Row) s.show.hoverRow = h.index;
        if (h.part == sw::Part::Plus) s.show.hoverPlus = h.index;
        if (h.part == sw::Part::Battle) s.show.hoverBattle = h.index;  // 3.0.11-beta.266
        if (h.part == sw::Part::Tab) s.show.hoverTab = h.index;        // 3.0.11-beta.267
        if (h.part == sw::Part::Arrow) s.show.hoverArrow = h.index;
        if (h.part == sw::Part::PlanMinus || h.part == sw::Part::PlanPlus || h.part == sw::Part::PlanLevel ||
            h.part == sw::Part::PlanReset || h.part == sw::Part::Goal ||
            h.part == sw::Part::PlanBuff) {  // .268, .269, .297
            s.show.hoverPlan = int(h.part);
            s.show.hoverPlanIndex = h.index;
        }
        s.show.hoverClose = h.part == sw::Part::Close;
    }
    if (uapad::runtime::look().shown && s.tab == 0) {  // 3.0.11-beta.92: the controller's stat - its «+» lit
        if (s.show.selected < 0 || s.show.selected > 5) s.show.selected = 0;
        s.show.hoverPlus = s.show.selected;
    }
    if (uapad::runtime::look().shown && s.tab == sw::kPlanTab) {  // .296: the controller's row - its › or + lit
        s.padRow = std::clamp(s.padRow, 0, kPadRows - 1);
        s.show.hoverPlan = int(s.padRow == 0                ? sw::Part::PlanLevel
                               : s.padRow <= sw::kStats     ? sw::Part::PlanPlus
                               : s.padRow == sw::kStats + 1 ? sw::Part::Goal
                                                            : sw::Part::PlanBuff);  // .297: the buffs' chips
        s.show.hoverPlanIndex = s.padRow >= 1 && s.padRow <= sw::kStats ? s.padRow - 1
                                : s.padRow > sw::kStats + 1             ? s.padRow - sw::kStats - 2
                                                                        : 1;
    }
    for (int i = 0; i < sw::kPlanBuffs; ++i) s.show.buffOn[i] = sw::planBuff(s.data, s.show.plan, i);  // .297
    const sw::Data& d = s.data;
    std::string key = std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ';' + std::to_string(scale) +
                      ';' + std::to_string(s.show.hoverRow) + ',' + std::to_string(s.show.hoverPlus) + ',' +
                      std::to_string(s.show.selected) + (s.show.hoverClose ? "x" : "") + ';' + std::to_string(d.free) +
                      ';' + std::to_string(s.show.hoverBattle) + ',' + std::to_string(s.show.pinnedBattle) + ';' +
                      std::to_string(d.job) + ',' + std::to_string(d.baseLevel) + ',' + std::to_string(d.jobLevel) +
                      ',' + std::to_string(d.hpMost) + ',' + std::to_string(d.spMost) + ',' + std::to_string(d.weight) +
                      ',' + std::to_string(d.weightMost) + ';' + std::to_string(s.tab) + ',' +
                      std::to_string(s.layout.refJob) + ',' + std::to_string(s.show.hoverTab) + ',' +
                      std::to_string(s.show.hoverArrow) + ';' + std::to_string(s.show.hoverPlan) + ',' +
                      std::to_string(s.show.hoverPlanIndex) + ';' + std::to_string(s.show.plan.level) + ':' +
                      std::to_string(s.show.aspdGoal) + ':';
    for (int i = 0; i < sw::kStats; ++i) key += std::to_string(s.show.plan.base[i]) + ',';
    key += ';';  // .297, .298
    for (int i = 0; i < sw::kPlanBuffs; ++i) key += std::to_string(d.learned[i]);
    for (int i = 0; i < sw::kPlanBuffs; ++i)
        key += std::to_string(s.show.plan.buffs[i]) + (d.buffNow[i] ? "n" : "") + (s.show.buffOn[i] ? "o," : ",");
    for (int i = 0; i < sw::kStats; ++i)
        key += std::to_string(d.base[i]) + '+' + std::to_string(d.bonus[i]) + '/' + std::to_string(d.need[i]) + ',';
    const int battle[] = {d.atk, d.atk2, d.matkMin, d.matkMax, d.hit, d.crit, d.def, d.def2, d.mdef, d.mdef2, d.flee,
                          d.flee2, d.aspd};
    for (int v : battle) key += std::to_string(v) + ',';
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()) + (uaui::ready() ? "u" : "");
        drawn != s.key) {  // 3.0.18-beta.29: uaUI's or the picture  // its grounds
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

void frame(HWND game, bool on, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, k, targetWidth, targetHeight);
    State& s = state();
    namespace sl = uikit::stats;
    s.sheetShown = s.live.frame("stats", s.shown, s.layout.x, s.layout.y, s.sheetGen, sl::rml(), sl::bind,
                                [&](sl::Model& m) {
                                    m.ground = s.live.ground();
                                    sl::fill(m, s.layout, s.data, s.show, texts(), measure);
                                });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    s.live.raise();  // 3.0.18-beta.43: at its place in the windows' stack
    if (!s.shown || s.k <= 0 || ((s.picture.width <= 0 || s.picture.height <= 0) && !s.sheetShown)) return;
    int cursor = -1, cx = 0, cy = 0;  // under the game's own cursor, as the minimap
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;  // 3.0.11-beta.49: its shadow under it
    // 3.0.18-beta.43: the shadow the driver's, under uaUI's window too
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h),
                  12 * s.scale, s.scale, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool shown() { return state().shown; }

// .296: a plan's row moved by the controller's left (-1) or right (+1): the level, a stat, the ASPD aimed at
void padPlanStep(State& s, int by) {
    if (s.padRow > sw::kStats + 1) {  // .297: a buff's chip - on or off
        const int b = s.padRow - sw::kStats - 2;
        s.show.plan.buffs[b] = sw::planBuff(s.data, s.show.plan, b) ? 0 : 1;
        return;
    }
    const int most = uagfx::statmath::baby(s.data.job) ? uagfx::statmath::kMaxBabyStat : uagfx::statmath::kMaxStat;
    if (s.padRow == 0) {
        s.show.plan.level = std::clamp(s.show.plan.level + by, s.data.baseLevel, 99);
    } else if (s.padRow <= sw::kStats) {
        int& v = s.show.plan.base[s.padRow - 1];
        v = std::clamp(v + by, s.data.base[s.padRow - 1], most);
    } else {
        const int now = s.show.aspdGoal > 0 ? s.show.aspdGoal : std::min(190, s.data.aspd + 5);
        s.show.aspdGoal = std::clamp(now + by, std::max(1, s.data.aspd), 190);
    }
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    if (s.show.selected < 0 || s.show.selected > 5) s.show.selected = 0;
    switch (c) {  // 3.0.11-beta.296: the tabs, «Планування»'s rows and «Довідка»'s classes too
        case PadCommand::TabBack:
        case PadCommand::TabOn:
            s.tab = (s.tab + (c == PadCommand::TabOn ? 1 : sw::kTabs - 1)) % sw::kTabs;
            return;
        case PadCommand::Close: closeGameWindow(); return;
        default: break;
    }
    if (s.tab == sw::kPlanTab) {
        const auto offered = [&](int row) {  // .297: a buff's row only when its chip is there
            return row <= sw::kStats + 1 || sw::planBuffOffered(s.data, row - sw::kStats - 2);
        };
        if (c == PadCommand::Up || c == PadCommand::Down) {
            const int by = c == PadCommand::Up ? kPadRows - 1 : 1;
            do s.padRow = (s.padRow + by) % kPadRows;
            while (!offered(s.padRow));
        }
        if (c == PadCommand::Left || c == PadCommand::Right) padPlanStep(s, c == PadCommand::Right ? 1 : -1);
        if (c == PadCommand::Reset) s.show.plan = sw::planFrom(s.data);
        return;
    }
    if (s.tab == sw::kRefTab) {
        if (c != PadCommand::Left && c != PadCommand::Right) return;
        const std::vector<int> jobs = uagfx::statmath::jobs();
        auto at = std::find(jobs.begin(), jobs.end(), s.layout.refJob);
        int i = at == jobs.end() ? 0 : int(at - jobs.begin());
        i = (i + (c == PadCommand::Right ? 1 : int(jobs.size()) - 1)) % std::max<int>(1, int(jobs.size()));
        s.refJob = jobs.empty() ? -1 : jobs[size_t(i)];
        return;
    }
    switch (c) {
        case PadCommand::Up: s.show.selected = (s.show.selected + 5) % 6; break;
        case PadCommand::Down: s.show.selected = (s.show.selected + 1) % 6; break;
        case PadCommand::Raise: {
            const bool sent = raise(s.show.selected);
            LOG_INFO("stats: the controller's A - a point to the stat %d%s", s.show.selected, sent ? "" : " not sent");
            break;
        }
        default: break;
    }
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    (void)wp;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL && m != WM_MOUSEMOVE) return false;
    float tx = 0, ty = 0;
    bool fromGame = false;  // 3.0.11-beta.43: where the game's cursor stands, as the hover
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, &fromGame)) return false;
    if (s.moving && (m == WM_MOUSEMOVE || m == WM_LBUTTONUP)) {  // its title dragged
        const int roomX = std::max(1, s.targetW - s.layout.w), roomY = std::max(1, s.targetH - s.layout.h);
        s.fx = std::clamp((tx - s.grabX) / float(roomX), 0.0f, 1.0f);
        s.fy = std::clamp((ty - s.grabY) / float(roomY), 0.0f, 1.0f);
        if (m == WM_LBUTTONUP) {
            s.moving = false;
            keepState(s);
        }
        result = 0;
        return true;
    }
    const sw::Hit h = sw::hitTest(s.layout, tx - float(s.layout.x), ty - float(s.layout.y));
    if ((m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) && s.pressesTold < 8) {  // where the presses land
        ++s.pressesTold;
        int cx = -1, cy = -1;
        readGameCursor(cx, cy);
        LOG_INFO("stats: a press at the window's (%d, %d), the game's cursor (%d, %d) - the target's (%.0f, %.0f) by "
                 "the %s; «Характеристики» at %d, %d %dx%d: part %d", GET_X_LPARAM(lp), GET_Y_LPARAM(lp), cx, cy, tx,
                 ty, fromGame ? "game's cursor" : "message's point", s.layout.x, s.layout.y, s.layout.w, s.layout.h,
                 int(h.part));
    }
    if (h.part == sw::Part::None || m == WM_MOUSEMOVE) return false;
    result = 0;
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    switch (h.part) {
        case sw::Part::Close:
            if (s.told++ < 12) LOG_INFO("stats: its close - the game's own window asked to close (its 0x87)");
            closeGameWindow();
            break;
        case sw::Part::Plus:
            s.show.selected = h.index;
            if (sw::canRaise(s.data, h.index)) {
                const bool sent = raise(h.index);
                s.readAt = 0;
                if (s.told++ < 12)
                    LOG_INFO("stats: %s +1 %s", sw::kNames[h.index],
                             sent ? "asked as the game's own «+»" : "not asked");
            }
            break;
        case sw::Part::Row:
            s.show.selected = h.index;
            s.show.pinnedBattle = -1;
            break;
        case sw::Part::Tab:  // 3.0.11-beta.267: «Персонаж» or «Довідка»
            s.tab = h.index;
            if (s.told++ < 12) LOG_INFO("stats: the tab %d", h.index);
            break;
        case sw::Part::PlanMinus:  // .268: a planned stat down to the character's own, up to 99 (a baby's 80)
        case sw::Part::PlanPlus: {
            const int most = uagfx::statmath::baby(s.data.job) ? uagfx::statmath::kMaxBabyStat
                                                                : uagfx::statmath::kMaxStat;
            const int step = GetKeyState(VK_SHIFT) < 0 ? 10 : 1;
            int& v = s.show.plan.base[h.index];
            v = std::clamp(v + (h.part == sw::Part::PlanPlus ? step : -step), s.data.base[h.index], most);
            break;
        }
        case sw::Part::PlanLevel: {  // the plan's level: the character's to 99
            const int step = GetKeyState(VK_SHIFT) < 0 ? 10 : 1;
            s.show.plan.level = std::clamp(s.show.plan.level + (h.index ? step : -step), s.data.baseLevel, 99);
            break;
        }
        case sw::Part::PlanReset:
            s.show.plan = sw::planFrom(s.data);
            break;
        case sw::Part::PlanBuff:  // .297: a buff counted in the plan or not
            s.show.plan.buffs[h.index] = sw::planBuff(s.data, s.show.plan, h.index) ? 0 : 1;
            break;
        case sw::Part::Goal: {  // .269: the ASPD aimed at, a whole one up to 190
            const int now = s.show.aspdGoal > 0 ? s.show.aspdGoal : std::min(190, s.data.aspd + 5);
            s.show.aspdGoal = std::clamp(now + (h.index ? 1 : -1), std::max(1, s.data.aspd), 190);
            break;
        }
        case sw::Part::Arrow: {  // .267: «Довідка»'s class, the previous or the next of the server's
            const std::vector<int> jobs = uagfx::statmath::jobs();
            const int now = s.layout.refJob;
            auto at = std::find(jobs.begin(), jobs.end(), now);
            int i = at == jobs.end() ? 0 : int(at - jobs.begin());
            i = (i + (h.index ? 1 : int(jobs.size()) - 1)) % std::max<int>(1, int(jobs.size()));
            s.refJob = jobs.empty() ? -1 : jobs[size_t(i)];
            break;
        }
        case sw::Part::Battle:  // 3.0.11-beta.266: a battle row's formulas kept in the panel (again: let go)
            s.show.pinnedBattle = s.show.pinnedBattle == h.index ? -1 : h.index;
            break;
        case sw::Part::Title:
            s.moving = true;
            s.grabX = tx - float(s.layout.x);
            s.grabY = ty - float(s.layout.y);
            break;
        default:
            break;
    }
    s.key.clear();
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (!s.on || !inGameWorld()) return;
    if (const uint32_t window = gameWindow()) hold(s, window, true);
}


bool box(int& x, int& y, int& w, int& h) {  // 3.0.11-beta.109
    const State& s = state();
    if (!s.shown) return false;
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

void moveTo(int x, int y) {  // 3.0.11-beta.109: question 4 - beside the window standing in its own place
    State& s = state();
    if (!s.shown) return;
    const int roomX = std::max(1, s.targetW - s.layout.w), roomY = std::max(1, s.targetH - s.layout.h);
    s.fx = std::clamp(float(x) / float(roomX), 0.0f, 1.0f);
    s.fy = std::clamp(float(y) / float(roomY), 0.0f, 1.0f);
    s.layout.x = x;
    s.layout.y = y;
    keepState(s);
}

}  // namespace uagfx::win::statswin
