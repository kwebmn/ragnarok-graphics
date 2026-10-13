#pragma once
#include <optional>
#include <vector>

#include "core/config.hpp"
#include "core/settings/ini_edit.hpp"

namespace uagfx::settings {

enum class Preset { Maximum, Balance, Speed, Plain };
constexpr int kPresetCount = 4;

// The uaro-graphics.ini edits of a preset (stage 2 spec §8): high_resolution, render_scale, sharpen, ui_filter,
// crisp_text.
std::vector<IniEdit> presetEdits(Preset p);
// The preset whose values these settings hold, if any.
std::optional<Preset> presetOf(const Config& c);

}  // namespace uagfx::settings
