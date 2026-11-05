#pragma once

namespace uagfx::skilldelay {

// 3.0.11-beta.276 (the owner's feature 7): the delay after a skill - its AfterCastActDelay by its level from the
// server's db/skill_cast_db.txt (core/skill_delay.inc, tools/skill_delay.py), cut by DEX as uaRO's server does it
// (delay_dependon_dex: × (150 − DEX) / 150; none at 150)
int afterCast(int skill, int level);   // ms, 0 none
int withDex(int ms, int dex);          // the server's cut
// A share 0-1 of the delay left at `now` for one that began at `from` and lasts `ms`; 0 past it
float left(unsigned long long from, int ms, unsigned long long now);

}  // namespace uagfx::skilldelay
