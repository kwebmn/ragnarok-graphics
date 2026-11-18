#pragma once

#include "core/config.hpp"

// 3.0.22-beta.5 (idea 4, core/laptop_set): a laptop's frames measured on the maps - with the set «Для ноутбука» and
// without it - plugged in, the game in front, no limit under 60; uaro-graphics\cache\laptop-frames.txt each minute
// for the settings app's «Гра». The game's thread.
namespace uagfx::win::laptopframes {

// Each presented frame: a laptop (a battery there), the game in front with a map, the settings (the set on is looked
// at once a second), the frames limit now (0: none; the light modes' too)
void frame(bool laptop, bool countable, const Config& config, int fpsLimit);

}  // namespace uagfx::win::laptopframes
