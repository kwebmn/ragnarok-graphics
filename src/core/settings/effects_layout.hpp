#pragma once

namespace uagfx::settings {

// The Effects page's layout (0.18; 1.9.3 shared with the game's console, win/console.cpp): each effect its box, the
// boxes under it (0.23.2) and its sliders - a slider may need one of those boxes on too - in the page's order, in
// blocks: 0 the picture's effects, 1 the map's objects (0.23.2), 2 the living world (1.7), 3 the remaster (1.8).
// 2.4: up to four boxes under one - the parts of what an effect draws, each its own (the water's foam, glints...).
constexpr int kLayoutChecks = 7;  // 3.0.15-beta.11: 6 (the mages' effects), 4 before; 3.0.16-beta.27 7 (mage_soft)
struct EffectLayout {
    const char* box;
    const char* checks[kLayoutChecks];
    const char* sliders[5];
    const char* needs[5];  // the box under it each slider needs on, or none
    int block;
};
const EffectLayout* effectLayout();
int effectLayoutCount();
// 3.0.18-beta.183 (the owner: «лучше одним пунктом чтобы не усложнять интерфейс»): the classes' and the mages'
// elements' own boxes, the soft look's and Storm Gust's «як у грі» - [effects] skill_looks in their place; still read
// from the file (a set's or a player's own), no row of the page's, the console's or the search's
bool hiddenEffect(const char* sectionKey);

}  // namespace uagfx::settings
