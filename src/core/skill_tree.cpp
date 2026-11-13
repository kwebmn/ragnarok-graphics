#include "core/skill_tree.hpp"

#include <algorithm>

namespace uagfx::skilltree {
namespace {

#include "core/skill_tree.inc"

const Entry* find(int job, int id) {
    for (const Entry& e : kTree)
        if (e.job == job && e.id == id) return &e;
    return nullptr;
}

// Each job and the one before it (the trees of the classic, transcendent and baby jobs, Taekwon's)
int parentOf(int job) {
    switch (job) {
        case 1: case 2: case 3: case 4: case 5: case 6: case 22: case 23: case 24: case 25: case 4046: return 0;
        case 7: case 13: case 14: case 21: return 1;
        case 9: case 16: return 2;
        case 11: case 19: case 20: return 3;
        case 8: case 15: return 4;
        case 10: case 18: return 5;
        case 12: case 17: return 6;
        case 4002: case 4003: case 4004: case 4005: case 4006: case 4007: return 4001;
        case 4008: return 7;
        case 4009: return 8;
        case 4010: return 9;
        case 4011: return 10;
        case 4012: return 11;
        case 4013: return 12;
        case 4014: return 13;
        case 4015: return 14;
        case 4016: return 15;
        case 4017: return 16;
        case 4018: return 17;
        case 4019: return 18;
        case 4020: return 19;
        case 4021: return 20;
        case 4022: return 21;
        case 4024: case 4025: case 4026: case 4027: case 4028: case 4029: case 4045: return 4023;
        case 4030: case 4036: case 4037: case 4044: return 4024;
        case 4032: case 4039: return 4025;
        case 4034: case 4042: case 4043: return 4026;
        case 4031: case 4038: return 4027;
        case 4033: case 4041: return 4028;
        case 4035: case 4040: return 4029;
        case 4047: case 4048: case 4049: return 4046;
        default: return -1;
    }
}

}  // namespace

int maxLevel(int job, int id) {
    const Entry* e = find(job, id);
    return e ? e->most : 0;
}

std::vector<int> chain(int job) {
    std::vector<int> out;
    for (int j = job; j >= 0 && out.size() < 8; j = parentOf(j)) out.insert(out.begin(), j);
    return out;
}

int homeJob(const std::vector<int>& jobs, int id) {
    for (int j : jobs)
        if (find(j, id)) return j;
    return -1;
}

int order(int job, int id) {
    int n = 0;
    for (const Entry& e : kTree) {
        if (e.job != job) continue;
        if (e.id == id) return n;
        ++n;
    }
    return 1 << 20;
}

std::vector<std::pair<int, int>> needs(int job, int id) {
    std::vector<std::pair<int, int>> out;
    if (const Entry* e = find(job, id))
        for (const auto& n : e->need)
            if (n[0] > 0 && n[1] > 0) out.emplace_back(n[0], n[1]);
    return out;
}

std::vector<int> skills(int job) {
    std::vector<int> out;
    for (const Entry& e : kTree)
        if (e.job == job) out.push_back(e.id);
    return out;
}

std::vector<int> nextJobs(int job, bool male) {
    switch (job) {
        case 1: return {7, 14};  // Swordman: Knight, Crusader
        case 2: return {9, 16};  // Mage: Wizard, Sage
        case 3: return {11, male ? 19 : 20};  // Archer: Hunter, Bard or Dancer
        case 4: return {8, 15};   // Acolyte: Priest, Monk
        case 5: return {10, 18};  // Merchant: Blacksmith, Alchemist
        case 6: return {12, 17};  // Thief: Assassin, Rogue
        case 7: case 13: return {4008};
        case 8: return {4009};
        case 9: return {4010};
        case 10: return {4011};
        case 11: return {4012};
        case 12: return {4013};
        case 14: case 21: return {4015};
        case 15: return {4016};
        case 16: return {4017};
        case 17: return {4018};
        case 18: return {4019};
        case 19: return {4020};
        case 20: return {4021};
        case 4002: return {4008, 4015};
        case 4003: return {4010, 4017};
        case 4004: return {4012, male ? 4020 : 4021};
        case 4005: return {4009, 4016};
        case 4006: return {4011, 4019};
        case 4007: return {4013, 4018};
        case 4024: return {4030, 4037};
        case 4025: return {4032, 4039};
        case 4026: return {4034, male ? 4042 : 4043};
        case 4027: return {4031, 4038};
        case 4028: return {4033, 4041};
        case 4029: return {4035, 4040};
        case 4046: return {4047, 4049};  // Taekwon: Star Gladiator, Soul Linker
        default: return {};
    }
}

std::vector<int> jobs() {
    std::vector<int> out;
    for (const Entry& e : kTree)
        if (std::find(out.begin(), out.end(), int(e.job)) == out.end()) out.push_back(int(e.job));
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace uagfx::skilltree
