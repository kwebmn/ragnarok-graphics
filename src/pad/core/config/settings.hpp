#pragma once
#include <array>
#include <string>
#include <vector>

#include "pad/core/config/config.hpp"
#include "pad/core/config/ini_edit.hpp"
#include "pad/core/i18n.hpp"

namespace uapad {

// What the Settings tab of the Start menu edits: one value per row, kept in uaro-pad.ini.
struct Settings {
    Lang lang = Lang::En;
    bool lock = false;
    bool focusClick = false;
    std::array<int, 3> mouseKeys = {0, 0, 0};  // middle, 4th, 5th button: 0 none, 1-9 F1-F9
    bool borderless = false;
    bool saveCpu = true;
    bool rumble = true;
    MarkStyle mark = MarkStyle::Poison;
    int walkPercent = 22;  // [stick] walk_radius, in percent of the window's shorter side
    bool operator==(const Settings& o) const {
        return lang == o.lang && lock == o.lock && focusClick == o.focusClick && mouseKeys == o.mouseKeys &&
               borderless == o.borderless && saveCpu == o.saveCpu && rumble == o.rumble && mark == o.mark &&
               walkPercent == o.walkPercent;
    }
    bool operator!=(const Settings& o) const { return !(*this == o); }
};

// The rows of the Settings tab, in menu order.
enum class SettingRow {
    Language,
    Lock,
    FocusClick,
    Middle,
    Button4,
    Button5,
    Borderless,
    SaveCpu,
    Rumble,
    Mark,
    WalkRadius,
    Count
};
constexpr size_t kSettingRows = size_t(SettingRow::Count);

Settings settingsFrom(const Config& c);
Settings defaultSettings();  // the built-in values: what Y and "reset all settings" go back to
Settings changeSetting(Settings s, SettingRow row, int dir);  // the next value in the row's ring
Settings resetSetting(Settings s, SettingRow row);
std::string settingLabel(SettingRow row, Lang lang);                     // UTF-8
std::string settingValue(const Settings& s, SettingRow row, Lang lang);  // UTF-8; a language in its own words

// The uaro-pad.ini edits for the rows that differ. Switching the CPU saving on writes `sleepWhenOn`
// (the number the file had, or 10 when it had 0).
std::vector<IniEdit> settingsEdits(const Settings& before, const Settings& after, int sleepWhenOn);
// `c` with the settings put in, for the running module after Save.
void applySettings(const Settings& s, int sleepWhenOn, Config& c);

}  // namespace uapad
