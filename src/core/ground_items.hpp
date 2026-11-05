#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace uagfx::grounditems {

// 3.0.11-beta.283 (the owner's features 11 and 12): the items lying on the ground as the server tells them - 0x009d
// (in sight), 0x009e (just dropped): <id>.L <item>.W <identified>.B <x>.W <y>.W and the amount (0x009d at 13, 0x009e
// at 15); 0x00a1 <id>.L gone (picked up, or out of sight). A map's change forgets them all.
struct Item {
    uint32_t id = 0;
    int item = 0, x = 0, y = 0, amount = 1;
    bool identified = true;
};
class Ground {
public:
    void packet(uint16_t id, const uint8_t* p);  // the whole packet, its id first
    void clear() { items_.clear(); }
    const std::map<uint32_t, Item>& items() const { return items_; }

private:
    std::map<uint32_t, Item> items_;
};

// How it stands out: a card (the pack's type 6) purple, a weapon or an armour (4, 5) blue, one worth kDear zeny or
// more a piece (the pack's price) gold; else plain - named only with Alt held
enum class Worth : uint8_t { Plain, Dear, Gear, Card };
constexpr int kDear = 10000;
Worth worth(int type, int price);
std::string label(const std::string& name, int amount);  // «Jellopy ×3», «Poring Card»

}  // namespace uagfx::grounditems
