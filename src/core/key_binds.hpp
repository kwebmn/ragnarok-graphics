#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace uagfx::keybinds {

// 3.0.24-beta.55 (the owner's «делай все что можешь без проверки», item 12: the keyboard concept's last stage, its
// «rebinding»): the walk mode's keys the player's own - [keys] in uaro-graphics.ini (the settings' choices), each
// action a key by its place on the keyboard (its scan code: the same key in any layout). The arrows, W A S D, the
// numpad, Enter, Esc and 1-9 stay the mode's own.
enum class Bind : uint8_t { Toggle, Attack, Pickup, Talk, Sit, Target, CameraLeft, CameraRight, kCount };
constexpr int kBinds = int(Bind::kCount);

struct Binds {
    unsigned scan[kBinds] = {0x29, 0x39, 0x21, 0x13, 0x2E, 0x0F, 0x10, 0x12};  // ` Space F R C Tab Q E
};
Binds defaults();

// The keys one may be given, by their words in the file (and the settings' choices' order)
constexpr const char* kTokens[] = {"grave", "space", "tab", "q", "e", "r", "f", "c", "g", "t", "z", "x", "v", "b"};
constexpr int kTokenCount = int(sizeof kTokens / sizeof kTokens[0]);
unsigned scanOf(const std::string& token);  // 0: not one of them
const char* tokenOf(unsigned scan);         // "": not one of them
const char* settingKey(Bind b);             // [keys]'s: "walk_toggle", "attack", ... "camera_right"
// The action a key is given to (`found` false: none)
Bind boundTo(const Binds& b, unsigned scan, bool& found);
// The keys list's walk mode line by the player's keys: «{1}» the toggle's name ... «{8}» the camera's right (Bind's
// order), each named by `name` (its scan code)
std::string walkLine(const std::string& form, const Binds& b, const std::function<std::string(unsigned scan)>& name);

}  // namespace uagfx::keybinds
