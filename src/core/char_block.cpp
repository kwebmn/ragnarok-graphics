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
constexpr float kButtonsY = 198, kBaseRowY = 126, kJobRowY = 160;
constexpr float kCompactW = 300, kCompactH = 74, kCompactPortrait = 46;
constexpr float kRingHp = 46, kRingHpW = 5, kRingSp = 39, kRingSpW = 4, kRingPortrait = 31;
constexpr float kIconHp = 28, kIconHpW = 4, kIconSp = 23, kIconSpW = 3, kIconPortrait = 18;
// under the block: the low HP warning, then a tip of up to 8 lines (3.0.11-beta.141: the experience's in them)
constexpr float kUnderW = 300, kWarnH = 30, kTipH = 144, kGap = 8, kRadius = 10, kCorner = 11;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kTile = 0x1B1D24;
constexpr uint32_t kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xECEFF5, kMuted = 0x9AA3B5, kWarn = 0xF0A050;
constexpr uint32_t kHp = 0x5BD36C, kHpLow = 0xFF5A4E, kSp = 0x4F8BFF, kWarnGround = 0x3A1414;
constexpr uint32_t kBaseExp = 0xB7A6FF, kJobExp = 0x7FD3C3;  // 3.0.11-beta.141: the concept's experience colours

float part(int value, int most) { return most > 0 ? std::clamp(float(value) / float(most), 0.0f, 1.0f) : 0.0f; }

float cover(float d) { return std::clamp(0.5f - d, 0.0f, 1.0f); }

// A ring `width` wide inside `outer` round (cx, cy): from twelve o'clock round to `share` of it
void ring(panel::Picture& p, float cx, float cy, float outer, float width, float share, uint32_t rgb, float a) {
    if (share <= 0) return;
    for (int y = std::max(0, int(cy - outer) - 1); y < std::min(p.height, int(cy + outer) + 2); ++y)
        for (int x = std::max(0, int(cx - outer) - 1); x < std::min(p.width, int(cx + outer) + 2); ++x) {
            const float px = float(x) + 0.5f, py = float(y) + 0.5f, r = panel::length(px - cx, py - cy);
            const float k = cover(r - outer) * cover(outer - width - r);
            if (k <= 0) continue;
            float soft = 1;
            if (share < 1) {
                float turn = std::atan2(px - cx, cy - py) / 6.2831853f;  // clockwise from twelve o'clock
                if (turn < 0) turn += 1;
                soft = std::clamp((share - turn) * 6.2831853f * std::max(r, 1.0f) + 0.5f, 0.0f, 1.0f);
            }
            if (soft > 0) panel::over(p.px[size_t(y) * size_t(p.width) + size_t(x)], rgb, a * k * soft);
        }
}

// A premultiplied picture inside a circle (its middle at the box's), over what is there
void putRound(panel::Picture& out, const panel::Picture& pic, const panel::Box& box) {
    const float cx = box.x + box.w / 2, cy = box.y + box.h / 2, r = box.w / 2;
    const int ox = int(std::lround(cx - float(pic.width) / 2)), oy = int(std::lround(cy - float(pic.height) / 2));
    for (int y = 0; y < pic.height; ++y)
        for (int x = 0; x < pic.width; ++x) {
            const int tx = ox + x, ty = oy + y;
            if (tx < 0 || ty < 0 || tx >= out.width || ty >= out.height) continue;
            const float k = cover(panel::length(float(tx) + 0.5f - cx, float(ty) + 0.5f - cy) - r);
            uint32_t c = pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
            if (k <= 0 || !(c >> 24)) continue;
            if (k < 1) {
                const auto ch = [&](int sh) { return uint32_t(float((c >> sh) & 0xFF) * k + 0.5f); };
                c = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
            }
            uint32_t& d = out.px[size_t(ty) * size_t(out.width) + size_t(tx)];
            const uint32_t keep = 255 - (c >> 24);
            const auto ch = [&](int sh) {
                return std::min<uint32_t>(255, ((c >> sh) & 0xFF) + ((d >> sh) & 0xFF) * keep / 255);
            };
            d = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
        }
}

// The panels' gold corner brackets (the concept's: 11 long, 2 wide, a dark edge)
void corners(panel::Picture& p, const panel::Box& b, float s) {
    const float len = kCorner * s, w = 2 * s, e = 1 * s;
    const panel::Box bars[] = {{b.x, b.y, len, w},         {b.x, b.y, w, len},  // the top left
                               {b.x + b.w - len, b.y, len, w}, {b.x + b.w - w, b.y, w, len},
                               {b.x, b.y + b.h - w, len, w},   {b.x, b.y + b.h - len, w, len},
                               {b.x + b.w - len, b.y + b.h - w, len, w}, {b.x + b.w - w, b.y + b.h - len, w, len}};
    for (const panel::Box& x : bars) panel::roundRect(p, panel::Box{x.x - e, x.y - e, x.w + 2 * e, x.h + 2 * e}, 0,
                                                      kBlack, 0.55f);
    for (const panel::Box& x : bars) panel::roundRect(p, x, 0, kGold, 1.0f);
}

void bar(panel::Picture& p, const panel::Box& b, float share, uint32_t rgb) {
    panel::roundRect(p, b, b.h / 2, kBlack, 0.45f);
    if (share > 0) panel::roundRect(p, panel::Box{b.x, b.y, std::max(b.h, b.w * share), b.h}, b.h / 2, rgb, 1.0f);
}

std::string replaced(std::string text, const std::string& value) {
    const size_t at = text.find("{1}");
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

// The tip's lines: a button's words; an experience row's (3.0.11-beta.141); the rings' numbers; the portrait's Alt+V;
// the compact block's numbers
std::vector<std::string> tipLines(const Layout& L, const Data& d, const Texts& t, const Show& show) {
    if (!show.tip) return {};
    if (L.view == View::Full && show.hoverButton >= 0 && show.hoverButton < kButtons)
        return {t.buttons[show.hoverButton]};
    if (L.view == View::Full && (show.hoverExp == 0 || show.hoverExp == 1))
        return expTip(show.hoverExp == 0 ? baseOf(d) : jobOf(d), t);
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

panel::Box warnBox(const Layout& L, const Data& d, const Texts& t, const Show& show) {
    if (!show.warning || !lowHp(d)) return panel::Box{};
    const std::string text = "! " + replaced(t.lowHp, std::to_string(lowPercent(d)));
    return panel::Box{L.under.x, L.under.y, panel::wordsWidth(text, 13 * L.s, true) + 24 * L.s, kWarnH * L.s};
}

panel::Box tipBox(const Layout& L, const Data& d, const Texts& t, const Show& show, const panel::Box& warn) {
    const std::vector<std::string> lines = tipLines(L, d, t, show);
    if (lines.empty()) return panel::Box{};
    float w = 0;
    for (const std::string& line : lines) w = std::max(w, panel::wordsWidth(line, 13 * L.s, true));
    const float y = warn.w > 0 ? warn.y + warn.h + 6 * L.s : L.under.y;
    float x = L.under.x;
    if (L.view == View::Full && show.hoverButton >= 0) x = L.buttons[show.hoverButton].x;  // under its button
    w += 20 * L.s;
    x = std::clamp(x, 0.0f, std::max(0.0f, float(L.w) - w));
    return panel::Box{x, y, w, (8 + 17 * float(lines.size())) * L.s};
}

}  // namespace

View nextView(View v) { return v == View::Full ? View::Compact : v == View::Compact ? View::Icon : View::Full; }

View altV(View shown, View small) {
    if (shown != View::Full) return View::Full;
    return small == View::Full ? View::Compact : small;
}

bool lowHp(const Data& d) { return d.hpMost > 0 && d.hp * 4 < d.hpMost; }

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

Layout layout(View view, Look look, int left, int top, float scale) {
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
        L.points = panel::Box{x0, 54 * s, right - x0, 16 * s};  // 3.0.11-beta.141: the free points, the arrows
        const float numbersX = right - kNumbersW * s, barX = p + 36 * s, barW = numbersX - 8 * s - barX;
        L.hpLabel = panel::Box{p, 80 * s, 30 * s, 16 * s};
        L.hpBar = panel::Box{barX, 83 * s, barW, 10 * s};
        L.hpText = panel::Box{numbersX, 80 * s, kNumbersW * s, 16 * s};
        L.spLabel = panel::Box{p, 102 * s, 30 * s, 16 * s};
        L.spBar = panel::Box{barX, 105 * s, barW, 10 * s};
        L.spText = panel::Box{numbersX, 102 * s, kNumbersW * s, 16 * s};
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
        row(kBaseRowY * s, L.baseLabel, L.expBar, L.basePct, L.baseText, L.baseRow);
        row(kJobRowY * s, L.jobLabel, L.jobBar, L.jobPct, L.jobText, L.jobRow);
        for (int i = 0; i < kButtons; ++i)
            L.buttons[i] =
                panel::Box{p + float(i) * (kButtonW + kButtonGap) * s, kButtonsY * s, kButtonW * s, kButtonH * s};
        L.block = panel::Box{0, 0, w, (kButtonsY + kButtonH) * s + p};
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
    return Hit{Part::Body, -1};
}

void paint(panel::Picture& out, const Layout& L, const Data& d, const Texts& t, const Show& show) {
    const float s = L.s;
    const bool low = lowHp(d);
    const uint32_t hpRgb = low ? kHpLow : kHp;
    if (rings(L)) {
        panel::circle(out, L.cx, L.cy, L.hpR + 1.5f * s, kBlack, 0.5f);  // the rings' ground
        ring(out, L.cx, L.cy, L.hpR, L.hpW, 1, kBlack, 0.55f);
        ring(out, L.cx, L.cy, L.hpR, L.hpW, part(d.hp, d.hpMost), hpRgb, 1.0f);
        ring(out, L.cx, L.cy, L.spR, L.spW, 1, kBlack, 0.55f);
        ring(out, L.cx, L.cy, L.spR, L.spW, part(d.sp, d.spMost), kSp, 1.0f);
        if (low) ring(out, L.cx, L.cy, L.hpR + 3 * s, 2 * s, 1, kHpLow, 0.45f);  // its glow
    } else {
        panel::roundRect(out, L.block, kRadius * s, kCard, uistyle::panel(), kWhite, 0.09f, 1.0f * s);
        if (L.view == View::Full) corners(out, L.block, s);
        bar(out, L.hpBar, part(d.hp, d.hpMost), hpRgb);
        bar(out, L.spBar, part(d.sp, d.spMost), kSp);
        // 3.0.11-beta.141: the base's experience and the job's (the concept's colours), the full block's a tick each
        // 10 %; the last level full and faint
        const auto experience = [&](const panel::Box& b, const Exp& e, uint32_t rgb) {
            if (b.w <= 0) return;
            panel::roundRect(out, b, b.h / 2, kBlack, 0.45f);
            const float f = e.next == 0 ? 1.0f : std::max(e.share, 0.0f);
            if (f > 0) panel::roundRect(out, panel::Box{b.x, b.y, std::max(b.h, b.w * f), b.h}, b.h / 2, rgb,
                                        e.next == 0 ? 0.45f : 1.0f);
            if (L.view == View::Full)
                for (int i = 1; i < 10; ++i)
                    panel::roundRect(out, panel::Box{b.x + b.w * float(i) / 10 - 0.5f * s, b.y, 1.0f * s, b.h}, 0,
                                     kBlack, 0.5f);
        };
        experience(L.expBar, baseOf(d), kBaseExp);
        experience(L.jobBar, jobOf(d), kJobExp);
        for (int i = 0; i < kButtons; ++i) {
            panel::Box b = L.buttons[i];
            if (b.w <= 0) continue;
            const bool hot = show.hoverButton == i, down = show.pressedButton == i;
            if (down) {  // 3.0.11-beta.51: the concept's press - 1 px down, darker
                b.y += 1 * s;
                panel::roundRect(out, b, 7 * s, kBlack, 0.30f, kGold, 0.8f, 1.0f * s);
            } else {
                panel::roundRect(out, b, 7 * s, kWhite, hot ? 0.14f : 0.06f, kGold, hot ? 0.8f : 0.0f, 1.0f * s);
            }
            const float icon = 18 * s;
            uiicons::draw(out, kButton[i].icon, b.x + (b.w - icon) / 2, b.y + (b.h - icon) / 2, icon,
                          hot ? kText : kMuted, 1.0f, 2.0f);
        }
        if (low && L.view == View::Full) {  // «!» before its numbers
            const float r = 7 * s, x = L.hpText.x - r - 2 * s;
            panel::circle(out, x, L.hpText.y + L.hpText.h / 2, r, kHpLow, 1.0f);
        }
    }
    // the portrait: the head on a dark round, its edge; none - the name's first letter (its words)
    const panel::Box& p = L.portrait;
    panel::circle(out, p.x + p.w / 2, p.y + p.h / 2, p.w / 2, kTile, 1.0f);
    if (d.portrait && d.portrait->width > 0) putRound(out, *d.portrait, p);
    ring(out, p.x + p.w / 2, p.y + p.h / 2, p.w / 2, 1.5f * s, 1, kWhite, 0.35f);
    if (low && !rings(L)) {  // 3.0.11-beta.46: the concept's - the portrait glows red (the rings have their own glow)
        ring(out, p.x + p.w / 2, p.y + p.h / 2, p.w / 2 + 3 * s, 3 * s, 1, kHpLow, 0.8f);  // its radius the outer
        ring(out, p.x + p.w / 2, p.y + p.h / 2, p.w / 2 + 7 * s, 4 * s, 1, kHpLow, 0.25f);
    }
    if (rings(L)) {
        panel::roundRect(out, L.badge, L.badge.h / 2, kCard, 0.95f, kGold, 1.0f, 1.2f * s);
        if (d.statusPoints > 0) {
            panel::circle(out, L.pointsDot.x + L.pointsDot.w / 2, L.pointsDot.y + L.pointsDot.h / 2,
                          L.pointsDot.w / 2 + 1.2f * s, kBlack, 0.7f);
            panel::circle(out, L.pointsDot.x + L.pointsDot.w / 2, L.pointsDot.y + L.pointsDot.h / 2,
                          L.pointsDot.w / 2, kGold2, 1.0f);
        }
        if (const int over = overweightOf(d); over > 0) {  // 3.0.11-beta.139: the server's state
            panel::circle(out, L.weightDot.x + L.weightDot.w / 2, L.weightDot.y + L.weightDot.h / 2,
                          L.weightDot.w / 2 + 1.2f * s, kBlack, 0.7f);
            panel::circle(out, L.weightDot.x + L.weightDot.w / 2, L.weightDot.y + L.weightDot.h / 2,
                          L.weightDot.w / 2, over == 2 ? kHpLow : kWarn, 1.0f);
        }
    }
    const panel::Box warn = warnBox(L, d, t, show);
    if (warn.w > 0 && !show.warningApart)
        panel::roundRect(out, warn, 9 * s, kWarnGround, 0.92f, kHpLow, 0.8f, 1.0f * s);
    const panel::Box tip = tipBox(L, d, t, show, warn);
    if (tip.w > 0) panel::roundRect(out, tip, 8 * s, kCard, 0.94f, kWhite, 0.12f, 1.0f * s);
}

void paintWarning(panel::Picture& out, const Layout& L, const Data& d, const Texts& t) {
    Show show;
    show.warning = true;
    const panel::Box warn = warnBox(L, d, t, show);
    if (warn.w > 0) panel::roundRect(out, warn, 9 * L.s, kWarnGround, 0.92f, kHpLow, 0.8f, 1.0f * L.s);
}

void paintFlare(panel::Picture& out, const Layout& L) {
    const float s = L.s;
    if (rings(L)) {
        ring(out, L.cx, L.cy, L.hpR + 2 * s, 2.5f * s, 1, kGold2, 1.0f);
        ring(out, L.cx, L.cy, L.hpR - L.hpW, 1.5f * s, 1, kGold2, 0.5f);
        return;
    }
    panel::roundRect(out, L.block, kRadius * s, kGold, 0.0f, kGold2, 1.0f, 2.0f * s);
    const panel::Box in{L.block.x + 2 * s, L.block.y + 2 * s, L.block.w - 4 * s, L.block.h - 4 * s};
    panel::roundRect(out, in, (kRadius - 2) * s, kGold, 0.10f, kGold, 0.45f, 2.0f * s);
}

std::vector<Words> words(const Layout& L, const Data& d, const Texts& t, const Show& show) {
    std::vector<Words> out;
    const float s = L.s;
    const bool low = lowHp(d);
    const panel::Box& p = L.portrait;
    if (!d.portrait || d.portrait->width <= 0) {  // the name's first letter in its round
        const bool wide = !d.name.empty() && (static_cast<unsigned char>(d.name[0]) & 0xE0) == 0xC0;  // UTF-8 of 2
        const std::string first = d.name.empty() ? "?" : d.name.substr(0, wide ? 2 : 1);
        out.push_back(Words{p, first, kText, p.w * 0.42f, true, Align::Center});
    }
    if (rings(L)) {
        out.push_back(Words{L.badge, std::to_string(d.baseLevel), kText, (L.view == View::Icon ? 10.5f : 12) * s, true,
                            Align::Center});
    } else if (L.view == View::Compact) {
        if (d.hpMost > 0) out.push_back(Words{L.hpText, "HP " + grouped(d.hp), low ? 0xFF9A8F : kText, 13 * s, true,
                                              Align::Left});
        if (d.spMost > 0) out.push_back(Words{L.spText, "SP " + grouped(d.sp), kText, 13 * s, true, Align::Right});
    } else {
        const float nameW = panel::wordsWidth(d.name, 16 * s, true);
        out.push_back(Words{panel::Box{L.name.x, L.name.y, nameW + 4 * s, L.name.h}, d.name, kText, 16 * s, true,
                            Align::Left});
        if (!d.job.empty())
            out.push_back(Words{panel::Box{L.name.x + nameW + 8 * s, L.name.y + 2 * s,
                                           panel::wordsWidth(d.job, 13 * s, false) + 4 * s, L.name.h - 2 * s},
                                d.job, kMuted, 13 * s, false, Align::Left});
        const std::string lv = levels(d);
        const float lvW = panel::wordsWidth(lv, 13 * s, true) + 4 * s;
        out.push_back(Words{panel::Box{L.levels.x + L.levels.w - lvW, L.levels.y, lvW, L.levels.h}, lv, kGold2,
                            13 * s, true, Align::Right});
        // the weight (3.0.11-beta.141: in the game's numbers), the zeny along the info line; the free points (stats',
        // skills') and the arrows running low along the points' line under it
        float x = L.info.x;
        const panel::Box* line = &L.info;
        const auto put = [&](const std::string& text, uint32_t rgb, bool bold, float gap) {
            const float w = panel::wordsWidth(text, 12.5f * s, bold) + 2 * s;
            out.push_back(Words{panel::Box{x, line->y, w, line->h}, text, rgb, 12.5f * s, bold, Align::Left});
            x += w + gap * s;
        };
        if (d.weight >= 0) {
            put(t.weight, kMuted, false, 4);
            const int over = overweightOf(d);  // 3.0.11-beta.139: by the server's state
            const std::string per = std::to_string(d.weight) + "\xC2\xA0%";
            put(d.weightMost > 0 && d.weightNow >= 0 ? pair(d.weightNow, d.weightMost) + " \xC2\xB7 " + per : per,
                over == 2 ? kHpLow : over == 1 ? kWarn : kText, true, 12);
        }
        if (d.zeny >= 0) {
            put(grouped(d.zeny), kText, true, 2);
            put("z", kMuted, false, 12);
        }
        x = L.points.x;
        line = &L.points;
        if (d.statusPoints > 0) put(counted(t.points, d.statusPoints), kGold2, false, 12);
        if (d.skillPoints > 0 && !t.skillPoints.empty()) put(counted(t.skillPoints, d.skillPoints), kGold2, false, 12);
        if (d.ammo >= 0 && d.ammo < kAmmoLow && !t.ammo.empty()) {  // 3.0.11-beta.114: the arrows running low
            put(t.ammo, kMuted, false, 4);
            put(std::to_string(d.ammo), d.ammo <= kAmmoVeryLow ? kHpLow : kWarn, true, 0);
        }
        out.push_back(Words{L.hpLabel, "HP", kMuted, 11 * s, true, Align::Left});
        out.push_back(Words{L.spLabel, "SP", kMuted, 11 * s, true, Align::Left});
        if (d.hpMost > 0) out.push_back(Words{L.hpText, pair(d.hp, d.hpMost), low ? 0xFF9A8F : kText, 13 * s, true,
                                              Align::Right});
        if (d.spMost > 0) out.push_back(Words{L.spText, pair(d.sp, d.spMost), kText, 13 * s, true, Align::Right});
        if (low) {  // in its red round
            const float r = 7 * s;
            out.push_back(Words{panel::Box{L.hpText.x - 2 * r - 2 * s, L.hpText.y, 2 * r, L.hpText.h}, "!", kText,
                                11 * s, true, Align::Center});
        }
        // 3.0.11-beta.141: the experience's rows - the word, the per cent (the last level «макс.»), the level's numbers
        // under the bar and the pace with the time to the level at the row's right (what fits of it)
        const auto experience = [&](const Exp& e, const panel::Box& label, const panel::Box& pct,
                                    const panel::Box& text) {
            if (!known(e)) return;
            out.push_back(Words{label, e.name, kMuted, 11 * s, true, Align::Left});
            out.push_back(Words{pct, e.next == 0 ? t.expMax : percent(std::max(e.share, 0.0f), t.decimal), kMuted,
                                12 * s, true, Align::Right});
            if (!numbers(e)) return;
            const float px = 11 * s;
            const std::string level = grouped(e.now) + " / " + grouped(e.next);
            const float levelW = panel::wordsWidth(level, px, false) + 4 * s;
            out.push_back(Words{panel::Box{text.x, text.y, levelW, text.h}, level, kMuted, px, false, Align::Left});
            std::string speed = pace(e, t, true);
            if (!speed.empty() && levelW + panel::wordsWidth(speed, px, false) + 12 * s > text.w)
                speed = pace(e, t, false);  // no room for the time: the pace alone
            if (speed.empty() || levelW + panel::wordsWidth(speed, px, false) + 12 * s > text.w) return;
            const float speedW = panel::wordsWidth(speed, px, false) + 4 * s;
            out.push_back(Words{panel::Box{text.x + text.w - speedW, text.y, speedW, text.h}, speed, kMuted, px, false,
                                Align::Right});
        };
        experience(baseOf(d), L.baseLabel, L.basePct, L.baseText);
        experience(jobOf(d), L.jobLabel, L.jobPct, L.jobText);
    }
    const panel::Box warn = warnBox(L, d, t, show);
    if (warn.w > 0 && !show.warningApart)
        out.push_back(Words{warn, "! " + replaced(t.lowHp, std::to_string(lowPercent(d))), kText, 13 * s, true,
                            Align::Center});
    const panel::Box tip = tipBox(L, d, t, show, warn);
    const std::vector<std::string> lines = tipLines(L, d, t, show);
    for (size_t i = 0; tip.w > 0 && i < lines.size(); ++i)
        out.push_back(Words{panel::Box{tip.x + 10 * s, tip.y + (4 + 17 * float(i)) * s, tip.w - 20 * s, 17 * s},
                            lines[i], i == 0 ? kText : kMuted, 13 * s, i == 0, Align::Left});
    return out;
}

std::vector<Words> warningWords(const Layout& L, const Data& d, const Texts& t) {
    Show show;
    show.warning = true;
    const panel::Box warn = warnBox(L, d, t, show);
    if (warn.w <= 0) return {};
    return {Words{warn, "! " + replaced(t.lowHp, std::to_string(lowPercent(d))), kText, 13 * L.s, true,
                  Align::Center}};
}

}  // namespace uagfx::charblock
