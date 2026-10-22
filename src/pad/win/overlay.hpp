#pragma once
#include <windows.h>

#include <cstddef>
#include <string>
#include <vector>

// The settings menu's window: drawn with GDI over the game window's client area. It never takes the focus,
// so the game keeps its keyboard and the pad keeps working; the mouse can click it.
namespace uapad::overlay {

struct View {
    struct Row {
        std::wstring label;
        std::wstring value;
        bool command = false;  // no value: a click on it does it (the "reset all" rows)
        bool operator==(const Row& o) const { return label == o.label && value == o.value && command == o.command; }
    };
    std::wstring title;
    std::vector<std::wstring> tabs;  // left to right
    size_t tab = 0;                  // the one shown
    std::vector<Row> rows;
    size_t selected = 0;
    size_t rowSlots = 0;   // rows the panel has room for: the taller tab, so switching tabs keeps its size
    int labelWidth = 166;  // pixels for the labels; the values take the rest
    std::wstring help;     // may hold a line break
    std::wstring save;
    std::wstring cancel;
    bool operator==(const View& o) const {
        return title == o.title && tabs == o.tabs && tab == o.tab && rows == o.rows && selected == o.selected &&
               rowSlots == o.rowSlots && labelWidth == o.labelWidth && help == o.help && save == o.save &&
               cancel == o.cancel;
    }
};

enum class Click { Tab, Select, Previous, Next, Save, Cancel };
struct MouseEvent {
    Click click;
    size_t row;  // the row; for Click::Tab the tab
};

// Shows the window (created on first use) or refreshes it, centred over `game`'s client area.
// False when the window cannot be created.
bool show(HWND game, const View& view);
void hide();
// Clicks on the window since the last call.
std::vector<MouseEvent> takeClicks();

}  // namespace uapad::overlay
