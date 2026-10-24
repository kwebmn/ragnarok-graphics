#pragma once
#include <windows.h>

#include <cstdint>
#include <string>

#include "core/config.hpp"
#include "core/window_places.hpp"

namespace uagfx::win {

// 3.0.3-beta.2 (the user, several game windows at once - the main character and helpers): the character in the game
// window's title ([windows] title_name; 3.0.3-beta.5 its numbers and the client's name, core/window_title), each
// character's window where it was put ([windows] remember_place, core/window_places in uaro-graphics\uaro-windows.txt).
// On the game's thread, a few times a second.
class SeveralWindows {
public:
    // `name`: the character in the game as the game spells it ("" at the login and the character select)
    void check(HWND window, const std::string& name, const Config& config);

private:
    std::wstring titleOf(const std::string& name, unsigned codePage) const;
    void title(HWND window, const std::wstring& text, bool now);
    void place(HWND window, const std::string& name);

    bool ownKept_ = false;
    std::wstring own_;     // the game's own title, kept at the first check
    std::string client_;   // the client's name: its build's uaro-client.json, else «uaRO»
    std::wstring titled_;  // the title now
    uint64_t titledAt_ = 0;
    std::string named_;     // whose it is
    bool titleOn_ = false;  // [windows] title_name then
    places::Tracker places_;
};

}  // namespace uagfx::win
