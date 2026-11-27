#include "core/friend_ids.hpp"

#include <cstring>

namespace uagfx::friendids {
namespace {

uint32_t rd32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

}  // namespace

void Ids::feed(uint16_t id, const uint8_t* p, size_t size) {
    if (!p) return;
    if (id == 0x0201 && size >= 4) {
        const size_t len = size_t(p[2]) | size_t(p[3]) << 8;
        accounts_.clear();
        for (size_t at = 4; at + 32 <= len && at + 32 <= size; at += 32) {
            const char* n = reinterpret_cast<const char*>(p + at + 8);
            const std::string name(n, strnlen(n, 24));
            if (!name.empty()) accounts_[name] = rd32(p + at);
        }
    } else if (id == 0x020a && size >= 10) {
        const uint32_t account = rd32(p + 2);
        for (auto it = accounts_.begin(); it != accounts_.end();)
            it = it->second == account ? accounts_.erase(it) : std::next(it);
    }
}

uint32_t Ids::accountOf(const std::string& name) const {
    const auto it = accounts_.find(name);
    return it == accounts_.end() ? 0 : it->second;
}

bool canInvite(bool inParty, bool online, bool member) { return inParty && online && !member; }

}  // namespace uagfx::friendids
