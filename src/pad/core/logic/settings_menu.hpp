#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "pad/core/config/bindings.hpp"
#include "pad/core/config/settings.hpp"
#include "pad/core/logic/pad.hpp"
#include "pad/core/world/world.hpp"

namespace uapad {

// The settings menu behind Start, without any drawing. Two tabs: Buttons (the bindable buttons in
// kBindableButtons order, then "reset all buttons") and Settings (SettingRow order, then "reset all
// settings"). The menu speaks the language of its own Language row, so a change shows at once.
class SettingsMenu {
public:
    enum class Result { None, Save, Cancel };
    enum class Tab { Buttons, Settings };
    struct Row {
        std::string label;  // UTF-8: the button, the setting, or the reset command
        std::string value;  // UTF-8: what it does / is set to; empty on the reset row
        bool hasValue = true;
        bool selected = false;
    };

    // `slotNames`: what the client's bar holds in each slot (UTF-8, empty when unknown), shown next to "Slot".
    // Buttons and stick directions held right now (the Start that opened the menu) count as already pressed.
    void open(const Bindings& current, const Settings& settings, const std::array<std::string, kHotkeySlots>& slotNames,
              uint16_t heldButtons, uint32_t nowMs, float lx = 0, float ly = 0);
    void close() { open_ = false; }
    bool isOpen() const { return open_; }

    // One frame of the pad: LB / RB switch tabs, up/down picks a row, left/right changes its value (held:
    // repeats), A or Start saves, B cancels, Y resets the row; A on the last row resets the tab.
    Result update(const PadState& pad, uint32_t nowMs);
    // The mouse: show a tab; pick a row of the shown tab; turn its value one step; reset the shown tab.
    void showTab(Tab tab) { tab_ = tab; }
    void select(size_t row);
    void change(size_t row, int dir);
    void resetTab();

    Tab tab() const { return tab_; }
    size_t selected() const { return selected_[size_t(tab_)]; }
    size_t rowCount() const { return rowCount(tab_); }
    static size_t rowCount(Tab tab) { return (tab == Tab::Buttons ? kBindableCount : kSettingRows) + 1; }
    static size_t maxRowCount() { return std::max(rowCount(Tab::Buttons), rowCount(Tab::Settings)); }
    const Bindings& bindings() const { return bindings_; }
    const Settings& settings() const { return settings_; }
    Lang lang() const { return settings_.lang; }
    std::string tabTitle(Tab tab) const;
    std::vector<Row> rows() const;  // of the shown tab

private:
    // A direction held on the D-pad or the left stick: acts on the press, then again after a pause.
    struct Repeat {
        int dir = 0;
        bool live = true;  // false: held since before the menu opened, ignored until let go
        uint32_t nextMs = 0;
        void reset(int held);
        bool step(int d, uint32_t now);
    };

    void resetRow(size_t row);

    bool open_ = false;
    Tab tab_ = Tab::Buttons;
    Bindings bindings_{};
    Settings settings_;
    std::array<std::string, kHotkeySlots> slotNames_{};
    std::array<size_t, 2> selected_{};
    uint16_t prevButtons_ = 0;
    Repeat vertical_;
    Repeat horizontal_;
};

}  // namespace uapad
