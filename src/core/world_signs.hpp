#pragma once
#include <cstdint>
#include <string>

namespace uagfx::worldsigns {

// 3.0.16-beta.7 (the owner: «делай все» - the shops' signs and the chat rooms'): the game's signs over the characters
// are its world windows (core/world_windows): a shop's (class 0x69673C, 0x50 bytes: its merchant's account at + 0x4C,
// the message 0x1C; a click asks the game mode's 0x5F with it) and a chat room's (class 0x6966A4, 0x7C bytes: its type
// at + 0x6C, the room's id at + 0x78; a click joins it - 0 a locked one asks its password first, 3 none). Their
// place: the window's x, y, w, h at + 0x14. [interface] signs draws them anew as the new look's plates; the game's
// window stays where it is (not drawn) and takes the clicks as before.
constexpr uint32_t kShopClass = 0x69673C, kRoomClass = 0x6966A4;
constexpr uint32_t kRect = 0x14, kShopAccount = 0x4C, kRoomType = 0x6C, kRoomId = 0x78;

enum class Kind : uint8_t { Shop, Room, Locked };

// A room's kind by its type (0x00d7's, the window's + 0x6C): 0 locked, the rest open
inline Kind roomKind(int type) { return type == 0 ? Kind::Locked : Kind::Room; }

// The plate's accent: a shop's gold, an open room's blue, a locked one's grey (0xRRGGBB)
uint32_t accent(Kind k);

// A room's places as the plate shows them: «3/20»; none without a limit
std::string places(int users, int limit);

// A title kept to `most` letters (UTF-8 aware), an ellipsis put at its end when cut; spaces round it dropped
std::string shortTitle(const std::string& utf8, size_t most);

}  // namespace uagfx::worldsigns
