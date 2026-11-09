#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/settings/game_registry.hpp"

namespace uagfx::settings {

// 3.0.17-beta.13: every client of one PC read and wrote one key - Software\Gravity Soft\Ragnarok (with the player's
// VirtualStore copy): uaRO HD's screen (32 bits, its size, "Direct3D HAL") reached the old client on dgVoodoo, which
// stopped with «Cannot init d3d OR grf file has problem». A client of its own (uaro-client.json) gets a key of its
// own: the driver writes its name over the game's string (uaRO.exe 0x6BD7A8, 32 bytes to the next one).
constexpr const char* kSharedLeaf = "Ragnarok";
constexpr size_t kGameKeyRoom = 31;  // characters the game's string holds, its NUL after them
constexpr uint32_t kGameKeyAt = 0x6BD7A8;  // uaRO.exe's "Software\\Gravity Soft\\Ragnarok" (.data)

// The key's last part for a client: "Ragnarok" for the old uaRO (and an unknown one); its name when it fits the
// game's string (ASCII, no '\'); else "uaRO-" and four hex digits of the name, the same every time
std::string gameKeyLeaf(const std::string& client);
std::string gameKeyPath(const std::string& leaf);  // "Software\\Gravity Soft\\" + leaf

// The screen's values the settings app writes for its client (firstRunWrites, the screen's page); the old client's
// own come from its setup.exe, in the common copy
const std::vector<std::string>& screenValueNames();
// The common copy holds a setup.exe's screen: WIDTH, HEIGHT and DEVICENAME in the key itself
bool hasOwnSetup(const RegCopy& common);
// When a client moves to its own key: the screen's values of the shared key's personal copy go, so the old client
// reads its setup.exe's again - only when the common copy holds them (else nothing: no setup.exe screen to go back to)
std::vector<RegWrite> sharedRepair(const GameRegistry& shared);

}  // namespace uagfx::settings
