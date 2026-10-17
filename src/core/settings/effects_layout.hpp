#pragma once

namespace uagfx::settings {

// The Effects page's layout (0.18; 1.9.3 shared with the game's console, win/console.cpp): each effect its box, the
// boxes under it (0.23.2) and its sliders - a slider may need one of those boxes on too - in the page's order, in
// blocks: 0 the picture's effects, 1 the map's objects (0.23.2), 2 the living world (1.7), 3 the remaster (1.8).
struct EffectLayout {
    const char* box;
    const char* checks[2];
    const char* sliders[5];
    const char* needs[5];  // the box under it each slider needs on, or none
    int block;
};
const EffectLayout* effectLayout();
int effectLayoutCount();

}  // namespace uagfx::settings
