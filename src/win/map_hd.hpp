#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/minimap_proto.hpp"

namespace uagfx::win::maphd {

// 3.0.11-beta.72 (the owner: «сами картинки карт маленькие и не аккуратные»): a map's picture four times bigger
// than the game's 512 x 512, when the player's game has one - looked for on a thread of its own (the pack's central
// directory, a 2048 x 2048 WebP): the AI texture pack's picture of the game's own (by its keys, core/pack_source - the
// builder makes the interface's and the texture's), else a DATA.INI zip's "texture\유저인터페이스\map\<map>.webp" (the
// old client's texture_x4.zip). The log tells where it came from, or why none.

// The game's thread: `bmp` the game's own picture's file (the pack's keys; empty: by the name alone). The last asked
// is made; the one before, if not begun, never.
void ask(const std::string& map, std::vector<uint8_t> bmp);
// The game's thread: the picture made for `map`, handed once (nullptr: not yet, or there is none)
std::shared_ptr<const minimap::MapPicture> take(const std::string& map);

}  // namespace uagfx::win::maphd
