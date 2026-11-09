#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/settings/game_registry.hpp"

namespace uagfx::settings {

// 3.0.17-beta.18: every client of one PC reads its screen from one key (Software\Gravity Soft\Ragnarok + the player's
// VirtualStore copy): uaRO HD's (32 bits, its size, "Direct3D HAL") stopped the old client on dgVoodoo - «Cannot init
// d3d OR grf file has problem». A client of its own (uaro-client.json) keeps its screen in its own file
// (layout::kGameScreen); the driver answers the game's reads of these values from it and takes its writes of them,
// the key is not touched for them. (Beta.13-.16 gave the game a key of its own: the game hung, never confirmed why.)
struct GameScreen {
    bool fullscreen = false;
    uint32_t width = 0, height = 0;
};
std::optional<GameScreen> parseGameScreen(const std::string& text);  // nothing without a size of 1..16384
std::string gameScreenText(const GameScreen& s);

// The screen's values the game reads (uaRO.exe FUN_00654f60) and the settings app writes
const std::vector<std::string>& screenValueNames();
// The value the game gets for `name` (any case): the screen's size and mode, 32 bits, Direct3D HAL's device, the
// default driver; false for any other name
bool screenValue(const GameScreen& s, const std::string& name, RegValue& out);
// A write of the game's own (its options): the size or the mode into `s`; BITPERPIXEL and the device's taken as
// they are (the driver's are fixed); false for any other name or a value of another type
bool takeScreenWrite(GameScreen& s, const std::string& name, const RegValue& v);
// The screen the game reads in the key now (WIDTH, HEIGHT, ISFULLSCREENMODE), none without a size
std::optional<GameScreen> screenOf(const GameRegistry& r);

// 3.0.17-beta.14: the old client's Apps entry (app/installer namesOf): uaRO Graphics there - its own screen stays
constexpr const char* kOldAppsEntry = "uaRO Graphics";
// The common copy holds a setup.exe's screen: WIDTH, HEIGHT and DEVICENAME in the key itself
bool hasOwnSetup(const RegCopy& common);
// The screen's values of the shared key's personal copy go, so the old client reads its setup.exe's again - only when
// the common copy holds them (else nothing: no setup.exe screen to go back to)
std::vector<RegWrite> sharedRepair(const GameRegistry& shared);

}  // namespace uagfx::settings
