#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::statwin {

// 3.0.11-beta.42 (stage 8 of the new interface, its windows; the owner: «делай окна»): «Характеристики» (Alt+A)
// redrawn in style A (the concept's v17_windows): the six stats' table - base, bonus, their sum and the next point's
// price, a «+» where the free points pay for it - beside the battle's numbers, and a line under them telling what the
// stat pointed at gives by the game's (pre-renewal) formulas. The numbers are the game's session's, as its own status
// window shows them (FUN_00471d80); a «+» asks as its own does (the game mode's message 0x20). The parts that need no
// Windows; the words by GDI (win/stat_window).

constexpr int kStats = 6;
extern const char* const kNames[kStats];  // STR AGI VIT INT DEX LUK

// 3.0.11-beta.296 (the concept «Характеристики», the rest): a monster of the map the character is on (the server's
// spawns from the site's pack), for the plan's comparison
struct MapMob {
    int id = 0, level = 0, hp = 0;
    std::string name;
};
struct Data {
    int base[kStats] = {}, bonus[kStats] = {}, need[kStats] = {};
    int free = 0;  // the status points
    int atk = 0, atk2 = 0, matkMin = 0, matkMax = 0, hit = 0, crit = 0;
    int def = 0, def2 = 0, mdef = 0, mdef2 = 0, flee = 0, flee2 = 0, aspd = 0;
    // 3.0.11-beta.266 (the concept «Характеристики», stage 2): the class (-1 not known), the levels, the server's most
    // HP and SP, the weight now and its most (whole units)
    int job = -1, baseLevel = 0, jobLevel = 0, hpMost = 0, spMost = 0, weight = 0, weightMost = 0;
    // 3.0.11-beta.297 (the owner: «в планировании без учетов скилов типа impove consentration»), .298 («делай для всех
    // классов»): each plan buff's skill as learned (its level, 0 none) and whether it is on the character now
    int learned[17] = {};
    bool buffNow[17] = {};
    std::string map;                // .296: the map's name, its monsters (the most spawned first, no bosses)
    std::vector<MapMob> mapMobs;
};
// As the game's window shows its «+» (FUN_004722a0, message 0x23): a price, the points for it, under 99
bool canRaise(const Data& d, int stat);

struct Texts {
    std::string title, hotkey;    // «Характеристики», «Alt+A»
    std::string free;             // «Вільні очки: {1}»
    std::string columns[5];       // «Стат», «База», «Бонус», «Разом», «Ціна»
    std::string battle, cast;     // «Бій», «Час касту»
    std::string maxed;            // «макс.»
    std::string cost, shortBy, top;  // «{1} {2} → {3} коштує {4}, у вас {5}», «бракує {1}», «{1} {2} — максимум»
    std::string effects[kStats];  // what a point gives: «ATK +{1}, вантаж +30» ...
    std::string hint;             // nothing pointed at
    char decimal = ',';
    // 3.0.11-beta.266 (stage 2): the new rows, «{1}/с» after ASPD, the panel «Як рахується»'s words
    std::string delay, regen, weight, perSecond;
    std::string how, howNone, howSame, howMore, howWeapon, howLevel, howAspdWeapon, howAspdUnknown, howHits;
    std::string howAspdGoal, howAspdNever, howDef, howMdef, howPerfect, howCast, howCastNone, howDelay, howRegen;
    std::string howWeight, howJob, howNext;
    std::string uses[kStats];  // what each stat goes into
    // 3.0.11-beta.267 (stage 3): the tabs «Персонаж», «Планування» (.268), «Довідка»; the reference's words
    std::string tabs[3], refClass, refAspd, refWeapon, refDelay, refYourAgi, refCost, refCostFrom, refCostPoints;
    std::string refBonus, refBonusNone, refBonusNow;
    // 3.0.11-beta.268 (stage 4): «Планування»'s words
    std::string planLevel, planPoints, planShort, planResult, planReset, planNote;
    // 3.0.11-beta.269 (stage 5): the goals
    std::string goals, goalAspd, goalAspdNever, goalAspdNone, goalCast, goalCastDone, goalCastNever;
    // 3.0.11-beta.296: «Монстри карти {1}», its columns, nothing known
    std::string mobs, mobsHit, mobsDodge, mobsHits, mobsNone;
    std::string planBuffs, planBuffNow;  // .297: «Бафи в плані», «зараз»
};

// 3.0.11-beta.268 (stage 4, the owner: «только показать»): a plan of the stats - each stat's base planned, at a base
// level planned; nothing of it goes to the server
// 3.0.11-beta.297: the buffs a plan counts, each as on the character now (-1), off (0) or on (1); .298 every class's
// (pre-renewal eAthena's status.c, uAthena's own): Improve Concentration AGI and DEX +(2 + level) % of the base and
// the job's bonus; flat stats, HIT, FLEE, CRIT (tenths), ATK; Angelus VIT DEF +50 %; the attack's delay per mille
// less - the skills' best one (only with their weapons), the potions' best one on top of it
struct PlanBuffSpec {
    const char* name;
    uint16_t icon, skill;  // the server's status icon; the skill that gives it (0: others' or a potion - offered all)
    int stats[kStats];
    int hit, flee, crit10, atk, defPercent, aspd;
    bool potion;
    uint32_t weapons;  // the weapon types (bits of job_db1's columns) it works with; 0 any
};
constexpr int kPlanBuffs = 17, kBuffConcentration = 0, kBuffBlessing = 1, kBuffAgility = 2, kBuffAdrenaline = 13;
extern const PlanBuffSpec kPlanBuffSpecs[kPlanBuffs];
static_assert(sizeof(Data::learned) / sizeof(int) == kPlanBuffs && sizeof(Data::buffNow) == kPlanBuffs);
struct Plan {
    int base[kStats] = {};
    int level = 0;
    int buffs[kPlanBuffs];
    Plan() {
        for (int& b : buffs) b = -1;
    }
};
Plan planFrom(const Data& d);  // the character as it is
bool planBuff(const Data& d, const Plan& p, int buff);  // .297: the buff counted in the plan
bool planBuffOffered(const Data& d, int buff);  // a class skill's only when learned or on; others' to everyone
// .298: the attack's delay in per mille (1000 none) with the buffs counted - now (on the character) or the plan's
int aspdRate(const Data& d, int weapon, bool planned, const Plan* p);
// .297: the plan's six stats in all - its base, the things' and the passive skills' part kept, its buffs counted
void planTotals(const Data& d, const Plan& p, int out[kStats]);
int planAvailable(const Data& d, const Plan& p);  // the free points now and the levels' to come
int planUsed(const Data& d, const Plan& p);       // the points the planned stats take
struct PlanRow {
    std::string name, now, then;
    bool changed = false;
};
// The numbers now and with the plan: the server's now, the plan's by the formulas' difference (the things' and the
// buffs' part kept)
std::vector<PlanRow> planRows(const Data& d, const Plan& p, const Texts& t);
// 3.0.11-beta.269 (stage 5, the concept's «Цели»): what the plan still needs for ASPD `aspdGoal` (with the plan's DEX)
// and for no cast at all (DEX 150 with the bonuses) - the stat it takes and its points past the plan's
std::vector<std::string> planGoals(const Data& d, const Plan& p, int aspdGoal, const Texts& t);
// 3.0.11-beta.296 (the concept's monster comparison and HIT/FLEE by the map's monsters): each monster of the map now
// and with the plan by the server's rules (core/mob_battle): the character's chance to hit it, to dodge it, the blows
// it takes (0 not known); the plan's numbers by the formulas' difference, as planRows
struct MobRow {
    std::string name;
    int level = 0;
    int hitNow = 0, hitThen = 0, dodgeNow = 0, dodgeThen = 0, hitsNow = 0, hitsThen = 0;
};
constexpr int kMobRows = 5;
std::vector<MobRow> planMobs(const Data& d, const Plan& p);
// .296: the plans kept across the game's starts (uaro-graphics\stat-plans.txt), by the character's name: one line a
// character - its name in hex, the level, the six stats, the ASPD aimed at
struct SavedPlan {
    Plan plan;
    int aspdGoal = 0;
};
std::string plansText(const std::map<std::string, SavedPlan>& plans);
std::map<std::string, SavedPlan> parsePlans(const std::string& text);

struct Row {
    std::string name, value;
};
// The battle's numbers: ATK «28 + 17», MATK «334 ~ 550», HIT, FLEE «132 + 1», CRIT, DEF «6 + 32», MDEF, ASPD and the
// cast's time «−56 %» (by DEX: a 150th a point)
std::vector<Row> battleRows(const Data& d, const Texts& t);
// The line about a stat: its next point's price against the free points and what it gives; the hint for -1
std::string explain(const Data& d, int stat, const Texts& t);
// 3.0.11-beta.266 (stage 2, the panel «Як рахується»): what is explained - a stat (0-5) or a battle row (kBattle + its
// index in battleRows), -1 nothing; its title and its lines - the server's formula on the character's own numbers
constexpr int kBattle = 100, kExplainLines = 4;
struct Explained {
    std::string title;
    std::vector<std::string> lines;  // kExplainLines at most
};
Explained explained(const Data& d, int item, const Texts& t);
// The weapon type (job_db1's column) the server's ASPD comes from with these stats; -1 none fits (skills or things)
int weaponOf(const Data& d);

using Measure = std::function<float(const std::string& utf8, float px, bool bold)>;

struct Show {
    int hoverRow = -1, hoverPlus = -1, selected = -1;
    int hoverBattle = -1, pinnedBattle = -1;  // 3.0.11-beta.266: a battle row under the mouse, the one clicked
    int hoverTab = -1, hoverArrow = -1;       // 3.0.11-beta.267: a tab, the class's ‹ (0) or › (1)
    int hoverPlan = -1, hoverPlanIndex = -1;  // 3.0.11-beta.268: a plan's part (its Part) and its index
    Plan plan;                                // .268: «Планування»'s (the window's, kept while the game runs)
    int aspdGoal = 0;                         // .269: the ASPD aimed at (0: the plan's + 5)
    bool buffOn[kPlanBuffs] = {};             // .297: each plan buff counted (paint has no Data: set with the plan)
    bool hoverClose = false;
};

struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, close, freeChip, table, header, battle, line;  // .266: line - the panel «Як рахується»
    std::vector<panel::Box> rows, pluses;  // the six stats' rows and «+»
    std::vector<panel::Box> battleRows;    // 3.0.11-beta.266
    // 3.0.11-beta.267 (stage 3): the tab shown (0 «Персонаж», 1 «Довідка») and the class the reference tells of;
    // the tabs, the class's arrows and the reference's parts
    int tab = 0, refJob = -1;
    panel::Box tabs[3], prev, next, classRow, aspd, cost, bonus;
    // 3.0.11-beta.268 (stage 4): «Планування» - the level's row and its ‹ ›, the stats' rows with − and +, the
    // results, «Скинути»
    panel::Box planLevelRow, levelPrev, levelNext, planResults, planReset;
    panel::Box goals, goalPrev, goalNext;  // .269
    panel::Box mobs;                       // .296: «Монстри карти»
    panel::Box buffsRow, buffChips[kPlanBuffs];  // .297: the buffs the plan counts
    std::vector<panel::Box> planRowsBoxes, planMinus, planPlus;
    int wide = 0;  // 3.0.19-beta.1: its own width at its size - the live window's (uikit stats' refit leaves it)
};
// The weapons the class can use: its delay, the ASPD at AGI 1, 50, 80, 99 and at the AGI given, with the DEX given
struct AspdRow {
    int weapon = 0, delay = 0;
    int at[4] = {}, mine = 0;  // whole ASPD
};
constexpr int kAgiSteps[4] = {1, 50, 80, 99}, kCostSteps[5] = {50, 70, 80, 90, 99}, kMostAspdRows = 12;
std::vector<AspdRow> aspdTable(int job, int agi, int dex);
const char* weaponName(int weapon);
constexpr int kTabs = 3, kPlanTab = 1, kRefTab = 2;  // .268: «Персонаж», «Планування», «Довідка»
// tab kRefTab «Довідка»: refJob - the class told of (-1: the character's)
Layout layout(int x, int y, float scale, const Data& d, const Texts& t, const Measure& m, int tab = 0,
              int refJob = -1);

// .266 Battle, .267 Tab and Arrow, .268 the plan's
enum class Part : uint8_t { None, Close, Plus, Row, Title, Body, Battle, Tab, Arrow, PlanMinus, PlanPlus, PlanLevel,
                            PlanReset, Goal,  // .269: Goal - the ASPD aimed at, ‹ (0) or › (1)
                            PlanBuff };       // .297: a buff of the plan, its index
struct Hit {
    Part part = Part::None;
    int index = -1;
};
Hit hitTest(const Layout& L, float x, float y);
int explainedItem(const Show& sh);  // .266: the stat or battle row the panel tells of


enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Data& d, const Show& sh, const Texts& t, const Measure& m);

}  // namespace uagfx::statwin
