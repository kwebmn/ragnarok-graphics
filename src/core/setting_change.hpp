#pragma once
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/settings/ini_edit.hpp"

// 3.0.24-beta.37 (the laptop's run, AMD Radeon 610M): effects.ao = 1 through the MCP's «setting» did nothing - written
// straight, the effects' set «auto» went on deciding the switches. A setting changed as Alt+O changes it (from
// win/setting_edit, now its lines here): an old file's ten groups written into the effects first (2.6.22), an effect's
// own switch or the mode under a set making the set the player's own (2.2), «custom» chosen likewise (2.2.2).
namespace uagfx::settingchange {

enum class Outcome { Edits, Same, Unknown, Invalid };

// What Alt+O shows: the effects' switches as their set has them, the AI pictures as the file says now
Config shown(const Config& c, int cardLevel);
// The lines writing `key` ("section.key") = `token` takes, into `out` (Edits); Same: it is so already (as shown);
// Unknown: no such setting; Invalid: a value it does not take - `out` left as it was for these
Outcome edits(const Config& c, int cardLevel, const std::string& key, const std::string& token,
              std::vector<settings::IniEdit>& out);

}  // namespace uagfx::settingchange
