#pragma once
#include <string>

#include "core/config.hpp"

namespace uagfx::win {

// The game's text in Windows' terms (3.0.3-beta.6, from several_windows): its code page - [roext] code_page with
// ROExt's work in the driver, else Windows' own - and a text of it as Unicode, Unicode as UTF-8.
unsigned gameCodePage(const Config& config);
std::wstring widened(const std::string& text, unsigned codePage);
std::string utf8Of(const std::wstring& text);

}  // namespace uagfx::win
