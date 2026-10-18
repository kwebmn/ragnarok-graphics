#pragma once
#include <string>

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

}  // namespace uagfx::win
