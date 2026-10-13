#pragma once
#include <string>

namespace uagfx::app {

// Alt+letter on the settings app's controls (stage 2 spec §6). withMnemonic puts '&' before a letter of the text not
// taken yet - the first of a word if one is free, else any - and adds it to `taken` (lower case); an '&' already in
// the text is doubled, to show as it is. No free letter: the text gets none. A text that marks a letter of its own
// ("&Fix") keeps it, and the letter is taken. Latin and Cyrillic letters, digits.
std::wstring withMnemonic(const std::wstring& text, std::wstring& taken);
// The letter a text already marks with '&' ("&Apply") goes into `taken`.
void takeMnemonic(const std::wstring& text, std::wstring& taken);

}  // namespace uagfx::app
