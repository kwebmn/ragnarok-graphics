#pragma once
#include <string>

#include "core/reference_quests.hpp"

namespace uagfx::refwalk {

// 3.0.18-beta.246 (the reference's stage 8: the site builds the maps' walkable parts with its data, the client keeps
// up without a release): /api/public/game/walkable.json = the quests' shape {v, sha256, size, count (maps), url}, the
// file walkable-<sha12>.bin = tools/walk_grids.py's (core/walk_grid reads it). The settings app's watcher keeps them in
// uaro-graphics\reference (walkable.json and the file); the driver reads that file before the package's own
// uaro-graphics\walkable.bin. No Windows here.
using Manifest = refquests::Manifest;
std::string fileName(const Manifest& m);  // "walkable-<sha12>.bin"
std::string sitePath(const Manifest& m);  // "/api/public/game/walkable-<sha12>.bin" - never the answer's url

}  // namespace uagfx::refwalk
