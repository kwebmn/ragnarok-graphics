#pragma once
#include <windows.h>

#include <string>

#include "core/config.hpp"
#include "core/view3d.hpp"

namespace uagfx::win::view3dwin {

// 3.0.19-beta.6 (the owner: «давай попробуем, сделай побольше всего ... чтобы сразу получить качественную картинку»;
// core/view3d, plan docs/superpowers/plans/2026-11-14-view3d.md): the 3D view. Ctrl+Shift+V: the game's own camera ->
// behind the character -> from its eyes -> back. The game's camera build (uaRO.exe 0x4faf00) sent here: the game's yaw
// ours, its build, then the eye and the view matrix (+0x94) and its inverse (+0xc4) ours, the renderer's projection
// (0x6EE3D8: f0, f1, the culling's slopes) for the wider field of view - the game draws its world, its sprites and its
// clicks through it. The mouse turns the view: its move from the screen's middle, the game's cursor held there (Alt
// held: a free cursor for the windows); W A S D walk (the walk mode on while in it). On the game's thread.

void install();                          // the camera's hook, once (the game's world not needed yet)
bool key(UINT m, WPARAM wp, LPARAM lp);  // Ctrl+Shift+V; true: ours (the game sees neither the key nor its letter)
// each frame: the cursor held in the window while looking, the log; 3.0.19-beta.10: [view3d]'s fields of view and
// mouse speed taken (for the camera's next build)
void frame(const Config& config);
// 3.0.21-beta.12 (the owner: «думаю также пора кнопку в интерфейс с 3д режимами вставить»): the views by the
// interface's buttons (the character block's second row, win/own_windows) - whether the 3D view works with this
// build of uaRO.exe (the camera's hook in), the view asked for now (the game's own, behind, first person), and a
// view asked for as Ctrl+Shift+V asks (taken at the camera's next build); false: not here or not in the world
bool available();
view3d::Mode wanted();
bool ask(view3d::Mode m, const char* why);
// 3.0.22-beta.30 (the owner's video session: the camera reset after Pause): Pause on - the 3D view held as it is (the
// game's own camera meanwhile), Pause off - back the same; false: nothing changed
bool hold(bool on, const char* why);
// 3.0.22-beta.32 (the client's MCP «view», «state»): the 3D view's look (degrees: the game's yaw 0-360, the pitch up
// positive) - read, or put at once (NaN keeps that one; the pitch within view3d::kPitchMost); false: the view is
// the game's own
bool look(float& yaw, float& pitch);
bool setLook(float yaw, float pitch);
// 3.0.22-beta.33 (the MCP's «camera», «flight»): the free camera on or off (at the next build); a move of it by
// cells along its look (forward with the pitch, right, up; within its reach, out of walls); the flight's point,
// play, stop, clear (Ctrl+Shift+P, O, L's way); its points, whether it flies, its seconds. false: no 3D view, or
// no free camera for a move or a point
bool freeOn(bool on);
bool freeMove(float forward, float right, float up);
bool flight(const std::string& action);
void flightNow(int& points, bool& flying, float& seconds);
bool active();                           // the 3D view on
// 3.0.21-beta.12 (R-EV2ABB): the step the game drew its over-everything sprites with this frame (core view3d::overAll)
float onTopStep();
// 3.0.21-beta.13 (core/key_hints): from behind; the free camera on; the skill the game waits a target for (2 an
// enemy's, 4 a friend's, 0 none - off the 3D view 0); the skills cast by a click on the crosshair so far
bool behind();
bool freeCamera();
// 3.0.22-beta.24 (the cinema camera): the flight's points set (Ctrl+Shift+P), its flight on (Ctrl+Shift+O); a note for
// the screen once (settings::Text and its {1}): a point set, the points cleared, two points needed, nine at most
int flightPoints();
bool flying();
bool takeNote(int& text, int& arg);
int skillWait();
uint32_t clickCasts();
bool looking();                          // and the mouse turns it (not Alt): the crosshair in the cursor's place
bool fromTheEyes();                      // 3.0.19-beta.13: the view from the character's eyes
bool ownHidden();                        // 3.0.19-beta.15: the character's own picture left out (eyes, or right behind)
// 3.0.19-beta.31 (the testers: «на себе неможливо накласти бафи, аля хіл ітд, і неможливо скастувати на моба скіл
// який вимагає кліка по мобу»): a left press while looking and the game waiting for a skill's target - the skill cast
// on the crosshair's target or the character itself (core view3d::skillTargetOf), the wait ended; true: the press
// is not the game's any more
bool castWaiting(bool shift);
// 3.0.19-beta.36: the 3D view's right and forward laid flat along the ground (the world's x, z), from the view it
// wrote last; false off the 3D view
bool viewAxes(float right[2], float forward[2]);
// 3.0.19-beta.38 (R-S3NQBF: the monsters' HP bars over the hills hiding them): whether the ground lets the 3D view's
// eye see an actor's place (core view3d::groundLetsSee by the game's own heights); 3.0.19-beta.39 and the map's models
// (a house, a castle's wall, a tree: view3d::boxHides), their answer kept a moment for each actor; true off the view
bool seenActor(uint32_t id, const float to[3]);
// 3.0.19-beta.42 (core/ground_thin): a world point's depth in the view the 3D view wrote last (false off it), and the
// camera's builds so far (the frames' turns)
bool viewDepthOf(const float at[3], float& depth);
uint32_t builds();
// 3.0.20-beta.3: the game's view object of its last camera build (its +0x94 the view matrix), 0 before one
uint32_t lastView();

}  // namespace uagfx::win::view3dwin
