#pragma once
#include <bitset>
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::closedwin {

// 3.0.18-beta.120 (the concept «Ігровий інтерфейс», Claude Doc 3zHfHfPfhA5nCjqGU5xb1Z, stage 2: «✕ на каждом окне»):
// our own windows the player closed with ✕ - hidden, not switched off (their switches stay on); opened again by the
// character block's icons or their keys; kept in uaro-graphics\closed-windows.txt, a key a line
enum class Window : uint8_t { Hunt, Mvp, Party, Target, Count };  // «Відстеження» keeps its own (its ✕ hides it)
using Set = std::bitset<size_t(Window::Count)>;
const char* keyOf(Window w);  // "hunt", "mvp", "party", "target"
Set read(const std::string& text);
std::string write(const Set& s);
inline bool has(const Set& s, Window w) { return s.test(size_t(w)); }
inline void set(Set& s, Window w, bool closed) { s.set(size_t(w), closed); }

// 3.0.18-beta.120 (stage 3): the character block's second row - our windows («Довідник», «Полювання», «Таймери
// босів», «Відстеження», «Група», «Що мене вбило», «Налаштування»), each shown when its switch is on (settings always)
enum class Own : uint8_t { Reference, Hunt, Mvp, Tracker, Party, Recap, Settings, Count };
struct Switches {
    bool reference = false, hunt = false, mvp = false, tracker = false, party = false, recap = false;
};
std::vector<Own> row(const Switches& on);
// Ctrl+letter's window (not «Довідник»'s Ctrl+F - its own); false: none of ours
bool ownOfKey(unsigned vk, Own& out);
const char* keyName(Own o);  // "Ctrl+H"; settings "Alt+D" (the console, till its own window)

}  // namespace uagfx::closedwin
