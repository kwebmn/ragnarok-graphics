#pragma once
#include <string>

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
// 2.3, uaRO Studio's pick: the game's own cursor in its pixels; false in another build of the exe.
bool readGameCursor(int& x, int& y);
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
