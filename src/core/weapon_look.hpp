#pragma once

namespace uagfx::weaponlook {

// 3.0.18-beta.129 (item 6: the weapon on «Спорядження»'s character, the owner's own - the game's equipment
// window draws none): a weapon's kind by its item number as uaRO.exe FUN_0064bc50 gives it (0 none, -1 not a weapon) -
// its sprite «인간족\<the job's folder><sex>_<kind's name>» when the weapon has no sprite of its own
int kindOf(int item);
// The kinds some jobs draw otherwise (FUN_005b1ee0): a mage's spear as a staff, a mace or an axe as a sword, a
// priest's knuckle none (-1)
int forJob(int job, int kind);

}  // namespace uagfx::weaponlook
