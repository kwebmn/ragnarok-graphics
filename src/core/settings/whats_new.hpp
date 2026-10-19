#pragma once
#include <string>

namespace uagfx::settings {

// 2.6: "What's new" - a version's part of the release notes the package carries (TEST.<language>.md next to
// uaRO.exe: "# uaRO Graphics 2.5.27 — ..." and its points), as plain text: a line a point, "•" before it, no
// Markdown marks; "" when the notes have no such version.
std::string notesOf(const std::string& markdown, const std::string& version);

}  // namespace uagfx::settings
