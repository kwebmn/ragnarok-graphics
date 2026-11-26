#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace uagfx::itemstate {

// 3.0.24-beta.4 (docs/superpowers/specs/2026-11-25-items-via-packets-design.md: the owner's way 2 - «состояние по
// пакетам сервера»): the bag and the storage as the server tells them, in uAthena's packets for PACKETVER 7
// (clif.c): the lists - stackables 0x01f0 (storage) / 0x01ee (bag), 18 bytes each from 4 (index, id, type, identified,
// amount, equip, cards); equipment 0x00a6 / 0x00a4, 20 each (index, id, type, identified, its places, worn,
// attribute, refine, cards) - the storage's count 0x00f2, a thing in 0x00f4 / out 0x00f6 / closed 0x00f8; the bag's
// thing in 0x00a0 (its result 0), out 0x00af, used 0x01c8 (what is left). The storage's lists come in parts: a part
// adds to what is known (the storage is known anew when it opens - its first list after a close).
struct Item {
    uint16_t index = 0;  // the server's (bag: place + 2, storage: place + 1)
    uint16_t id = 0;
    uint32_t amount = 0;
    bool identified = false;
    uint8_t refine = 0, attribute = 0;
    uint16_t cards[4] = {};
    uint16_t places = 0;  // equipment: where it is worn (0 a stackable)
};
enum class Where : uint8_t { Bag, Storage, Cart, Shop };  // 3.0.24-beta.8: the cart; .9: an NPC deal's answer
struct Change {
    Where where = Where::Bag;
    uint16_t index = 0;
    int64_t delta = 0;
    bool closed = false;  // the storage closed
    int result = -1;      // 3.0.24-beta.9: an NPC deal's answer (0x00ca / 0x00cb): 0 done, 1 zeny, 2 weight, 3 other
};

class State {
public:
    // A whole packet of the server's; what changed (for the actions waiting on it), none
    std::optional<Change> feed(const uint8_t* p, size_t n);
    const std::vector<Item>& bag() const { return bag_; }
    const std::vector<Item>& storage() const { return storage_; }
    // 3.0.24-beta.8: the cart - lists 0x01ef / 0x0122 (index place + 2), in 0x0124, out 0x0125, its count and
    // weight 0x0121, emptied 0x012b (a Change with `closed`)
    const std::vector<Item>& cart() const { return cart_; }
    int cartMost() const { return cartMost_; }
    int cartWeight() const { return cartWeight_; }
    int cartMostWeight() const { return cartMostWeight_; }
    const Item* find(Where w, uint16_t index) const;
    bool storageOpen() const { return storageOpen_; }
    int storageMost() const { return storageMost_; }
    void reset();  // a new map or connection: nothing known
    void clearCart() { cart_.clear(); }  // 3.0.24-beta.8: another map - its list comes again

private:
    std::optional<Change> moved(uint16_t kind, Where w, const uint8_t* p, size_t n);
    std::vector<Item>& list(Where w) { return w == Where::Bag ? bag_ : w == Where::Cart ? cart_ : storage_; }
    std::vector<Item> bag_, storage_, cart_;
    int cartMost_ = 0, cartWeight_ = 0, cartMostWeight_ = 0;
    bool storageOpen_ = false;
    int storageMost_ = 0;
};

}  // namespace uagfx::itemstate
