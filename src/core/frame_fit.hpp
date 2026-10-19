#pragma once

namespace uagfx::frame_fit {

// 2.5.25: frame generation shows several frames for each of the game's; more than the monitor shows are dropped
// unevenly (R-Q7RBYV: 156 a second on a 144 Hz monitor) and only cost the card time its game frames need. The most
// game frames a second that keep every shown one under the refresh by Reflex's margin (refresh - refresh^2 / 3600:
// 144 Hz -> 138); 0 - no limit: no generated frames, or the refresh unknown (Windows' 0 and 1 mean its default).
int gameFps(int refreshHz, int shownPerGameFrame);
// The frame pacer's limit: the player's [frames] fps_limit (0: none) or that, whichever is lower.
int pacedFps(int fpsLimit, int refreshHz, int shownPerGameFrame);

}  // namespace uagfx::frame_fit
