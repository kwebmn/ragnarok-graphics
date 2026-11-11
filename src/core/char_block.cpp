#include "core/char_block.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/exp_rate.hpp"
#include "core/ui_style.hpp"

namespace uagfx::charblock {
namespace {

// The concept's sizes at scale 1 (v17_char): the full block 372 wide, its portrait 54, its bars 10, the experience 6
// (3.0.11-beta.141: the base's and the job's, their numbers under them); the compact one 300 x 74 (portrait 46, the
// experience's two thin lines at its foot); the rings round a 62 portrait (HP 5 outer, SP 4 inner); the icon's round 36
constexpr float kFullW = 372, kPad = 12, kPortrait = 54, kNumbersW = 124, kButtonW = 32, kButtonH = 26, kButtonGap = 6;
// 3.0.11-beta.235 (the concept «Блок персонажа»): the full block's rows - the social line, the regen under HP and SP,
// the battle's cells; the experience rows (or the session's) and the buttons after them
constexpr float kHpY = 94, kSpY = 126, kBattleY = 160, kCellH = 32, kLowerY = 200, kRowStep = 34;
constexpr float kCompactW = 300, kCompactH = 74, kCompactPortrait = 46;
constexpr float kRingHp = 46, kRingHpW = 5, kRingSp = 39, kRingSpW = 4, kRingPortrait = 31;
constexpr float kIconHp = 28, kIconHpW = 4, kIconSp = 23, kIconSpW = 3, kIconPortrait = 18;
// under the block: the low HP warning, then a tip of up to 8 lines (3.0.11-beta.141: the experience's in them)
constexpr float kUnderW = 300, kWarnH = 30, kTipH = 144, kGap = 8, kRadius = 10, kCorner = 11;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kTile = 0x1B1D24;
constexpr uint32_t kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xECEFF5, kMuted = 0x9AA3B5, kWarn = 0xF0A050;
constexpr uint32_t kHp = 0x5BD36C, kHpLow = 0xFF5A4E, kSp = 0x4F8BFF, kWarnGround = 0x3A1414;
constexpr uint32_t kBaseExp = 0xB7A6FF, kJobExp = 0x7FD3C3;  // 3.0.11-beta.141: the concept's experience colours
constexpr uint32_t kFight = 0xF3D27A;  // 3.0.11-beta.238: the last fight's cost on the bars, yellow

float part(int value, int most) { return most > 0 ? std::clamp(float(value) / float(most), 0.0f, 1.0f) : 0.0f; }

std::string replaced(std::string text, const std::string& value) {
    const size_t at = text.find("{1}");
    if (at != std::string::npos) text.replace(at, 3, value);
    return text;
}

std::string replaced2(std::string text, const std::string& value) {  // 3.0.11-beta.235: «{2}»
    const size_t at = text.find("{2}");
    if (at != std::string::npos) text.replace(at, 3, value);
    return text;
}

// «+3 очки»: the count, its word of «a|b|c» by it - one, a few, many (the Slavic rule); of «a|b» one, many
std::string counted(const std::string& forms, int n) {
    std::vector<std::string> f;
    size_t from = 0;
    for (size_t at; (at = forms.find('|', from)) != std::string::npos; from = at + 1)
        f.push_back(forms.substr(from, at - from));
    f.push_back(forms.substr(from));
    size_t i = 0;
    const bool few = n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 12 || n % 100 > 14);
    if (f.size() >= 3) i = n % 10 == 1 && n % 100 != 11 ? 0 : few ? 1 : 2;
    else if (f.size() == 2) i = n == 1 ? 0 : 1;
    return "+" + std::to_string(n) + " " + f[i];
}

std::string levels(const Data& d) {
    return "Base " + std::to_string(d.baseLevel) + " \xC2\xB7 Job " + std::to_string(d.jobLevel);
}

std::string pair(int value, int most) { return grouped(value) + " / " + grouped(most); }

int lowPercent(const Data& d) { return int(std::lround(part(d.hp, d.hpMost) * 100)); }

bool rings(const Layout& L) { return L.view == View::Icon || (L.view == View::Compact && L.look == Look::Rings); }

// 3.0.11-beta.139: the overweight state - the server's when known, else by the classic 50 % and 90 %
int overweightOf(const Data& d) {
    if (d.overweight >= 0) return d.overweight;
    return d.weight >= 90 ? 2 : d.weight >= 50 ? 1 : 0;
}

// 3.0.11-beta.141: one of the two experiences - its share, the level's numbers, its pace and gains
struct Exp {
    const char* name;
    float share;
    long long now, next;
    double perHour;
    long long session, last;
};
Exp baseOf(const Data& d) {
    return Exp{"Base", d.baseExp, d.baseNow, d.baseNext, d.basePerHour, d.baseSession, d.baseLast};
}
Exp jobOf(const Data& d) { return Exp{"Job", d.jobExp, d.jobNow, d.jobNext, d.jobPerHour, d.jobSession, d.jobLast}; }
bool known(const Exp& e) { return e.share >= 0 || e.next >= 0; }
bool numbers(const Exp& e) { return e.next > 0 && e.now >= 0; }

// A per cent not held to 100 (a pace, a session's gain): "4,2 %", from 100 whole
std::string share(double part, char decimal) {
    char buf[32];
    const double v = std::max(0.0, part * 100.0);
    std::snprintf(buf, sizeof buf, v >= 100 ? "%.0f" : "%.1f", v);
    std::string out = buf;
    for (char& c : out)
        if (c == '.') c = decimal;
    return out + "\xC2\xA0%";
}

// Its pace: "+4,2 %/год" (under 0,1 % an hour its exp: "+12 345/год"), the time to the level after it (`time`)
std::string pace(const Exp& e, const Texts& t, bool time) {
    if (e.perHour <= 0 || e.next <= 0) return {};
    const double part = e.perHour / double(e.next);
    std::string out = "+" + (part >= 0.001 ? share(part, t.decimal) : grouped(std::llround(e.perHour))) + "/" + t.hour;
    const long long minutes = exprate::minutesTo(e.now, e.next, e.perHour);
    if (time && e.now >= 0 && minutes >= 0)
        out += " \xC2\xB7 \xE2\x89\x88 " + duration(minutes, t.hour, t.minute);
    return out;
}

// «Base 46,3 % · 1 234 567 / 2 666 000»; the last level «Base макс.»
std::string expLine(const Exp& e, const Texts& t) {
    if (!known(e)) return {};
    std::string out =
        std::string(e.name) + " " + (e.next == 0 ? t.expMax : percent(std::max(e.share, 0.0f), t.decimal));
    if (numbers(e)) out += " \xC2\xB7 " + grouped(e.now) + " / " + grouped(e.next);
    return out;
}

// Pointed at an experience row: its line, what is left to the level, the pace an hour, the time to the level, this
// session's gain, the last gain
std::vector<std::string> expTip(const Exp& e, const Texts& t) {
    std::vector<std::string> lines;
    if (const std::string line = expLine(e, t); !line.empty()) lines.push_back(line);
    if (numbers(e) && !t.toLevel.empty()) lines.push_back(replaced(t.toLevel, grouped(std::max(0LL, e.next - e.now))));
    if (e.perHour > 0 && e.next > 0) {
        if (!t.perHour.empty())
            lines.push_back(replaced(t.perHour, grouped(std::llround(e.perHour)) + " \xC2\xB7 " +
                                                    share(e.perHour / double(e.next), t.decimal)));
        const long long minutes = exprate::minutesTo(e.now, e.next, e.perHour);
        if (e.now >= 0 && minutes >= 0 && !t.levelIn.empty())
            lines.push_back(replaced(t.levelIn, duration(minutes, t.hour, t.minute)));
    }
    if (e.session > 0 && !t.session.empty())
        lines.push_back(replaced(t.session, grouped(e.session) + (e.next > 0 ? " \xC2\xB7 " +
                                                                               share(double(e.session) / double(e.next),
                                                                                     t.decimal)
                                                                             : std::string())));
    if (e.last > 0 && !t.lastGain.empty()) lines.push_back(replaced(t.lastGain, grouped(e.last)));
    return lines;
}

// The weight's line of a tip: «Вага 1 710 / 4 500 · 38 %», what the overweight stops after it
void weightLines(std::vector<std::string>& lines, const Data& d, const Texts& t) {
    if (d.weightMost > 0 && d.weightNow >= 0 && d.weight >= 0)  // 3.0.11-beta.139: the weight's numbers
        lines.push_back(t.weight + " " + pair(d.weightNow, d.weightMost) + " \xC2\xB7 " + std::to_string(d.weight) +
                        "\xC2\xA0%");
    const int over = overweightOf(d);  // 3.0.11-beta.139: what the overweight stops
    if (over == 1 && !t.overweight1.empty()) lines.push_back(t.overweight1);
    if (over == 2 && !t.overweight2.empty()) lines.push_back(t.overweight2);
}

// 3.0.11-beta.238: the regen a minute as the server gives it - the natural each period (twice as often sitting), the
// skills' each 10 s (uAthena natural_heal_skill_interval), the sitting skills' each 10 s while sitting
long long perMinute(int natural, int every, int skill, int sitSkill, bool sitting) {
    if (natural < 0) return -1;
    const long long nat = every > 0 ? (long long)natural * 60 / every * (sitting ? 2 : 1) : 0;
    return nat + (long long)skill * 6 + (sitting ? (long long)sitSkill * 6 : 0);
}

// 3.0.11-beta.235: a Zone's tip - what the part pointed at means
std::vector<std::string> zoneTip(Zone z, const Data& d, const Texts& t) {
    std::vector<std::string> lines;
    const auto add = [&](const std::string& line) {
        if (!line.empty()) lines.push_back(line);
    };
    switch (z) {
        case Zone::Weight:
            weightLines(lines, d, t);
            add(t.tipWeight);
            break;
        case Zone::Zeny:
            if (d.zeny >= 0) add(grouped(d.zeny) + " z");
            if (d.sessionMinutes >= 0) add(replaced(t.tipZeny, (d.zenyGain >= 0 ? "+" : "") + grouped(d.zenyGain)));
            break;
        case Zone::Social:
            if (!d.guild.empty()) add(replaced(t.guild, d.guild));
            if (!d.party.empty()) {
                add(replaced(t.party, d.party));
                if (d.partyTotal > 0)
                    add(replaced2(replaced(t.tipOnline, std::to_string(d.partyOnline)), std::to_string(d.partyTotal)));
            }
            break;
        case Zone::Points:
            if (d.statusPoints > 0) add(t.tipPoints);
            if (d.skillPoints > 0) add(t.tipSkillPoints);
            break;
        case Zone::HpRegen:
        case Zone::SpRegen: {
            const bool hp = z == Zone::HpRegen;
            const int v = hp ? d.hpRegen : d.spRegen;
            if (v < 0) break;
            // 3.0.11-beta.238: each source of it - the natural (sitting twice as often), the skills'
            const int every = hp ? d.hpEverySec : d.spEverySec;
            add(std::string(hp ? "HP " : "SP ") +
                replaced2(replaced(t.regenNatural.empty() ? t.regen : t.regenNatural, std::to_string(v)),
                          std::to_string(d.sitting ? every / 2 : every)));
            if (const int sk = hp ? d.hpSkill : d.spSkill; sk > 0) add(replaced(t.regenSkill, std::to_string(sk)));
            if (const int sk = hp ? d.hpSitSkill : d.spSitSkill; sk > 0 && d.sitting)
                add(replaced(t.regenSitSkill, std::to_string(sk)));
            if (overweightOf(d) >= 1) add(t.overweight1);
            add(t.tipRegen1);
            add(t.tipRegen2);
            break;
        }
        case Zone::Atk:
            if (d.atk >= 0) add(replaced2(replaced(t.tipAtk, std::to_string(d.atk)), std::to_string(d.atk2)));
            add(t.tipOpen);
            break;
        case Zone::Def:
            if (d.atk >= 0) add(replaced2(replaced(t.tipDef, std::to_string(d.def)), std::to_string(d.def2)));
            add(t.tipOpen);
            break;
        case Zone::Flee:
            if (d.atk >= 0) add(replaced2(replaced(t.tipFlee, std::to_string(d.flee)), std::to_string(d.flee2)));
            add(t.tipOpen);
            break;
        case Zone::Aspd:
            if (d.atk >= 0 && d.aspd > 0 && d.aspd < 200) {
                char hits[16];
                std::snprintf(hits, sizeof hits, "%.2f", 50.0 / double(200 - d.aspd));
                std::string h = hits;
                for (char& c : h)
                    if (c == '.') c = t.decimal;
                add(replaced(t.tipAspd, h));
            }
            add(t.tipOpen);
            break;
        case Zone::Time:
            if (d.sessionMinutes >= 0) add(replaced(t.tipTime, duration(d.sessionMinutes, t.hour, t.minute)));
            break;
        case Zone::Gain:
            if (d.sessionMinutes >= 0) add(replaced(t.tipZeny, (d.zenyGain >= 0 ? "+" : "") + grouped(d.zenyGain)));
            break;
        case Zone::Kills: add(t.tipKills); break;
        case Zone::Deaths: add(t.tipDeaths); break;
    }
    return lines;
}

// The tip's lines: a button's words; an experience row's (3.0.11-beta.141); the rings' numbers; the portrait's Alt+V;
// the compact block's numbers
std::vector<std::string> tipLines(const Layout& L, const Data& d, const Texts& t, const Show& show) {
    if (!show.tip) return {};
    if (L.view == View::Full && show.hoverButton >= 0 && show.hoverButton < kButtons)
        return {t.buttons[show.hoverButton]};
    if (L.view == View::Full && (show.hoverExp == 0 || show.hoverExp == 1))
        return expTip(show.hoverExp == 0 ? baseOf(d) : jobOf(d), t);
    if ((L.view == View::Full || rings(L)) && show.hoverZone >= 0 && show.hoverZone < kZones) {  // 3.0.11-beta.235
        std::vector<std::string> lines = zoneTip(Zone(show.hoverZone), d, t);
        if (rings(L) && show.hoverZone == int(Zone::Points) && d.statusPoints > 0)  // 3.0.11-beta.236: how many
            lines.insert(lines.begin(), counted(t.points, d.statusPoints));
        if (!lines.empty() || L.view == View::Full) return lines;
    }
    const std::string& key = L.view == View::Full ? t.fold : t.unfold;
    const auto experience = [&](std::vector<std::string>& lines) {
        for (const Exp& e : {baseOf(d), jobOf(d)})
            if (const std::string line = expLine(e, t); !line.empty()) lines.push_back(line);
    };
    if (rings(L) && show.hoverBlock && d.hpMost > 0) {
        std::vector<std::string> lines{"HP " + pair(d.hp, d.hpMost), "SP " + pair(d.sp, d.spMost), levels(d)};
        experience(lines);
        weightLines(lines, d, t);
        if (!key.empty()) lines.push_back(key);
        return lines;
    }
    if (show.hoverPortrait && !rings(L)) return key.empty() ? std::vector<std::string>{} : std::vector{key};
    if (L.view == View::Compact && show.hoverBlock) {  // the compact block's body: what it does not show
        std::vector<std::string> lines{levels(d)};
        experience(lines);
        weightLines(lines, d, t);
        if (!key.empty()) lines.push_back(key);
        return lines;
    }
    return {};
}

}  // namespace

View nextView(View v) { return v == View::Full ? View::Compact : v == View::Compact ? View::Icon : View::Full; }

View altV(View shown, View small) {
    if (shown != View::Full) return View::Full;
    return small == View::Full ? View::Compact : small;
}

bool lowHp(const Data& d) { return d.hpMost > 0 && d.hp * 4 < d.hpMost; }

bool lastLevels(const Data& d) { return d.baseNext == 0 && d.jobNext == 0; }

bool opensStats(Zone z) { return z == Zone::Atk || z == Zone::Def || z == Zone::Flee || z == Zone::Aspd; }

const char* jobName(int job) {
    static const char* const kFirst[] = {
        "Novice",     "Swordsman",  "Mage",     "Archer",       "Acolyte",    "Merchant", "Thief",
        "Knight",     "Priest",     "Wizard",   "Blacksmith",   "Hunter",     "Assassin", "Knight",
        "Crusader",   "Monk",       "Sage",     "Rogue",        "Alchemist",  "Bard",     "Dancer",
        "Crusader",   "Wedding",    "Super Novice", "Gunslinger", "Ninja",    "Christmas"};
    static const char* const kHigh[] = {  // 4001 on: the transcendent, the babies, the Taekwon's
        "High Novice",   "High Swordsman", "High Mage",     "High Archer",    "High Acolyte",  "High Merchant",
        "High Thief",    "Lord Knight",    "High Priest",   "High Wizard",    "Whitesmith",    "Sniper",
        "Assassin Cross", "Lord Knight",   "Paladin",       "Champion",       "Professor",     "Stalker",
        "Creator",       "Clown",          "Gypsy",         "Paladin",        "Baby Novice",   "Baby Swordsman",
        "Baby Mage",     "Baby Archer",    "Baby Acolyte",  "Baby Merchant",  "Baby Thief",    "Baby Knight",
        "Baby Priest",   "Baby Wizard",    "Baby Blacksmith", "Baby Hunter",  "Baby Assassin", "Baby Knight",
        "Baby Crusader", "Baby Monk",      "Baby Sage",     "Baby Rogue",     "Baby Alchemist", "Baby Bard",
        "Baby Dancer",   "Baby Crusader",  "Super Baby",    "Taekwon",        "Star Gladiator", "Star Gladiator",
        "Soul Linker"};
    if (job >= 0 && job < int(std::size(kFirst))) return kFirst[job];
    if (job >= 4001 && job < 4001 + int(std::size(kHigh))) return kHigh[job - 4001];
    return "";
}

std::string grouped(long long n) {
    std::string digits = std::to_string(n < 0 ? -n : n), out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i && (digits.size() - i) % 3 == 0) out += "\xC2\xA0";
        out += digits[i];
    }
    return n < 0 ? "-" + out : out;
}

std::string percent(float share, char decimal) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f", double(std::clamp(share, 0.0f, 1.0f)) * 100.0);
    std::string out = buf;
    for (char& c : out)
        if (c == '.') c = decimal;
    return out + "\xC2\xA0%";
}

std::string duration(long long minutes, const std::string& hour, const std::string& minute) {
    minutes = std::max(1LL, minutes);
    if (minutes < 60) return std::to_string(minutes) + " " + minute;
    if (minutes < 600) {
        const long long m = minutes % 60;
        return std::to_string(minutes / 60) + " " + hour + (m ? " " + std::to_string(m) + " " + minute : "");
    }
    const long long hours = (minutes + 30) / 60;
    return hours > 999 ? "> 999 " + hour : std::to_string(hours) + " " + hour;
}

const Button kButton[kButtons] = {{0x7E, uiicons::Icon::Stats},   {0x81, uiicons::Icon::Equip},
                                  {0x80, uiicons::Icon::Bag},     {0x82, uiicons::Icon::Skill},
                                  {0x85, uiicons::Icon::Friends}, {0x7F, uiicons::Icon::Gear}};

Layout layout(View view, Look look, int left, int top, float scale, bool sessionRows) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    L.view = view;
    L.look = look;
    L.x = left;
    L.y = top;
    const bool round = view == View::Icon || (view == View::Compact && look == Look::Rings);
    if (round) {
        const bool icon = view == View::Icon;
        L.hpR = (icon ? kIconHp : kRingHp) * s;
        L.hpW = (icon ? kIconHpW : kRingHpW) * s;
        L.spR = (icon ? kIconSp : kRingSp) * s;
        L.spW = (icon ? kIconSpW : kRingSpW) * s;
        const float r = (icon ? kIconPortrait : kRingPortrait) * s, edge = 3 * s;
        L.cx = L.hpR + edge;
        L.cy = L.hpR + edge;
        L.portrait = panel::Box{L.cx - r, L.cy - r, 2 * r, 2 * r};
        const float bw = (icon ? 26 : 32) * s, bh = (icon ? 15 : 18) * s;
        L.badge = panel::Box{L.cx - bw / 2, L.cy + L.hpR - bh * 0.6f, bw, bh};
        const float dot = (icon ? 4 : 5) * s, at = L.hpR - L.hpW / 2;
        L.pointsDot = panel::Box{L.cx + at * 0.707f - dot, L.cy - at * 0.707f - dot, 2 * dot, 2 * dot};
        L.weightDot = panel::Box{L.cx + at * 0.906f - dot, L.cy - at * 0.423f - dot, 2 * dot, 2 * dot};
        L.block = panel::Box{0, 0, 2 * L.cx, L.badge.y + L.badge.h};
    } else if (view == View::Compact) {
        const float w = kCompactW * s, p = 10 * s, pic = kCompactPortrait * s;
        L.block = panel::Box{0, 0, w, kCompactH * s};
        L.portrait = panel::Box{p, p, pic, pic};
        const float x0 = p + pic + 10 * s, right = w - 12 * s;
        L.hpBar = panel::Box{x0, 14 * s, right - x0, 10 * s};
        L.spBar = panel::Box{x0, 29 * s, right - x0, 8 * s};
        L.hpText = panel::Box{x0, 42 * s, 130 * s, 16 * s};
        L.spText = panel::Box{right - 130 * s, 42 * s, 130 * s, 16 * s};
        L.expBar = panel::Box{p, 62 * s, w - 2 * p, 3 * s};  // 3.0.11-beta.141: the base's, the job's under it
        L.jobBar = panel::Box{p, 67 * s, w - 2 * p, 3 * s};
    } else {
        const float w = kFullW * s, p = kPad * s, pic = kPortrait * s;
        L.portrait = panel::Box{p, p, pic, pic};
        const float x0 = p + pic + 12 * s, right = w - p;
        L.name = panel::Box{x0, 14 * s, right - x0, 20 * s};
        L.levels = panel::Box{x0, 14 * s, right - x0, 20 * s};
        L.info = panel::Box{x0, 36 * s, right - x0, 17 * s};
        // 3.0.11-beta.235: the weight's bar after its word, the guild's and the party's line, the points' under it
        L.weightBar = panel::Box{x0 + 40 * s, 36 * s + 6 * s, 60 * s, 5 * s};
        L.social = panel::Box{x0, 54 * s, right - x0, 16 * s};
        L.points = panel::Box{x0, 71 * s, right - x0, 16 * s};
        const float numbersX = right - kNumbersW * s, barX = p + 36 * s, barW = numbersX - 8 * s - barX;
        L.hpLabel = panel::Box{p, kHpY * s, 30 * s, 16 * s};
        L.hpBar = panel::Box{barX, (kHpY + 3) * s, barW, 10 * s};
        L.hpText = panel::Box{numbersX, kHpY * s, kNumbersW * s, 16 * s};
        L.hpRegen = panel::Box{barX, (kHpY + 15) * s, right - barX, 13 * s};
        L.spLabel = panel::Box{p, kSpY * s, 30 * s, 16 * s};
        L.spBar = panel::Box{barX, (kSpY + 3) * s, barW, 10 * s};
        L.spText = panel::Box{numbersX, kSpY * s, kNumbersW * s, 16 * s};
        L.spRegen = panel::Box{barX, (kSpY + 15) * s, right - barX, 13 * s};
        const float cellGap = 6 * s, cellW = (right - p - 3 * cellGap) / 4;
        for (int i = 0; i < 4; ++i)
            L.battle[i] = panel::Box{p + float(i) * (cellW + cellGap), kBattleY * s, cellW, kCellH * s};
        // 3.0.11-beta.141: the experience's rows as HP's and SP's - the word, the bar, the per cent; its numbers under
        // them across the row
        const auto row = [&](float y, panel::Box& label, panel::Box& bar, panel::Box& pct, panel::Box& text,
                             panel::Box& whole) {
            label = panel::Box{p, y, 34 * s, 16 * s};
            bar = panel::Box{barX, y + 5 * s, barW, 6 * s};
            pct = panel::Box{numbersX, y, kNumbersW * s, 16 * s};
            text = panel::Box{barX, y + 16 * s, right - barX, 14 * s};
            whole = panel::Box{p, y, right - p, 30 * s};
        };
        float lowerEnd = 0;
        if (sessionRows) {  // 3.0.11-beta.235: the last levels - this session in place of the experience
            L.sessionShown = true;
            L.sessionHead = panel::Box{p, kLowerY * s, right - p, 14 * s};
            const float sw = (right - p - 2 * cellGap) / 3;
            for (int i = 0; i < 3; ++i)
                L.session[i] = panel::Box{p + float(i) * (sw + cellGap), (kLowerY + 18) * s, sw, kCellH * s};
            lowerEnd = (kLowerY + 18 + kCellH) * s;
        } else {
            row(kLowerY * s, L.baseLabel, L.expBar, L.basePct, L.baseText, L.baseRow);
            row((kLowerY + kRowStep) * s, L.jobLabel, L.jobBar, L.jobPct, L.jobText, L.jobRow);
            lowerEnd = (kLowerY + kRowStep + 30) * s;
        }
        const float buttonsY = lowerEnd + 10 * s;
        for (int i = 0; i < kButtons; ++i)
            L.buttons[i] =
                panel::Box{p + float(i) * (kButtonW + kButtonGap) * s, buttonsY, kButtonW * s, kButtonH * s};
        L.block = panel::Box{0, 0, w, buttonsY + kButtonH * s + p};
        L.zones[int(Zone::Weight)] = panel::Box{x0, L.info.y, 150 * s, L.info.h};
        L.zones[int(Zone::Zeny)] = panel::Box{x0 + 156 * s, L.info.y, right - x0 - 156 * s, L.info.h};
        L.zones[int(Zone::Social)] = L.social;
        L.zones[int(Zone::Points)] = L.points;
        L.zones[int(Zone::HpRegen)] = L.hpRegen;
        L.zones[int(Zone::SpRegen)] = L.spRegen;
        for (int i = 0; i < 4; ++i) L.zones[int(Zone::Atk) + i] = L.battle[i];
        if (L.sessionShown) {
            L.zones[int(Zone::Time)] = L.sessionHead;
            for (int i = 0; i < 3; ++i) L.zones[int(Zone::Gain) + i] = L.session[i];
        }
    }
    const float underY = L.block.y + L.block.h + kGap * s;
    L.w = int(std::ceil(std::max(L.block.w, kUnderW * s)));
    L.under = panel::Box{0, underY, float(L.w), (kWarnH + 6 + kTipH) * s};
    L.h = int(std::ceil(underY + L.under.h));
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    const bool round = rings(L);
    if (round) {
        // 3.0.11-beta.236 (the owner: «точки… для чего они?» - «подсказки когда наводишь на непонятное»): the rings'
        // dots their own - the free points', the weight's
        const auto near = [&](const panel::Box& b) {
            return b.w > 0 && std::hypot(x - (b.x + b.w / 2), y - (b.y + b.h / 2)) <= b.w / 2 + 3 * L.s;
        };
        if (near(L.pointsDot)) return Hit{Part::Zone, int(Zone::Points)};
        if (near(L.weightDot)) return Hit{Part::Zone, int(Zone::Weight)};
        if (std::hypot(x - L.cx, y - L.cy) <= L.hpR) return Hit{Part::Portrait, -1};  // the whole round: its button
        return L.badge.contains(x, y) ? Hit{Part::Portrait, -1} : Hit{};
    }
    if (!L.block.contains(x, y)) return Hit{};
    const panel::Box& p = L.portrait;
    if (std::hypot(x - (p.x + p.w / 2), y - (p.y + p.h / 2)) <= p.w / 2) return Hit{Part::Portrait, -1};
    for (int i = 0; i < kButtons; ++i)
        if (L.buttons[i].w > 0 && L.buttons[i].contains(x, y)) return Hit{Part::Button, i};
    if (L.baseRow.w > 0 && L.baseRow.contains(x, y)) return Hit{Part::Exp, 0};  // 3.0.11-beta.141
    if (L.jobRow.w > 0 && L.jobRow.contains(x, y)) return Hit{Part::Exp, 1};
    for (int i = 0; i < kZones; ++i)  // 3.0.11-beta.235
        if (L.zones[i].w > 0 && L.zones[i].contains(x, y)) return Hit{Part::Zone, i};
    return Hit{Part::Body, -1};
}

Model model(const Data& d, const Texts& t, View view, Look look, bool sessionRows) {
    Model m;
    const Layout L = layout(view, look, 0, 0, 1.0f, sessionRows);
    const bool low = lowHp(d);
    m.name = d.name;
    m.job = d.job;
    m.levels = levels(d);
    m.badge = std::to_string(d.baseLevel);
    if (!d.portrait || d.portrait->width <= 0) {
        const bool wide = !d.name.empty() && (static_cast<unsigned char>(d.name[0]) & 0xE0) == 0xC0;
        m.letter = d.name.empty() ? "?" : d.name.substr(0, wide ? 2 : 1);
    }
    m.low = low;
    if (low) m.lowText = "! " + replaced(t.lowHp, std::to_string(lowPercent(d)));
    if (d.weight >= 0) {
        const int over = overweightOf(d);
        m.weightShown = true;
        m.weightLabel = t.weight;
        m.weightPct = std::to_string(d.weight) + "\xC2\xA0%";
        m.weightTone = over == 2 ? "bad" : over == 1 ? "warn" : "";
        m.weightShare = std::clamp(float(d.weight) / 100.0f, 0.0f, 1.0f);
    }
    if (d.zeny >= 0) m.zenyShown = true, m.zeny = grouped(d.zeny);
    if (!d.guild.empty()) m.social.push_back({replaced(t.guild, d.guild), "muted"});
    if (!d.party.empty())
        m.social.push_back({replaced(t.party, d.party) +
                                (d.partyTotal > 0 ? " \xC2\xB7 " + std::to_string(d.partyOnline) + "/" +
                                                        std::to_string(d.partyTotal)
                                                  : std::string()),
                            "muted"});
    if (d.statusPoints > 0) m.points.push_back({counted(t.points, d.statusPoints), "gold"});
    if (d.skillPoints > 0 && !t.skillPoints.empty()) m.points.push_back({counted(t.skillPoints, d.skillPoints), "gold"});
    if (d.ammo >= 0 && d.ammo < kAmmoLow && !t.ammo.empty())
        m.points.push_back({t.ammo + " " + std::to_string(d.ammo), d.ammo <= kAmmoVeryLow ? "bad" : "warn"});
    if (!d.broken.empty() && !t.broken.empty()) m.points.push_back({replaced(t.broken, d.broken), "bad"});
    const auto fed = [&](const std::string& who, int full) {
        if (full < 0 || who.empty()) return;
        m.points.push_back({who + " " + std::to_string(full) + "\xC2\xA0%", full <= 10 ? "bad" : full <= 25 ? "warn" : ""});
    };
    fed(t.pet, d.pet);
    fed(t.homun, d.homun);
    m.hpShown = d.hpMost > 0;
    m.spShown = d.spMost > 0;
    if (m.hpShown) m.hp = pair(d.hp, d.hpMost), m.hpCompact = "HP " + grouped(d.hp);
    if (m.spShown) m.sp = pair(d.sp, d.spMost), m.spCompact = "SP " + grouped(d.sp);
    m.hpShare = d.hpMost > 0 ? std::clamp(float(d.hp) / float(d.hpMost), 0.0f, 1.0f) : 0;
    m.spShare = d.spMost > 0 ? std::clamp(float(d.sp) / float(d.spMost), 0.0f, 1.0f) : 0;
    if (d.costShown > 0) m.hpCost = std::max(0.0f, d.hpCost), m.spCost = std::max(0.0f, d.spCost);
    const auto regen = [&](Chip& left, Chip& right, int per, int every, int skill, int sitSkill, int now, int most,
                           float cost) {
        if (per < 0) return;
        if (overweightOf(d) >= 1) {
            left = {t.regenNone, "warn"};
            return;
        }
        const long long minute = perMinute(per, every, skill, sitSkill, d.sitting);
        left = {t.regenPerMin.empty()
                    ? replaced2(replaced(t.regen, std::to_string(per)), std::to_string(every))
                    : replaced2(replaced(t.regenPerMin, grouped(minute)), d.sitting ? t.sitting : t.standing),
                d.sitting ? "" : "muted"};
        if (cost > 0 && d.costShown > 0 && !t.fightCost.empty()) {
            right = {replaced(t.fightCost, percent(cost, t.decimal)), "fight"};
            return;
        }
        if (most <= 0) return;
        if (now >= most) right = {t.regenFull, "muted"};
        else if (minute > 0)
            right = {replaced(t.regenToFull, duration(((long long)(most - now) + minute - 1) / minute, t.hour, t.minute)),
                     "muted"};
    };
    regen(m.hpRegen, m.hpRegenRight, d.hpRegen, d.hpEverySec, d.hpSkill, d.hpSitSkill, d.hp, d.hpMost, d.hpCost);
    regen(m.spRegen, m.spRegenRight, d.spRegen, d.spEverySec, d.spSkill, d.spSitSkill, d.sp, d.spMost, d.spCost);
    if (d.atk >= 0) {
        m.battle = {{"ATK", std::to_string(d.atk) + "+" + std::to_string(d.atk2), ""},
                    {"DEF", std::to_string(d.def) + "+" + std::to_string(d.def2), ""},
                    {"FLEE", std::to_string(d.flee) + "+" + std::to_string(d.flee2), ""},
                    {"ASPD", std::to_string(d.aspd), ""}};
    }
    if (L.sessionShown && d.sessionMinutes >= 0) {
        m.sessionShown = true;
        m.sessionHead = replaced(t.sessionHead, duration(d.sessionMinutes, t.hour, t.minute));
        const long long perHour = d.sessionMinutes > 0 ? d.zenyGain * 60 / d.sessionMinutes : 0;
        m.session = {{(perHour >= 0 ? "+" : "") + grouped(perHour) + " z/" + t.hour,
                      (d.zenyGain >= 0 ? "+" : "") + grouped(d.zenyGain) + " z", d.zenyGain >= 0 ? "gold" : "bad"},
                     {t.killed, std::to_string(d.kills), ""},
                     {t.deaths, std::to_string(d.deaths), d.deaths > 0 ? "warn" : ""}};
    }
    const auto experience = [&](const Exp& e, ExpRow& row, int which) {
        if (!known(e)) return;
        row.shown = true;
        row.label = e.name;
        row.pct = e.next == 0 ? t.expMax : percent(std::max(e.share, 0.0f), t.decimal);
        row.share = std::clamp(e.share, 0.0f, 1.0f);
        Show sh;
        sh.hoverExp = which;
        const Layout full = layout(View::Full, look, 0, 0, 1.0f, sessionRows);
        row.tip = tipLines(full, d, t, sh);
        if (!numbers(e)) return;
        row.numbers = grouped(e.now) + " / " + grouped(e.next);
        const long long gain = which == 0 ? d.gainBase : d.gainJob;
        if (gain > 0 && d.gainShown > 0 && e.next > 0) {
            char buf[32];
            const double p = double(gain) / double(e.next) * 100;
            std::snprintf(buf, sizeof buf, p >= 10 ? " (%.1f\xC2\xA0%%)" : " (%.2f\xC2\xA0%%)", p);
            std::string g = "+" + grouped(gain) + buf;
            for (char& c : g)
                if (c == '.') c = t.decimal;
            row.right = g, row.rightTone = "fight";
            return;
        }
        row.right = pace(e, t, true), row.rightTone = "muted";
    };
    experience(baseOf(d), m.baseExp, 0);
    experience(jobOf(d), m.jobExp, 1);
    for (int z = 0; z < kZones; ++z) {
        Show sh;
        sh.hoverZone = z;
        m.zoneTips[z] = tipLines(L, d, t, sh);
    }
    Show body;
    body.hoverBlock = true;
    m.bodyTip = tipLines(L, d, t, body);
    m.portraitTip = view == View::Full ? t.fold : t.unfold;
    return m;
}

int stripHeight(float scale) { return std::max(5, int(std::lround(8 * (scale > 0 ? scale : 1.0f)))); }

std::array<StripBar, 2> stripBars(const Data& d) {
    const auto one = [](const Exp& e, uint32_t rgb) {
        return StripBar{e.next == 0 ? 1.0f : std::clamp(e.share, 0.0f, 1.0f), e.next == 0 ? 0.45f : 1.0f, rgb};
    };
    return {one(baseOf(d), kBaseExp), one(jobOf(d), kJobExp)};
}

std::vector<std::string> stripTip(const Data& d, const Texts& t) {
    std::vector<std::string> lines;
    for (const Exp& e : {baseOf(d), jobOf(d)}) {
        std::string line = expLine(e, t);
        if (line.empty()) continue;
        if (const std::string speed = pace(e, t, true); !speed.empty()) line += " \xC2\xB7 " + speed;
        lines.push_back(line);
    }
    return lines;
}

}  // namespace uagfx::charblock
