#include "core/weapon_look.hpp"
#include <initializer_list>

namespace uagfx::weaponlook {

int kindOf(int i) {
    if (i == 0) return 0;
    if (i >= 1100 && i < 1150) return 2;
    if (i >= 1150 && i < 1200) return 3;
    if (i >= 1200 && i < 1250) return 1;
    if (i >= 1250 && i < 1300) return 0x10;
    if (i >= 1300 && i < 1350) return 6;
    if (i >= 1350 && i < 1400) return 7;
    if (i >= 1400 && i < 1450) return 4;
    if (i >= 1450 && i < 1500) return i == 0x5C0 || i == 0x5C1 ? 10 : 5;
    if (i >= 1500 && i < 1550) return 8;
    if (i >= 1550 && i < 1600) return 0xF;
    if (i >= 1600 && i < 1700) return 10;
    if (i >= 1700 && i < 1750) return 0xB;
    if (i >= 1800 && i < 1900) return 0xC;
    if (i >= 1900 && i < 1950) return 0xD;
    if (i >= 1950 && i < 2000) return 0xE;
    if (i >= 2000 && i < 2100) return 0x17;
    if (i >= 13000 && i < 13100) return 1;
    if (i >= 13100 && i < 13150) return 0x11;
    if (i >= 13150 && i < 13200) return 0x12;
    if (i >= 13300 && i < 13400) return 0x16;
    if (i >= 13400 && i < 13500) return 2;
    return -1;
}

int forJob(int job, int k) {
    const auto in = [&](std::initializer_list<int> jobs) {
        for (const int j : jobs)
            if (j == job) return true;
        return false;
    };
    if (in({8, 0xFA9, 0xFBF}) && k == 0xC) return -1;
    if (in({6, 0xFA7, 0xFBD}) && (k == 8 || k == 6 || k == 7)) return 2;
    if (in({0xC, 0x11, 0xFAD, 0xFB2, 0xFC3, 0xFC8}) && (k == 8 || k == 7)) return 2;
    if (in({0x11, 0xFB2, 0xFC8}) && k == 6) return 1;
    if (in({9, 0x10, 2, 0xFAA, 0xFB1, 0xFA3, 0xFC0, 0xFC7, 0xFB9}) && k == 5) return 10;
    return k;
}

}  // namespace uagfx::weaponlook
