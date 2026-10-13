#pragma once
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/config.hpp"
#include "core/settings/ini_edit.hpp"

namespace uagfx::settings {

enum class Preset { Maximum, Balance, Speed, Plain };
constexpr int kPresetCount = 4;

// The uaro-graphics.ini edits of a preset (stage 2 spec §8): high_resolution, render_scale, sharpen, ui_filter,
// crisp_text.
std::vector<IniEdit> presetEdits(Preset p);
// The game's own quality a preset sets from 0.6.2 (spec §8), as the words of the game's key settings (game_settings):
// Maximum and Balance - sprites and textures high; Speed - textures half; mipmaps never.
std::vector<std::pair<std::string, std::string>> presetRegWords(Preset p);
// The preset whose values these settings hold, if any; `gameWords` (setting -> word) is checked where it has the words
// a preset sets, and an empty one leaves the game's quality out.
std::optional<Preset> presetOf(const Config& c, const std::map<std::string, std::string>& gameWords = {});

}  // namespace uagfx::settings
