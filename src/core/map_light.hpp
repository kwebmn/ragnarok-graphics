#pragma once
#include <algorithm>

namespace uagfx::map_light {

// 2.7.32 (the tester in Prontera's sewers: «стены от огня очень гладкие» - the game's own, as Pause showed): the maps'
// ground and walls are their texture shaded (`base`: the lightmap's alpha times the texture) plus the map's coloured
// light (`light`, a channel of its rgb), added and cut at white (Direct3D 7's MODULATEALPHA_ADDCOLOR) - by a torch
// every shade of a wall came out one yellow. A strong light now goes on as onto a screen, 1 - (1 - base) (1 - light),
// a weak one (up to 0.3: the maps' tint all over) added as the game adds it; between them by q, steep enough never to
// dim a brighter light's patch for the textures up to 0.7. shaders.cpp's softMapLight works it out alike.
inline float soft(float base, float light) {
    const float q = std::min(1.43f * std::max(light - 0.3f, 0.0f), light);
    return std::clamp(base + light - base * q, 0.0f, 1.0f);
}

}  // namespace uagfx::map_light
