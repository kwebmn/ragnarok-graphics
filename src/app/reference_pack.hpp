#pragma once
#include <optional>
#include <string>

#include "core/reference_pack.hpp"

namespace uagfx::app {

// 3.0.11-beta.5: the game's reference pack (core/reference_pack) in <client>\uaro-graphics\reference\ - the game's
// watcher asks the site at the game's start. true: the pack there is the site's newest (fetched now or before); false
// and why (the site not reached, a pack of other bytes than its manifest says) - the copy there stays as it was.
bool noteReference(const std::wstring& dir, std::string& error);
// The manifest of the pack there (the app shows it): none when there is no pack.json or no pack beside it.
std::optional<refpack::Manifest> referenceThere(const std::wstring& dir);

}  // namespace uagfx::app
