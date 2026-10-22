#include "pad/core/config/settings.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace uapad {
namespace {

constexpr int kWalkMin = 10;  // the menu's walk radius, percent
constexpr int kWalkMax = 44;
constexpr int kWalkStep = 2;

constexpr int kSleepWhenOn = 10;  // background_sleep_ms when the file has no number of its own

int sleepOn(int sleepWhenOn) { return sleepWhenOn > 0 ? sleepWhenOn : kSleepWhenOn; }

const char* onOff(bool v) { return v ? "1" : "0"; }

std::string markText(MarkStyle style, Lang lang) {
    switch (style) {
        case MarkStyle::Poison: return tr(lang, Text::MarkPoison);
        case MarkStyle::DeadlyPoison: return tr(lang, Text::MarkDeadlyPoison);
        case MarkStyle::Curse: return tr(lang, Text::MarkCurse);
        case MarkStyle::Stun: return tr(lang, Text::MarkStun);
        case MarkStyle::Sleep: return tr(lang, Text::MarkSleep);
        case MarkStyle::Silence: return tr(lang, Text::MarkSilence);
        case MarkStyle::Sight: return tr(lang, Text::MarkSight);
        case MarkStyle::Ruwach: return tr(lang, Text::MarkRuwach);
        case MarkStyle::Gasp: return tr(lang, Text::MarkGasp);
        case MarkStyle::Off: break;
    }
    return tr(lang, Text::Off);
}

int mouseKeyIndex(SettingRow row) { return row == SettingRow::Middle ? 0 : row == SettingRow::Button4 ? 1 : 2; }

Text labelOf(SettingRow row) {
    switch (row) {
        case SettingRow::Language: return Text::Language;
        case SettingRow::Lock: return Text::Lock;
        case SettingRow::FocusClick: return Text::FocusClick;
        case SettingRow::Middle: return Text::MiddleButton;
        case SettingRow::Button4: return Text::MouseButton4;
        case SettingRow::Button5: return Text::MouseButton5;
        case SettingRow::Borderless: return Text::Borderless;
        case SettingRow::SaveCpu: return Text::SaveCpu;
        case SettingRow::Rumble: return Text::Rumble;
        case SettingRow::WalkRadius: return Text::WalkRadius;
        case SettingRow::Mark: break;
        case SettingRow::Count: break;
    }
    return Text::Mark;
}

// The row's line in uaro-pad.ini.
IniEdit editOf(const Settings& s, SettingRow row, int sleepWhenOn) {
    switch (row) {
        case SettingRow::Language: return IniEdit{"general", "language", langToken(s.lang)};
        case SettingRow::Lock: return IniEdit{"mouse", "lock", onOff(s.lock)};
        case SettingRow::FocusClick: return IniEdit{"mouse", "focus_click", onOff(s.focusClick)};
        case SettingRow::Middle: return IniEdit{"mouse", "middle", fKeyToken(s.mouseKeys[0])};
        case SettingRow::Button4: return IniEdit{"mouse", "button4", fKeyToken(s.mouseKeys[1])};
        case SettingRow::Button5: return IniEdit{"mouse", "button5", fKeyToken(s.mouseKeys[2])};
        case SettingRow::Borderless: return IniEdit{"window", "borderless", onOff(s.borderless)};
        case SettingRow::SaveCpu:
            return IniEdit{"window", "background_sleep_ms", std::to_string(s.saveCpu ? sleepOn(sleepWhenOn) : 0)};
        case SettingRow::Rumble: return IniEdit{"rumble", "enabled", onOff(s.rumble)};
        case SettingRow::WalkRadius: {
            char text[16];
            std::snprintf(text, sizeof text, "%.2f", s.walkPercent / 100.0);
            return IniEdit{"stick", "walk_radius", text};
        }
        case SettingRow::Mark: break;
        case SettingRow::Count: break;
    }
    return IniEdit{"aim", "mark", markStyleToken(s.mark)};
}

bool same(const Settings& a, const Settings& b, SettingRow row) {
    switch (row) {
        case SettingRow::Language: return a.lang == b.lang;
        case SettingRow::Lock: return a.lock == b.lock;
        case SettingRow::FocusClick: return a.focusClick == b.focusClick;
        case SettingRow::Middle:
        case SettingRow::Button4:
        case SettingRow::Button5: return a.mouseKeys[mouseKeyIndex(row)] == b.mouseKeys[mouseKeyIndex(row)];
        case SettingRow::Borderless: return a.borderless == b.borderless;
        case SettingRow::SaveCpu: return a.saveCpu == b.saveCpu;
        case SettingRow::Rumble: return a.rumble == b.rumble;
        case SettingRow::Mark: return a.mark == b.mark;
        case SettingRow::WalkRadius: return a.walkPercent == b.walkPercent;
        case SettingRow::Count: break;
    }
    return true;
}

}  // namespace

Settings settingsFrom(const Config& c) {
    Settings s;
    s.lang = c.lang;
    s.lock = c.mouseLock;
    s.focusClick = c.focusClick;
    s.mouseKeys = c.mouseKeys;
    s.borderless = c.borderless;
    s.saveCpu = c.backgroundSleepMs > 0;
    s.rumble = c.rumble;
    s.mark = c.mark;
    s.walkPercent = int(std::lround(c.walkRadius * 100.0f));
    return s;
}

Settings defaultSettings() { return settingsFrom(Config{}); }

Settings changeSetting(Settings s, SettingRow row, int dir) {
    switch (row) {
        case SettingRow::Language: s.lang = nextLang(s.lang, dir); break;
        case SettingRow::Lock: s.lock = !s.lock; break;
        case SettingRow::FocusClick: s.focusClick = !s.focusClick; break;
        case SettingRow::Middle:
        case SettingRow::Button4:
        case SettingRow::Button5: {
            int& key = s.mouseKeys[mouseKeyIndex(row)];
            key = ((key + (dir > 0 ? 1 : -1)) % 10 + 10) % 10;  // none, F1 ... F9
            break;
        }
        case SettingRow::Borderless: s.borderless = !s.borderless; break;
        case SettingRow::SaveCpu: s.saveCpu = !s.saveCpu; break;
        case SettingRow::Rumble: s.rumble = !s.rumble; break;
        case SettingRow::Mark:
            s.mark = MarkStyle(((int(s.mark) + (dir > 0 ? 1 : -1)) % kMarkStyles + kMarkStyles) % kMarkStyles);
            break;
        case SettingRow::WalkRadius:  // a slider: it stops at both ends
            s.walkPercent = std::clamp(s.walkPercent + (dir > 0 ? kWalkStep : -kWalkStep), kWalkMin, kWalkMax);
            break;
        case SettingRow::Count: break;
    }
    return s;
}

Settings resetSetting(Settings s, SettingRow row) {
    const Settings d = defaultSettings();
    switch (row) {
        case SettingRow::Language: s.lang = d.lang; break;
        case SettingRow::Lock: s.lock = d.lock; break;
        case SettingRow::FocusClick: s.focusClick = d.focusClick; break;
        case SettingRow::Middle:
        case SettingRow::Button4:
        case SettingRow::Button5: s.mouseKeys[mouseKeyIndex(row)] = d.mouseKeys[mouseKeyIndex(row)]; break;
        case SettingRow::Borderless: s.borderless = d.borderless; break;
        case SettingRow::SaveCpu: s.saveCpu = d.saveCpu; break;
        case SettingRow::Rumble: s.rumble = d.rumble; break;
        case SettingRow::Mark: s.mark = d.mark; break;
        case SettingRow::WalkRadius: s.walkPercent = d.walkPercent; break;
        case SettingRow::Count: break;
    }
    return s;
}

std::string settingLabel(SettingRow row, Lang lang) { return tr(lang, labelOf(row)); }

std::string settingValue(const Settings& s, SettingRow row, Lang lang) {
    auto onOffText = [&](bool v) { return std::string(tr(lang, v ? Text::On : Text::Off)); };
    switch (row) {
        case SettingRow::Language: return langName(s.lang);
        case SettingRow::Lock: return onOffText(s.lock);
        case SettingRow::FocusClick: return tr(lang, s.focusClick ? Text::FocusGoesIn : Text::FocusStaysOut);
        case SettingRow::Middle:
        case SettingRow::Button4:
        case SettingRow::Button5: {
            const int key = s.mouseKeys[mouseKeyIndex(row)];
            return key == 0 ? std::string("—") : fKeyToken(key);
        }
        case SettingRow::Borderless: return onOffText(s.borderless);
        case SettingRow::SaveCpu: return onOffText(s.saveCpu);
        case SettingRow::Rumble: return onOffText(s.rumble);
        case SettingRow::Mark: return markText(s.mark, lang);
        case SettingRow::WalkRadius: return std::to_string(s.walkPercent) + "%";
        case SettingRow::Count: break;
    }
    return "";
}

std::vector<IniEdit> settingsEdits(const Settings& before, const Settings& after, int sleepWhenOn) {
    std::vector<IniEdit> edits;
    for (size_t i = 0; i < kSettingRows; ++i) {
        const SettingRow row = SettingRow(i);
        if (!same(before, after, row)) edits.push_back(editOf(after, row, sleepWhenOn));
    }
    return edits;
}

void applySettings(const Settings& s, int sleepWhenOn, Config& c) {
    c.lang = s.lang;
    c.mouseLock = s.lock;
    c.focusClick = s.focusClick;
    c.mouseKeys = s.mouseKeys;
    c.borderless = s.borderless;
    c.backgroundSleepMs = s.saveCpu ? sleepOn(sleepWhenOn) : 0;
    c.rumble = s.rumble;
    c.mark = s.mark;
    c.walkRadius = float(s.walkPercent) / 100.0f;
}

}  // namespace uapad
