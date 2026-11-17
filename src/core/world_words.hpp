#pragma once
#include <cstdint>

// 3.0.20-beta.85 (R-H3S9MY «ник персонажа который ходит гдето не рядом с ним а на верху, также непонятно с мобами
// бывает такое»): uaRO.exe places an actor's words over its head (the chat, a skill's shout: its window at the actor's
// + 0x214, FUN_0054ac00 each frame) and the hovered one's name (the game mode's + 0x2DC, FUN_00567690) from the
// actor's point on the screen and its scale (its + 0x98, + 0x9C, + 0x48) - which only the actor's own render writes.
// In the 3D view the camera turns with the mouse and an actor the game does not draw that frame (behind the eye, past
// its culling) keeps the point of an older frame: its words hung anywhere on the screen. Their places as the game
// works them out, from the actor's feet as the 3D view's camera puts them on the screen now (win/windows_draw).
namespace uagfx::worldwords {

struct Screen {
    int width = 0, height = 0;  // the game's picture (the renderer's + 0x24, + 0x28)
};
struct Point {
    int x = 0, y = 0;
};
// FUN_0054ac00: a window w x h of words over the head of an actor whose feet are at (fx, fy) with `scale`
Point wordsAt(float fx, float fy, float scale, int w, int h, Screen s);
// FUN_00567690: the hovered one's name by the game mode's + 0x40C (0 under the feet, 1 to the left, 2 to the right,
// 3 over the head; another as 0)
Point nameAt(int mode, float fx, float fy, float scale, int w, int h, Screen s);
// The game's scale of an actor (FUN_00412d60: rhw * f0 * k, over the norm SetSize keeps at 0x6D5EFC); 0 for no norm
float scaleOf(float rhw, float f0, float k, float norm);

}  // namespace uagfx::worldwords
