#pragma once
#include <windows.h>

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "core/char_status.hpp"

namespace uagfx::win {

// Part 3 (DLSS): the view matrix of the frame being drawn, read from uaRO.exe's memory (core/game_camera.hpp). False
// when the exe is not the build those addresses belong to, when the game shows no world (the login, the character
// list) or when the matrix is no camera's; reading never crashes the game.
bool readGameCamera(float view[12]);
// 1.4.2: the view-space depth of the point the camera looks at - the character; false outside the game.
bool readCameraTargetDepth(float& z);

// AI textures (0.12): the map the game shows ("prontera"), from the same game mode as the camera; "" when there is no
// world (the login, the character list) or the exe is another build.
std::string readGameMap();
// 3.0.11-beta.1 (the new interface's minimap prototype): the player's cell by the game's own minimap formula
// (core/minimap_proto cellOf) and the map's cells; false outside a world or in another build
bool readPlayerCell(int& x, int& y, int& width, int& height);
// 3.0.11-beta.31: the player's place in the game's world (x, its height y - up along -y -, z) and the map's cells
// (their count across and down, a cell's world size)
bool readPlayerWorld(float at[3], int& width, int& height, int& cellSize);
// 3.0.11-beta.71 (the owner: «туда куда чар смотрит»): which way the character faces - its object's + 0x38, degrees
// (FUN_00544da0 turns it toward a point, the packet 0x9c sets it; minimap::facingOf); false: no character
bool readPlayerFacing(float& degrees);
// 3.0.11-beta.87 (the concept «Контроллер в интерфейсе uaRO», stage 3): an actor's place in the game's world by its id
// (the world's actors' list + 0x18, an actor's id + 0x208, its place + 4; the character itself by its own); false: not
// in sight, another build
bool readActorWorld(uint32_t id, float at[3]);
// Its sprite's box on the screen the game keeps to pick actors with the mouse (0x74B5F0 + 0x28, FUN_004f9890: a
// record's left, top +0, +4, right, bottom +0xC, +0x10, its id +0x18) - left, top, right, bottom in the game's pixels
bool readActorBox(uint32_t id, float box[4]);
// 3.0.11-beta.95: every actor's box there at once (the monsters' HP bars) - its id to left, top, right, bottom;
// 3.0.11-beta.100: `classes` - each one's class too (the record's +0x1C, FUN_0054aee0's: 1001-3999 a monster)
void readActorBoxes(std::unordered_map<uint32_t, std::array<float, 4>>& out,
                    std::unordered_map<uint32_t, int>* classes = nullptr);
// The game's own minimap: it is drawn while its UI window 14 exists (UIWindowMgr+0x7C); hidden and made again as
// Ctrl+Tab does (UI command 0x99: DeleteWindow / MakeWindow of window 14, the latter only where the map has one) -
// false when not done (another build, nothing to do). On the game's thread.
bool gameMinimapShown();
bool hideGameMinimap();
bool showGameMinimap();
// 2.3, uaRO Studio's pick: the game's own cursor in its pixels; false in another build of the exe.
bool readGameCursor(int& x, int& y);
// 3.0.11-beta.43 (the owner on «Речі»: «крестик не закрывает ... не открывает карточку»): a mouse message's point in
// the target as the parts take their clicks - where the game's own cursor stands (what the player sees, and what the
// parts' hover follows), else the message's own (the wheel's from the screen); `fromGame` which one; false none
bool pointerInTarget(HWND game, UINT m, LPARAM lp, double k, int targetWidth, int targetHeight, float& tx, float& ty,
                     bool* fromGame = nullptr);
// 3.0.2-beta.12: uaRO.exe is the build these addresses (and the profile's steps, core/sample_profile.hpp) belong to
bool knownGameBuild();
// 3.0.4-beta.5 (the user's idea 5): a building map's camera held as free as outdoors - the game mode's outdoor flag
// (core/game_camera.hpp kModeOutdoor) kept at 1 while `wanted`; set back to the game's 0 when not wanted any more on
// the same map. True while it holds a building map's.
bool keepCameraFree(bool wanted);
// 3.0.4-beta.17 (the user chose «Ограничить отдаление», R-35N5QN): the game's outdoors zoom limit (camera::kZoomMost,
// 2040) held at camera::kZoomLimit while `wanted` - past its far plane, 1500, the game draws neither the character nor
// the monsters -, its own put back when not; a camera already further out drawn in at once
void limitZoom(bool wanted);
// 3.0.4-beta.20 (R-UBT67Q «отчет с водой у горизонта»): the game's quadtree boxes raised to its water's top while
// `wanted` (the own ground on; core/game_camera.hpp raiseToWater) - the game then draws its water up to its far plane,
// the own far water on from there; its own boxes back when not wanted any more on the same map
void raiseWaterBoxes(bool wanted);
// 3.0.4-beta.6: the game shows its world (not the login or the character list); false in another build of the exe
bool inGameWorld();
// 3.0.11-beta.35: the game mode's object while it shows its world; 0 at the login and the character list, or in
// another build of the exe
uint32_t readGameMode();
// 3.0.5-beta.6 (the graphics test, core/graphics_test.hpp): the camera's turn and distance - the targets the game eases
// toward; false outside its world or in another build of the exe
bool readCameraPose(float& yaw, float& distance);
// written as the targets, and with `exact` as the shown ones too (no easing: the same path on every computer)
bool writeCameraPose(float yaw, float distance, bool exact);
// the map's camera: outdoors or a building (as the game holds it now), its own limits (core::graphics_test zoomRange)
bool readCameraLimits(bool& outdoor, bool& ownLimits, int& limitFrom, int& limitSpan);
// The game's memory read without a fault where nothing is mapped
bool readGame(uint32_t address, void* out, size_t size);
// 3.0.3-beta.2: the name of the character in the game (core/char_name), as the game spells it; "" at the login and the
// character select, or in another build of the exe (GetCharName's bytes are checked once)
std::string readOwnName();
// 3.0.3-beta.5: that character's levels, experience, HP, SP and weight (core/char_status); false at the login and the
// character select, or in another build of the exe (the instructions that put them there are checked once)
bool readOwnStatus(charstatus::Status& s);

}  // namespace uagfx::win
