#pragma once
#include <windows.h>

#include <cstddef>
#include <string>

#include "core/text_field.hpp"

namespace uagfx::win::uifield {

// 3.0.18-beta.72 (the big six, E): a key while a field of the new interface types - the letters through the game's
// code page as UTF-8 (any language), Backspace a whole letter, Delete all, Ctrl+A all chosen, Ctrl+C/X/V the
// clipboard (one line); Enter and Esc let it go. Every key but Alt's is the field's: none reaches the game (no letter
// twice, no skill on a letter typed). True: taken.
enum class Result : unsigned char { None, Changed, Done };
bool key(textfield::Field& f, UINT m, WPARAM wp, size_t most, Result& result);
// The clipboard's words as UTF-8 ("" none); put words on it
std::string clipboardText();
void setClipboard(const std::string& utf8);

}  // namespace uagfx::win::uifield
