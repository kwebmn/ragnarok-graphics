#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace uagfx::grounditems {

// 3.0.11-beta.283 (the owner's features 11 and 12): the items lying on the ground as the server tells them - 0x009d
// (in sight), 0x009e (just dropped): <id>.L <item>.W <identified>.B <x>.W <y>.W and the amount (0x009d at 13, 0x009e
// at 15); 0x00a1 <id>.L gone (picked up, or out of sight). A map's change forgets them all.
// 3.0.20-beta.44 (R-RDA885: names of items gone half an hour ago): the server tells an item gone only to those who see
// it then - after a warp within the same map, a respawn or a new login on it nobody tells. Entering a map (0x0073
// after the login, 0x0091 a warp, 0x0092 another map server's) forgets them all; and one not told again for
// kMostLife is forgotten by itself (uAthena keeps a dropped item 60 s, flooritem_lifetime: three times that)
constexpr uint64_t kMostLife = 180000;  // ms
// and one the player stands more than kOutOfSight cells from (either way; uAthena shows 40 round, area_size) - the
// server tells it again when it comes back into sight
constexpr int kOutOfSight = 60;
struct Item {
    uint32_t id = 0;
    int item = 0, x = 0, y = 0, amount = 1;
    bool identified = true;
    uint64_t seenMs = 0;  // when the server last told of it
};
class Ground {
public:
    void packet(uint16_t id, const uint8_t* p, uint64_t nowMs = 0);  // the whole packet, its id first
    void forgetOld(uint64_t nowMs);
    void forgetFar(int x, int y, int cells);  // the player's cell
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
