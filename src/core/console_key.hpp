#pragma once

namespace uagfx::console {

// 2.15: the effects' console (win/console) opens and closes on Ctrl + the key left of 1 (VK_OEM_3): that key alone is
// «ё» in a Russian layout and the apostrophe in a Ukrainian one - typed in the game's chat as letters.
constexpr unsigned kKey = 0xC0;  // VK_OEM_3
inline bool toggles(bool ctrl, unsigned vk) { return ctrl && vk == kKey; }

}  // namespace uagfx::console
