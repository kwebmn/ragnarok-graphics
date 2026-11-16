#include "core/ground_items.hpp"

#include <cstring>
#include <iterator>

namespace uagfx::grounditems {
namespace {

uint16_t w16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
uint32_t l32(const uint8_t* p) { return uint32_t(p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24); }

}  // namespace

void Ground::packet(uint16_t id, const uint8_t* p, uint64_t nowMs) {
    if (id == 0x0073 || id == 0x0091 || id == 0x0092) {  // 3.0.20-beta.44: a map entered - none known there yet
        items_.clear();
        return;
    }
    if (!p) return;
    if (id == 0x009d || id == 0x009e) {
        Item it;
        it.id = l32(p + 2);
        it.item = w16(p + 6);
        it.identified = p[8] != 0;
        it.x = w16(p + 9);
        it.y = w16(p + 11);
        it.amount = w16(p + (id == 0x009d ? 13 : 15));
        if (it.amount <= 0) it.amount = 1;
        it.seenMs = nowMs;
        if (items_.size() > 3000) items_.clear();
        items_[it.id] = it;
    } else if (id == 0x00a1) {
        items_.erase(l32(p + 2));
    }
}

void Ground::forgetOld(uint64_t nowMs) {
    for (auto it = items_.begin(); it != items_.end();)
        it = nowMs > it->second.seenMs && nowMs - it->second.seenMs > kMostLife ? items_.erase(it) : std::next(it);
}

void Ground::forgetFar(int x, int y, int cells) {
    for (auto it = items_.begin(); it != items_.end();) {
        const int dx = it->second.x > x ? it->second.x - x : x - it->second.x;
        const int dy = it->second.y > y ? it->second.y - y : y - it->second.y;
        it = dx > cells || dy > cells ? items_.erase(it) : std::next(it);
    }
}

Worth worth(int type, int price) {
    if (type == 6) return Worth::Card;
    if (type == 4 || type == 5) return Worth::Gear;
    if (price >= kDear) return Worth::Dear;
    return Worth::Plain;
}

std::string label(const std::string& name, int amount) {
    return amount > 1 ? name + " \xC3\x97" + std::to_string(amount) : name;
}

}  // namespace uagfx::grounditems
