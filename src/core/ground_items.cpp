#include "core/ground_items.hpp"

#include <cstring>

namespace uagfx::grounditems {
namespace {

uint16_t w16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
uint32_t l32(const uint8_t* p) { return uint32_t(p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24); }

}  // namespace

void Ground::packet(uint16_t id, const uint8_t* p) {
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
        if (items_.size() > 3000) items_.clear();
        items_[it.id] = it;
    } else if (id == 0x00a1) {
        items_.erase(l32(p + 2));
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
