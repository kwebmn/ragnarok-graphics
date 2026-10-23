#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace uagfx::charname {

// 3.0.3-beta.2 (the user, several game windows: the character's name in the window's title): uaRO.exe keeps its
// character's name XOR a key - CSession (kSession) +kNameAt, kStored bytes XOR the kStored at *kKeyAt
// (CSession::GetCharName 0x642500 decodes it, SetCharName 0x642550 sets it at the character select, 0x45aeab).
constexpr uint32_t kSession = 0x771148;
constexpr uint32_t kNameAt = 0x618;
constexpr uint32_t kKeyAt = 0x6E2F78;
constexpr size_t kStored = 64;
constexpr size_t kMost = 24;  // the server's names: 23 bytes and their end
// GetCharName's first bytes in this build: lea esi, [ecx+0x618]; mov ecx, 16; mov edi, 0x772fe8; rep movsd;
// mov ecx, [0x6e2f78] - the name's place and key are where they are read from
constexpr uint32_t kGetCharName = 0x642500;
constexpr size_t kCodeSize = 27;
extern const uint8_t kCode[kCodeSize];

// The name to its end, as the game spells it (its code page); "" for none - never set (all zero), no end within
// kMost bytes, or a control character in it.
std::string decode(const uint8_t* stored, const uint8_t* key);

}  // namespace uagfx::charname
