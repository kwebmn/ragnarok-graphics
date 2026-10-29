#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "core/reference_index.hpp"
#include "core/reference_live.hpp"
#include "core/world_map.hpp"

namespace uagfx::win::reference {

// 3.0.11-beta.6: the game's reference pack there (uaro-graphics\reference - the game's watcher keeps it fresh,
// app/reference_pack) read on a thread of its own at the first ask, and again when its pack.json changes (looked at
// every 30 s at most). Null until read, and when there is none.
std::shared_ptr<const refindex::Index> index();
// 3.0.11-beta.10: the players on each map now (uaro-graphics\reference\online.json, the watcher's every 2 minutes),
// read again when it changes (looked at every 30 s); null none or not fresh (core/reference_live)
std::shared_ptr<const reflive::Online> online();
// 3.0.11-beta.12: the players' public MVP timers and the site's market (uaro-graphics\reference\live.json) - as
// online()
std::shared_ptr<const reflive::Live> live();
// 3.0.11-beta.11: the world map - the site's sheet (uaro-graphics\reference\worldmap.json) with the client's own
// minimaps put on it, made on a thread of its own at the first ask (and again when the sheet changes); null until then
struct World {
    worldmap::Sheet sheet;
    std::vector<uint32_t> argb;  // sheet.w x sheet.h
};
std::shared_ptr<const World> world();

}  // namespace uagfx::win::reference
