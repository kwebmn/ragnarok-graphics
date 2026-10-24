#include "core/platform.hpp"

namespace uagfx::platform {

std::string describe(const Facts& f) {
    if (!f.wine) return "Windows";
    std::string s = "Wine " + (f.wineVersion.empty() ? std::string("?") : f.wineVersion);
    if (f.proton) s += " (Proton)";
    if (f.steamDeck) s += " on a Steam Deck";
    return s;
}

bool bridgeAllowed(const Facts& f, bool wineOverride) { return !f.wine || wineOverride; }

}  // namespace uagfx::platform
