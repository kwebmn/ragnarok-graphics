#pragma once
#include <windows.h>

#include <string>

#include "core/config.hpp"
#include "core/window_places.hpp"

namespace uagfx::win {

// 3.0.3-beta.2 (the user, several game windows at once - the main character and helpers): the character's name in
// the game window's title ([windows] title_name), each character's window where it was put ([windows] remember_place,
// core/window_places in uaro-graphics\uaro-windows.txt). On the game's thread, a few times a second.
class SeveralWindows {
public:
    // `name`: the character in the game as the game spells it ("" at the login and the character select)
    void check(HWND window, const std::string& name, const Config& config);

private:
    void title(HWND window, const std::string& name, unsigned codePage);
    void place(HWND window, const std::string& name);

    bool ownKept_ = false;
    std::wstring own_;    // the game's own title, kept at the first change
    std::string titled_;  // the name in the title now, "" the game's own
    places::Tracker places_;
};

}  // namespace uagfx::win
