#pragma once
#include <string>
#include <vector>

namespace uagfx::wine_setup {

// 3.0.23-beta.13 (R-F93RB7: Debian + Bottles, the game a white screen and no uaro-graphics.log - Wine's own built-in
// ddraw taken over ours): under Wine the settings app tells Wine, for the game's exe alone, to take these DLLs from
// the game's folder first (Wine's own after) - HKCU\Software\Wine\AppDefaults\<exe>\DllOverrides
struct Override {
    const wchar_t* dll;
    const wchar_t* order;
};
// ddraw: our driver; dinput8: ROExt (uaRO's dinput8.dll). The game's other DLLs (cps, binkw32, ijl15, mss32) have no
// built-in Wine twin: taken from its folder anyway
const std::vector<Override>& overrides();
// The key for an exe given by its path or name (the name alone, as Wine matches it); empty for none
std::wstring appDefaultsKey(const std::wstring& exe);

}  // namespace uagfx::wine_setup
