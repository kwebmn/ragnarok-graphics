#include "core/game_packets.hpp"

#include <cstring>

namespace uagfx::packets {
namespace {

uint16_t rd16(const uint8_t* p, size_t at) { return uint16_t(p[at] | p[at + 1] << 8); }
uint32_t rd32(const uint8_t* p, size_t at) {
    return uint32_t(p[at]) | uint32_t(p[at + 1]) << 8 | uint32_t(p[at + 2]) << 16 | uint32_t(p[at + 3]) << 24;
}
// A name of 24 bytes to its end (or all 24)
std::string name(const uint8_t* p, size_t at) {
    const char* s = reinterpret_cast<const char*>(p + at);
    return std::string(s, strnlen(s, 24));
}

}  // namespace

Event parse(const uint8_t* p, size_t size) {
    if (!p || size < 2) return {};
    switch (rd16(p, 0)) {
        case 0x0097: {
            if (size < 28) return {};
            const size_t len = rd16(p, 2);
            if (len < 28 || len > size) return {};
            const char* t = reinterpret_cast<const char*>(p + 28);
            return Whisper{name(p, 4), std::string(t, strnlen(t, len - 28))};
        }
        case 0x00fe:
            if (size < 30) return {};
            return PartyInvite{rd32(p, 2), name(p, 6)};
        case 0x00e5:
            if (size < 26) return {};
            return TradeRequest{name(p, 2)};
        case 0x016a:
            if (size < 30) return {};
            return GuildInvite{rd32(p, 2), name(p, 6)};
        case 0x0207:
            if (size < 34) return {};
            return FriendRequest{rd32(p, 2), name(p, 10)};
        case 0x0196:
            if (size < 9) return {};
            return StatusIcon{rd16(p, 2), rd32(p, 4), p[8] != 0};
        case 0x0080:
            if (size < 7) return {};
            return Vanish{rd32(p, 2), p[6]};
        case 0x011a:
            if (size < 15) return {};
            return SkillCast{rd16(p, 2), rd16(p, 4), rd32(p, 6), rd32(p, 10)};
        default:
            return {};
    }
}

}  // namespace uagfx::packets
