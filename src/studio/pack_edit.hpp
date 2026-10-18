#pragma once
#include <functional>
#include <string>
#include <vector>

#include "core/texture_pack.hpp"

namespace uagfx::studio {

// 2.3 (T4, T7): a change of the pack by the studio - one writer at a time: the builder's mutex taken (a builder
// running, or another studio's change: busy) and the pack writer's (a game making pictures holds it: busy), the pack
// gone on (pack::Writer, resume), changed, closed with its index; nothing is rewritten but its directory and index.
// false and why: "the pack is busy", or what failed.
using PackChange = std::function<bool(pack::Writer& w, pack::Index& index, std::string& error)>;
bool packEdit(const std::string& pack, const PackChange& change, std::string& error);
// A running game told of the keys changed (link\changed.txt, the event Local\uaro-studio-changed): it takes their
// pictures and materials from the pack again.
void tellGame(const std::vector<std::string>& keys);

}  // namespace uagfx::studio
