#pragma once
#include <cstddef>
#include <string>

namespace uagfx::textfield {

// 3.0.18-beta.72 (the big six, E: «свой ввод текста»): a field of the new interface typed at its end - its words UTF-8
// (any language the keyboard types, as uaUI draws them), at most `most` bytes; a whole-text selection (Ctrl+A) that the
// next letter, Backspace or Delete replaces
struct Field {
    std::string text;
    bool all = false;  // everything selected
};
bool type(Field& f, const std::string& utf8, size_t most);  // the letters (or a paste) added; false: nothing fitted
bool backspace(Field& f);  // the last letter (or everything selected) gone
bool clear(Field& f);
// A paste's words for a field of one line: line breaks and tabs as spaces, other control characters dropped
std::string oneLine(const std::string& utf8);

}  // namespace uagfx::textfield
