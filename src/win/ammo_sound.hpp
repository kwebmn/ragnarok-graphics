#pragma once

namespace uagfx::win::ammosound {

// 3.0.11-beta.129 (the owner «делай» to «короткий сигнал, когда остаётся 10 стрел и когда они кончились»): [interface]
// ammo_sound - the ammo worn (win/ammo_watch) watched each frame; falling to 10 or fewer, a soft two-note chime, to
// none a lower three-note one (core/ui_chime), through the game's own sound player (win/sound_hook playOwn: its
// volume, none while its sound is off); a map's loading tells nothing again. On the game's thread.
void frame(bool on);

}  // namespace uagfx::win::ammosound
