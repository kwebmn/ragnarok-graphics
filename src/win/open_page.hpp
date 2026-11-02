#pragma once
#include <string>

namespace uagfx::win {

// A page of the site in the player's browser (shell32 asked for only now: the driver's imports stay the four)
bool openPage(const std::wstring& url);

}  // namespace uagfx::win
