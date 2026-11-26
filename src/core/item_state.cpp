#include "core/item_state.hpp"

#include <algorithm>

namespace uagfx::itemstate {
namespace {

uint16_t r16(const uint8_t* p, size_t at) { return uint16_t(p[at] | (p[at + 1] << 8)); }
uint32_t r32(const uint8_t* p, size_t at) {
    return uint32_t(p[at]) | uint32_t(p[at + 1]) << 8 | uint32_t(p[at + 2]) << 16 | uint32_t(p[at + 3]) << 24;
}
void cardsAt(const uint8_t* p, size_t at, Item& it) {
    for (int i = 0; i < 4; ++i) it.cards[i] = r16(p, at + size_t(2 * i));
}
void put(std::vector<Item>& list, const Item& it) {
    const auto at = std::find_if(list.begin(), list.end(), [&](const Item& x) { return x.index == it.index; });
    if (at == list.end()) list.push_back(it);
    else *at = it;
}

}  // namespace

const Item* State::find(Where w, uint16_t index) const {
    const std::vector<Item>& l = w == Where::Bag ? bag_ : w == Where::Cart ? cart_ : storage_;
    const auto at = std::find_if(l.begin(), l.end(), [index](const Item& x) { return x.index == index; });
    return at == l.end() ? nullptr : &*at;
}

void State::reset() {
    bag_.clear();
    storage_.clear();
    cart_.clear();
    cartMost_ = cartWeight_ = cartMostWeight_ = 0;
    storageOpen_ = false;
    storageMost_ = 0;
}

// A thing in (0x00f4 / 0x0124: index, amount L, id, identified, attribute, refine, cards) or out (0x00f6 / 0x0125:
// index, amount L) of the storage or the cart
std::optional<Change> State::moved(uint16_t kind, Where w, const uint8_t* p, size_t n) {
    std::vector<Item>& l = list(w);
    if (kind == 0x00f4) {
        if (n < 21) return std::nullopt;
        Item it;
        it.index = r16(p, 2);
        const uint32_t amount = r32(p, 4);
        it.id = r16(p, 8);
        it.identified = p[10] != 0;
        it.attribute = p[11];
        it.refine = p[12];
        cardsAt(p, 13, it);
        const Item* was = find(w, it.index);
        it.amount = (was ? was->amount : 0) + amount;
        put(l, it);
        return Change{w, it.index, int64_t(amount)};
    }
    if (n < 8) return std::nullopt;
    const uint16_t index = r16(p, 2);
    const uint32_t amount = r32(p, 4);
    const auto at = std::find_if(l.begin(), l.end(), [index](const Item& x) { return x.index == index; });
    if (at != l.end()) {
        at->amount = at->amount > amount ? at->amount - amount : 0;
        if (at->amount == 0) l.erase(at);
    }
    return Change{w, index, -int64_t(amount)};
}

std::optional<Change> State::feed(const uint8_t* p, size_t n) {
    if (!p || n < 2) return std::nullopt;
    const uint16_t id = r16(p, 0);
    const auto listOf = [&](Where w, size_t each, bool equip) -> std::optional<Change> {
        if (n < 4) return std::nullopt;
        const size_t len = r16(p, 2);
        if (len < 4 || len > n || (len - 4) % each != 0) return std::nullopt;
        if (w == Where::Storage && !storageOpen_) {  // the first list after a close: the storage known anew
            storage_.clear();
            storageOpen_ = true;
        }
        for (size_t at = 4; at + each <= len; at += each) {
            Item it;
            it.index = r16(p, at);
            it.id = r16(p, at + 2);
            it.identified = p[at + 5] != 0;
            if (equip) {
                it.amount = 1;
                it.places = r16(p, at + 6);
                it.attribute = p[at + 10];
                it.refine = p[at + 11];
                cardsAt(p, at + 12, it);
            } else {
                it.amount = r16(p, at + 6);
                cardsAt(p, at + 10, it);
            }
            put(list(w), it);
        }
        return std::nullopt;
    };
    switch (id) {
        case 0x01f0: return listOf(Where::Storage, 18, false);
        case 0x00a6: return listOf(Where::Storage, 20, true);
        case 0x01ee: return listOf(Where::Bag, 18, false);
        case 0x00a4: return listOf(Where::Bag, 20, true);
        case 0x01ef: return listOf(Where::Cart, 18, false);  // 3.0.24-beta.8
        case 0x0122: return listOf(Where::Cart, 20, true);
        case 0x0121:
            if (n < 14) return std::nullopt;
            cartMost_ = r16(p, 4);
            cartWeight_ = int(r32(p, 6));
            cartMostWeight_ = int(r32(p, 10));
            return std::nullopt;
        case 0x00ca:
        case 0x00cb: {  // 3.0.24-beta.9: the NPC deal answered
            if (n < 3) return std::nullopt;
            Change c;
            c.where = Where::Shop;
            c.result = p[2];
            return c;
        }
        case 0x012b:
            cart_.clear();
            return Change{Where::Cart, 0, 0, true};
        case 0x0124:
        case 0x0125: return moved(id == 0x0124 ? 0x00f4 : 0x00f6, Where::Cart, p, n);
        case 0x00f2:
            if (n < 6) return std::nullopt;
            storageOpen_ = true;
            storageMost_ = r16(p, 4);
            return std::nullopt;
        case 0x00f4: {
            if (n < 21) return std::nullopt;
            storageOpen_ = true;
            return moved(0x00f4, Where::Storage, p, n);
        }
        case 0x00f6: return moved(0x00f6, Where::Storage, p, n);
        case 0x00f8:
            storageOpen_ = false;
            storage_.clear();
            return Change{Where::Storage, 0, 0, true};
        case 0x00a0: {
            if (n < 23 || p[22] != 0) return std::nullopt;  // refused: nothing came
            Item it;
            it.index = r16(p, 2);
            const uint16_t amount = r16(p, 4);
            it.id = r16(p, 6);
            it.identified = p[8] != 0;
            it.attribute = p[9];
            it.refine = p[10];
            cardsAt(p, 11, it);
            it.places = r16(p, 19);
            const Item* was = find(Where::Bag, it.index);
            it.amount = (was ? was->amount : 0) + amount;
            put(bag_, it);
            return Change{Where::Bag, it.index, int64_t(amount)};
        }
        case 0x00af: {
            if (n < 6) return std::nullopt;
            const uint16_t index = r16(p, 2), amount = r16(p, 4);
            const auto at = std::find_if(bag_.begin(), bag_.end(), [index](const Item& x) { return x.index == index; });
            if (at != bag_.end()) {
                at->amount = at->amount > amount ? at->amount - amount : 0;
                if (at->amount == 0) bag_.erase(at);
            }
            return Change{Where::Bag, index, -int64_t(amount)};
        }
        case 0x01c8: {
            if (n < 13 || p[12] == 0) return std::nullopt;  // not used
            const uint16_t index = r16(p, 2), left = r16(p, 10);
            const auto at = std::find_if(bag_.begin(), bag_.end(), [index](const Item& x) { return x.index == index; });
            if (at == bag_.end()) return std::nullopt;
            const int64_t delta = int64_t(left) - int64_t(at->amount);
            at->amount = left;
            if (left == 0) bag_.erase(at);
            return Change{Where::Bag, index, delta};
        }
        default: return std::nullopt;
    }
}

}  // namespace uagfx::itemstate
