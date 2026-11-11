#include "core/weapon_look.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.18-beta.129 (item 6: the weapon on «Спорядження»'s character): a weapon's kind by its number, as
// uaRO.exe FUN_0064bc50 gives it, and the jobs' own kinds (FUN_005b1ee0)
TEST(weapon_look_kinds) {
    CHECK_EQ(weaponlook::kindOf(0), 0);
    CHECK_EQ(weaponlook::kindOf(1116), 2);    // one-handed sword
    CHECK_EQ(weaponlook::kindOf(1160), 3);    // two-handed sword
    CHECK_EQ(weaponlook::kindOf(1201), 1);    // dagger
    CHECK_EQ(weaponlook::kindOf(1250), 0x10); // katar
    CHECK_EQ(weaponlook::kindOf(1301), 6);    // axe
    CHECK_EQ(weaponlook::kindOf(1351), 7);
    CHECK_EQ(weaponlook::kindOf(1401), 4);
    CHECK_EQ(weaponlook::kindOf(1460), 5);
    CHECK_EQ(weaponlook::kindOf(1472), 10);   // two of them staves
    CHECK_EQ(weaponlook::kindOf(1501), 8);
    CHECK_EQ(weaponlook::kindOf(1550), 0xF);
    CHECK_EQ(weaponlook::kindOf(1601), 10);
    CHECK_EQ(weaponlook::kindOf(1701), 0xB);
    CHECK_EQ(weaponlook::kindOf(1801), 0xC);
    CHECK_EQ(weaponlook::kindOf(1901), 0xD);
    CHECK_EQ(weaponlook::kindOf(1951), 0xE);
    CHECK_EQ(weaponlook::kindOf(2001), 0x17);
    CHECK_EQ(weaponlook::kindOf(13001), 1);
    CHECK_EQ(weaponlook::kindOf(13101), 0x11);
    CHECK_EQ(weaponlook::kindOf(13151), 0x12);
    CHECK_EQ(weaponlook::kindOf(13301), 0x16);
    CHECK_EQ(weaponlook::kindOf(13401), 2);
    CHECK_EQ(weaponlook::kindOf(2101), -1);   // not a weapon
    CHECK_EQ(weaponlook::forJob(9, 5), 10);   // a wizard's spear drawn as a staff
    CHECK_EQ(weaponlook::forJob(6, 8), 2);    // a thief's mace drawn as a sword
    CHECK_EQ(weaponlook::forJob(0x11, 6), 1);
    CHECK_EQ(weaponlook::forJob(8, 0xC), -1); // a priest's knuckle: none
    CHECK_EQ(weaponlook::forJob(7, 2), 2);
}
