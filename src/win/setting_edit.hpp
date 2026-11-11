#pragma once
#include <string>

#include "core/config.hpp"
#include "core/settings/choice_presets.hpp"

namespace uagfx::win::settingedit {

// 3.0.18-beta.122 (the concept «Ігровий інтерфейс»: «Налаштування» beside the console): a setting of uaro-graphics.ini
// written as the console writes it - `key` ("interface.buffs") to `token`; an effect's own switch or the effects' mode
// while a set decides them - the set made the player's own first (in the same write). False: not written (the same
// already, not a key of the schema, the file not writable)
bool set(const std::string& iniPath, const Config& config, const std::string& key, const std::string& token);
// One of «Як виглядає гра»'s choices - its variant's keys written
bool setLook(const std::string& iniPath, settings::presets::Choice c, int variant);
// The settings as the console shows them - each effect's switch as the set has it (the AI pictures as the file says)
Config shown(const Config& c);

}  // namespace uagfx::win::settingedit
