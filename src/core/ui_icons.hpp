#pragma once
#include <cstdint>

#include "core/window_panel.hpp"

// 3.0.11-beta.26: the interface concept's line icons (its style A mock-ups' own drawings, tools/ui_icons.py) - drawn
// as lines at any size, sharp at every interface scale.
namespace uagfx::uiicons {

enum class Icon : uint8_t {
    Search, Monster, Item, Location, Quest, Npc, Mvp, Pet, Craft, World, Tables, Back, External, Route,
    Dungeon, Minus, Plus, Expand, Warn,  // 3.0.11-beta.29: the world map's
    Home,                                // 3.0.11-beta.30: the first screen
    Stats, Equip, Bag, Skill, Friends, Gear,  // 3.0.11-beta.36: the character block's buttons
    Edit, Done,                               // 3.0.11-beta.40: the hotbar's
    kCount
};

// The icon over a picture (inside its clip): its 24 x 24 drawing `size` pixels wide with its top left at (x, y), its
// lines `stroke` of the 24 wide (the concept's 2; 2.2 the search's, 2.4 «back»), `rgb` at `a`
void draw(panel::Picture& p, Icon icon, float x, float y, float size, uint32_t rgb, float a = 1.0f,
          float stroke = 2.0f);

}  // namespace uagfx::uiicons
