#include "core/stat_math.hpp"

#include <algorithm>
#include <iterator>

namespace uagfx::statmath {
namespace {

struct Job {
    int id, weight, hpFactor, hpMultiplicator, spFactor;
    int delays[kWeapons];
    const char* bonuses;
};
#include "core/job_info.inc"

const Job* find(int job) {
    for (const Job& j : kJobs)
        if (j.id == job) return &j;
    return nullptr;
}

}  // namespace

Class classOf(int job) {
    Class c;
    const Job* j = find(job);
    if (!j) return c;
    c.known = true;
    c.weight = j->weight;
    c.hpFactor = j->hpFactor;
    c.hpMultiplicator = j->hpMultiplicator;
    c.spFactor = j->spFactor;
    std::copy(std::begin(j->delays), std::end(j->delays), c.delays);
    c.bonuses = j->bonuses;
    return c;
}

bool upper(int job) { return job >= 4001 && job <= 4022; }

std::vector<int> jobs() {
    std::vector<int> out;
    for (const Job& j : kJobs) out.push_back(j.id);
    return out;
}

std::vector<int> bonusLevels(int job, int stat) {
    std::vector<int> out;
    const Job* j = find(job);
    if (!j || stat < 0 || stat >= kStats) return out;
    for (int i = 0; j->bonuses[i]; ++i)
        if (j->bonuses[i] - '1' == stat) out.push_back(i + 1);
    return out;
}
bool baby(int job) { return job >= 4023 && job <= 4045; }

int pointCost(int value) { return (std::max(value, 1) - 1) / 10 + 2; }

int pointsBetween(int from, int to) {
    int sum = 0;
    for (int v = from; v < to; ++v) sum += pointCost(v);
    return sum;
}

int pointsAt(int baseLevel) {
    const int n = int(std::size(kStatPoints));
    return baseLevel >= 1 && baseLevel <= n ? kStatPoints[baseLevel - 1] : 0;
}

int jobBonus(int job, int jobLevel, int stat) {
    const Job* j = find(job);
    if (!j || stat < 0 || stat >= kStats) return 0;
    int n = 0;
    for (int i = 0; i < jobLevel && j->bonuses[i]; ++i)
        if (j->bonuses[i] - '1' == stat) ++n;
    return n;
}

bool ranged(int weapon) { return weapon == 11 || weapon == 13 || weapon == 14 || (weapon >= 17 && weapon <= 21); }

int atk(const int s[kStats], int weapon) {
    const int main = ranged(weapon) ? s[Dex] : s[Str], other = ranged(weapon) ? s[Str] : s[Dex];
    return main + (main / 10) * (main / 10) + other / 5 + s[Luk] / 5;
}

int matkMin(int i) { return i + (i / 7) * (i / 7); }
int matkMax(int i) { return i + (i / 5) * (i / 5); }
int hit(int baseLevel, int dex) { return baseLevel + dex; }
int flee(int baseLevel, int agi) { return baseLevel + agi; }
int perfectDodge10(int luk) { return 10 + luk; }
int crit10(int luk) { return 10 + luk * 10 / 3; }
int softMdef(int intel, int vit) { return intel + vit / 2; }
int vitDefLeast(int vit) { return vit / 2; }
int vitDefMost(int vit) { return vit / 2 + std::max(0, vit * (vit - 15) / 150 - 1); }

int amotion(int job, int weapon, int agi, int dex) {
    const Job* j = find(job);
    if (!j || weapon < 0 || weapon >= kWeapons || j->delays[weapon] >= 2000) return 0;
    const int base = j->delays[weapon];
    return std::max((200 - kMaxAspd) * 10, base - base * (4 * agi + dex) / 1000);
}

int aspd10(int amotionValue) { return 2000 - amotionValue; }

int amotionRated(int job, int weapon, int agi, int dex, int rate) {
    const Job* j = find(job);
    if (!j || weapon < 0 || weapon >= kWeapons || j->delays[weapon] >= 2000) return 0;
    const int base = j->delays[weapon];
    return std::max((200 - kMaxAspd) * 10, (base - base * (4 * agi + dex) / 1000) * rate / 1000);
}

// a hit each 2 amotion ms
double hitsPerSecond(int amotionValue) { return amotionValue > 0 ? 500.0 / amotionValue : 0.0; }

int maxHp(int job, int baseLevel, int vit) {
    const Job* j = find(job);
    if (!j || baseLevel < 1) return 0;
    // 3.0.11-beta.265 (R-VZ69Q2: an archer of level 38 has 610 by the server, 611 with the 35 counted in): the 35
    // added after the VIT's per cents
    int val = 0;
    for (int i = 1; i <= baseLevel; ++i) val += (i * j->hpFactor + 50) / 100;
    val += baseLevel * j->hpMultiplicator / 100;
    if (job == 24 || job == 25) val += 100;  // the Gunslinger's and the Ninja's, as the server's
    if (upper(job)) val += val * 25 / 100;
    else if (baby(job)) val -= val * 30 / 100;
    return val + val * vit / 100 + 35;
}

int maxSp(int job, int baseLevel, int intel) {
    const Job* j = find(job);
    if (!j || baseLevel < 1) return 0;
    int val = 10 + baseLevel * j->spFactor / 100;
    val += val * intel / 100;
    if (upper(job)) val += val * 25 / 100;
    else if (baby(job)) val -= val * 30 / 100;
    return val;
}

int hpRegen(int vit, int maxHpValue) { return 1 + vit / 5 + maxHpValue / 200; }

int spRegen(int intel, int maxSpValue) {
    int val = 1 + intel / 6 + maxSpValue / 100;
    if (intel >= 120) val += (intel - 120) / 2 + 4;
    return val;
}

int weightMost(int job, int baseStr) {  // .265 (R-VZ69Q2: 2630, not 2660): the base STR
    const Job* j = find(job);
    return j ? (j->weight + baseStr * 300) / 10 : 0;
}

int castPercent(int dex) { return std::clamp((kCastDex - dex) * 100 / kCastDex, 0, 100); }
int delayPercent(int dex) { return castPercent(dex); }

}  // namespace uagfx::statmath
