#pragma once

namespace uagfx::win {

// 3.0.4-beta.3 (the user's ideas 2, 4, 8): the zone server's packets as uaRO.exe takes them - its game mode's packet
// loop (FUN_00579900) reads each packet's id at 0x579b02 (`call 0x419350`, the buffer just filled); that call goes
// through us first, both checked by their bytes. Each packet parsed (core/game_packets.hpp) and given to the
// notifications. Independent of uaRO Pad's own reading. Put in at the DLL's load.
void installPacketTap();
bool packetTapOn();  // put in (this uaRO.exe's loop)

}  // namespace uagfx::win
