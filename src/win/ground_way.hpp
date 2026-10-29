#pragma once
#include <string>
#include <utility>
#include <vector>

#include "core/frame.hpp"

namespace uagfx::win::groundway {

// 3.0.11-beta.31 (the owner: «на самой локации нету линии по которой идти надо, только на карте»): the way followed
// on the new minimap drawn on the ground itself (core/ground_way) - in gold over a dark edge, against the depth with
// the world (the characters and the walls before it hide it), before the interface. On the game's thread.

// The way's cells on the map the player is on now (the minimap's, made again as the player walks); none: nothing
void set(const std::string& map, const std::vector<std::pair<int, int>>& cells);
// A frame begins (BeginScene): the way drawn once in it
void newFrame();
// Before the frame's first interface draw (Device7::record), or at its present when it had none: the strip with the
// frame's camera into the record; the game's picture `width` x `height`
void draw(FrameRecord& record, int width, int height);

}  // namespace uagfx::win::groundway
