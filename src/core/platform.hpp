#pragma once
#include <string>

namespace uagfx::platform {

// 3.0.4-beta.8 (the user's idea 7): Linux and the Steam Deck - uaRO.exe under Wine (Steam's Proton): what runs it,
// for the log and for the one part Wine cannot carry. The 64-bit renderer (uaro-render64.exe: DLSS, the HD textures)
// shows its picture in the game's window from another process, or through DirectComposition, which Wine lacks - a
// picture that stops would be all; the game's own process draws there instead.
struct Facts {
    bool wine = false;
    std::string wineVersion;
    bool proton = false;     // Steam's (its STEAM_COMPAT_DATA_PATH)
    bool steamDeck = false;  // SteamOS on a Deck (SteamDeck=1)
};

std::string describe(const Facts& f);  // "Windows", "Wine 9.0 (Proton) on a Steam Deck"
// Whether the 64-bit renderer may start: under Wine only with [renderer] bridge_wine (a tester's try)
bool bridgeAllowed(const Facts& f, bool wineOverride);

}  // namespace uagfx::platform
