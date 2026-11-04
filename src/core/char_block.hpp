#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/ui_icons.hpp"
#include "core/window_panel.hpp"

namespace uagfx::charblock {

// 3.0.11-beta.36 (stage 7 of the new interface, the HUD; the owner: «кольца или полоски на выбор. приступай»): the
// character's block at the screen's top left in place of the game's Basic Info window, style A (the concept's
// v17_char) - full (the portrait, the name and class, the levels, the weight, the zeny, the free points, HP and SP
// bars with their numbers, the experience with a tick each 10 %, the Basic Info window's buttons), compact (bars or
// rings, as chosen) and folded to a ring icon; a click on the portrait goes round them. Low HP: its bar (its ring)
// red, «!», a short warning under it. The parts that need no Windows; the words go over it by GDI (win/char_block).

enum class Look : uint8_t { Bars, Rings };       // the compact block's: [interface] character = bars | rings
enum class View : uint8_t { Full, Compact, Icon };
View nextView(View v);  // a click on the portrait: full, compact, the icon, full again
// 3.0.11-beta.141 (the owner: «блок с персонажем должен работать от альт-V»): Alt+V, the game's own key for its Basic
// Info window (its button 0x88: the window full or small) - the full block to `small` (the small one last used:
// compact or the icon), a small one to the full
View altV(View shown, View small);

struct Data {
    std::string name, job;
    int baseLevel = 0, jobLevel = 0;
    int hp = 0, hpMost = 0, sp = 0, spMost = 0;
    float baseExp = -1, jobExp = -1;  // the level's part done (0..1); < 0 not known
    int weight = -1;                  // per cent of the most; < 0 not known
    // 3.0.11-beta.139 (the owner: «в интерфейсе вес не работает ... или не правильно считает»): the game's own numbers
    // (whole units, as its Basic Info window showed them) - in the rings' tip; most 0 not known
    int weightNow = -1, weightMost = 0;
    // 3.0.11-beta.139 (the owner: «пишет что 60% но хп воостанавливается»): the server's own overweight state, its
    // status icons - 1 no natural regen (SI_WEIGHT50; uaRO's natural_heal_weight_rate is 70 %, not 50), 2 no attacks
    // nor skills (SI_WEIGHT90, 90 %), 0 none; < 0 not known: by the classic 50 % and 90 %
    int overweight = -1;
    long long zeny = -1;              // < 0 not known
    int statusPoints = 0;
    const panel::Picture* portrait = nullptr;  // the head, at the portrait's side; none: the name's first letter
    // 3.0.11-beta.114 (the concept «Концепция окон и инвентаря», question 10 «можна»): the arrows (bullets) worn, how
    // many; 0 ran out (gone from the bag), < 0 none worn - «Стріли N» on the info line under 50
    int ammo = -1;
    // 3.0.11-beta.141 (the owner: «прогресбар с бейз опытом и джоб опытом, также желательно там же написать и точное
    // количество опыта. и побольше подробностей»): the experience in the game's numbers - the level's now and the next
    // level's (0: the last level; < 0 not known), its pace an hour (< 0 not known yet: core/exp_rate), gained this
    // session and the last gain; the free skill points
    long long baseNow = -1, baseNext = -1, jobNow = -1, jobNext = -1;
    double basePerHour = -1, jobPerHour = -1;
    long long baseSession = 0, jobSession = 0, baseLast = 0, jobLast = 0;
    int skillPoints = 0;
    // 3.0.11-beta.235 (the concept «Блок персонажа»: «больше информации», «подсказки когда наводишь на непонятное»):
    // the guild's and the party's names ("" none), the party's members in the game and all; the natural regen a tick
    // (-1 not known) and its period; the battle's numbers as «Характеристики» has them (atk < 0 not known); this
    // session's minutes (< 0 not known), its zeny gained (less spent), the monsters the character hit that died, its
    // deaths
    std::string guild, party;
    int partyOnline = 0, partyTotal = 0;
    int hpRegen = -1, spRegen = -1, hpEverySec = 6, spEverySec = 8;
    int atk = -1, atk2 = 0, def = 0, def2 = 0, flee = 0, flee2 = 0, aspd = 0;
    long long sessionMinutes = -1, zenyGain = 0;
    int kills = 0, deaths = 0;
};
constexpr int kAmmoLow = 50, kAmmoVeryLow = 10;  // 3.0.11-beta.114: amber under 50, red at 10 or fewer
bool lowHp(const Data& d);  // under a quarter of the most, as the game's own gauges turn red
const char* jobName(int job);  // the server's English (conf/msg_athena.conf 550-620); "" not known
std::string grouped(long long n);  // "1 482 300", the thousands apart by no-break spaces
std::string percent(float part, char decimal);  // "46,3 %"
// 3.0.11-beta.141: the time to a level, «12 год 48 хв» - minutes under an hour, hours and minutes under ten hours,
// whole hours from ten (rounded), «> 999 год» past it; under a minute one
std::string duration(long long minutes, const std::string& hour, const std::string& minute);

// The Basic Info window's buttons (its own commands - FUN_0046c980 opens or closes each window): stats, equip,
// items, skills, friends, options
constexpr int kButtons = 6;
struct Button {
    int command;
    uiicons::Icon icon;
};
extern const Button kButton[kButtons];

// 3.0.11-beta.235: the full block's parts that tell more in a tip (Layout::zones)
enum class Zone : uint8_t { Weight, Zeny, Social, Points, HpRegen, SpRegen, Atk, Def, Flee, Aspd, Time, Gain, Kills,
                            Deaths };
constexpr int kZones = 14;
bool opensStats(Zone z);  // the battle's numbers: a click opens «Характеристики»

struct Layout {
    float s = 1;
    View view = View::Full;
    Look look = Look::Bars;
    int x = 0, y = 0, w = 0, h = 0;  // the picture in the target
    panel::Box block;                // what takes the mouse: the glass, the rings' disc
    panel::Box portrait;             // its circle's box
    panel::Box name, levels, info;   // full: the name and class line, the levels, the weight / zeny / points line
    panel::Box hpLabel, spLabel, hpBar, spBar, hpText, spText;  // full and compact bars
    // full: the base's experience row (its word, its bar, its per cent; its numbers under it) and the job's
    // (3.0.11-beta.141); compact: the two bars thin at its foot
    panel::Box baseLabel, expBar, basePct, baseText, jobLabel, jobBar, jobPct, jobText;
    panel::Box baseRow, jobRow;      // full: the rows the mouse points at (their tips)
    panel::Box points;               // full: the free points' line under the info (3.0.11-beta.141)
    panel::Box badge;                // the level on the rings (rings, the icon)
    float cx = 0, cy = 0, hpR = 0, hpW = 0, spR = 0, spW = 0;  // the rings: their middle, outer radii and widths
    panel::Box pointsDot, weightDot;  // the rings': free points (gold), heavy (orange)
    panel::Box buttons[kButtons];     // full
    panel::Box under;                 // under the block: the low HP warning, then a tip
    // 3.0.11-beta.235 (full): the social line, the weight's bar, the regen lines, the battle's four cells and the
    // session's four (in place of the experience rows at the last levels); each Zone's box (w 0: none)
    panel::Box social, weightBar, hpRegen, spRegen, battle[4], session[4], sessionHead;
    bool sessionShown = false;
    panel::Box zones[kZones];
};
// 3.0.11-beta.235: both levels the last - the session in place of the experience rows
bool lastLevels(const Data& d);
// Its top left at (left, top) in the target, its sizes at `scale`
Layout layout(View view, Look look, int left, int top, float scale, bool sessionRows = false);

enum class Part : uint8_t { None, Portrait, Button, Body, Exp, Zone };
struct Hit {
    Part part = Part::None;
    int index = -1;  // the button; the experience's row (0 the base, 1 the job)
};
Hit hitTest(const Layout& L, float x, float y);  // the picture's pixels

struct Texts {
    std::string weight;            // «Вага»
    std::string points;            // «очко|очки|очок»: the free points' word by their count (one, a few, many)
    std::string lowHp;             // «Мало HP · {1} %»
    std::string ammo;              // 3.0.11-beta.114: «Стріли»
    std::string overweight1, overweight2;  // 3.0.11-beta.139: what the overweight states stop (the rings' tip)
    std::string buttons[kButtons];  // «Характеристики · Alt+A» ...
    char decimal = ',';
    // 3.0.11-beta.141: the experience's words - the free skill points' word by their count, the last level's, an
    // hour's and a minute's, the tip's lines («До рівня {1}», «Рівень через ≈ {1}», «За годину +{1}», «За сесію +{1}»,
    // «Остання здобич +{1}»), the portrait's tip («Alt+V — згорнути», «Alt+V — розгорнути»)
    std::string skillPoints, expMax, hour, minute;
    std::string toLevel, levelIn, perHour, session, lastGain;
    std::string fold, unfold;
    // 3.0.11-beta.235: «Гільдія {1}», «Група {1} · {2}/{3}», «+{1} кожні {2} с», «повне», «до повного ≈ {1}», «не
    // відновлюється», «Сесія · {1}», «вбито», «смертей»; the tips' lines (the parts pointed at)
    std::string guild, party, regen, regenFull, regenToFull, regenNone, sessionHead, killed, deaths;
    std::string tipWeight, tipPoints, tipSkillPoints, tipRegen1, tipRegen2, tipAtk, tipDef, tipFlee, tipAspd, tipOpen;
    std::string tipTime, tipZeny, tipKills, tipDeaths, tipOnline;
};
struct Show {
    int hoverButton = -1;
    bool hoverBlock = false;  // the rings and the icon: their numbers in a tip
    bool warning = false;     // the low HP warning (its first 2 seconds)
    bool warningApart = false;  // 3.0.11-beta.46: its place kept (a tip under it), the warning drawn apart
    bool tip = true;            // 3.0.11-beta.51: the tip (the concept's: after 0.4 s pointed at)
    int pressedButton = -1;     // pressed: 1 px down, its backing darker
    int hoverExp = -1;          // 3.0.11-beta.141: the full block's experience row pointed at (0 the base, 1 the job)
    int hoverZone = -1;         // 3.0.11-beta.235: the Zone pointed at (its tip)
    bool hoverPortrait = false;  // 3.0.11-beta.141: the portrait pointed at (its tip tells Alt+V)
};
void paint(panel::Picture& out, const Layout& L, const Data& d, const Texts& t, const Show& show);
// 3.0.11-beta.46 (the concept's motion): the low HP warning alone - its own picture of the block's size, so that it
// slides in from above and fades (win/char_block); nothing when the HP is not low
void paintWarning(panel::Picture& out, const Layout& L, const Data& d, const Texts& t);
// A new level: the block's edge in gold (the rings: a gold ring round them), drawn over the block flaring once
void paintFlare(panel::Picture& out, const Layout& L);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xECEFF5;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Data& d, const Texts& t, const Show& show);
std::vector<Words> warningWords(const Layout& L, const Data& d, const Texts& t);  // paintWarning's

}  // namespace uagfx::charblock
