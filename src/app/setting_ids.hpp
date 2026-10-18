#pragma once
#include <cstddef>

#include "core/settings/schema.hpp"

namespace uagfx::app {

// The control ids of the settings pages built from the schema (page_settings.cpp): four for each row of
// uaro-graphics.ini's schema (its box, list or field, a slider's number) from kRows, each row of the game's key from
// kReg, and the page's own buttons. 2.4.2: the rows from 4000 - from 1100 the schema's row 98 on (the Frames page, the
// Sound page's own part) ran into kForgetLogin and the game's key's ids, and got no commands: their boxes changed
// nothing.
constexpr int kRows = 4000;  // + 4 x the setting's index in iniSettings()
constexpr int kForgetLogin = 1490;
constexpr int kAuto = 1492;  // 0.50
constexpr int kGpu = 1493;   // 0.50.2
constexpr int kReg = 1500;   // + 4 x the setting's index in regSettings()

inline int rowId(size_t i) { return kRows + int(i) * 4; }
inline const settings::IniSetting* settingOfId(int id) {
    const std::vector<settings::IniSetting>& all = settings::iniSettings();
    if (id < kRows || id >= rowId(all.size())) return nullptr;
    const size_t i = size_t(id - kRows) / 4;
    return (id - kRows) % 4 == 0 ? &all[i] : nullptr;
}

}  // namespace uagfx::app
