#pragma once
#include <memory>

#include "core/reference_index.hpp"
#include "core/reference_live.hpp"

namespace uagfx::win::reference {

// 3.0.11-beta.6: the game's reference pack there (uaro-graphics\reference - the game's watcher keeps it fresh,
// app/reference_pack) read on a thread of its own at the first ask, and again when its pack.json changes (looked at
// every 30 s at most). Null until read, and when there is none.
std::shared_ptr<const refindex::Index> index();
// 3.0.11-beta.10: the players on each map now (uaro-graphics\reference\online.json, the watcher's every 2 minutes),
// read again when it changes (looked at every 30 s); null none or not fresh (core/reference_live)
std::shared_ptr<const reflive::Online> online();

}  // namespace uagfx::win::reference
