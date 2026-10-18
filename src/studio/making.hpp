#pragma once
#include <string>

#include "core/settings/strings.hpp"

namespace uagfx::studio {

class Jobs;
class CatalogWork;

// 2.3 (T4, T5, T6): the studio's work on pictures, run by its jobs - make (a texture's picture made again by a chain
// about the engine's network, kept in studio\made\ to be looked at), keep (into the pack, its '+' line), take out,
// the game's own always; every write of the pack under its mutexes, the running game told.
void addPictureWork(Jobs& jobs, CatalogWork& catalog, settings::Lang lang);

std::wstring madeFile(const std::string& key);  // studio\made\<key>.raw (its chain beside it in .txt)

}  // namespace uagfx::studio
