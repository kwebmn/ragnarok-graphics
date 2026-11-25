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
    const std::vector<Item>& l = w == Where::Bag ? bag_ : storage_;
    const auto at = std::find_if(l.begin(), l.end(), [index](const Item& x) { return x.index == index; });
    return at == l.end() ? nullptr : &*at;
}

void State::reset() {
    bag_.clear();
    storage_.clear();
    storageOpen_ = false;
    storageMost_ = 0;
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
        case 0x00f2:
            if (n < 6) return std::nullopt;
            storageOpen_ = true;
            storageMost_ = r16(p, 4);
            return std::nullopt;
        case 0x00f4: {
            if (n < 21) return std::nullopt;
            storageOpen_ = true;
            Item it;
            it.index = r16(p, 2);
            const uint32_t amount = r32(p, 4);
            it.id = r16(p, 8);
            it.identified = p[10] != 0;
            it.attribute = p[11];
            it.refine = p[12];
            cardsAt(p, 13, it);
            const Item* was = find(Where::Storage, it.index);
            it.amount = (was ? was->amount : 0) + amount;
            put(storage_, it);
            return Change{Where::Storage, it.index, int64_t(amount)};
        }
        case 0x00f6: {
            if (n < 8) return std::nullopt;
            const uint16_t index = r16(p, 2);
            const uint32_t amount = r32(p, 4);
            const auto at =
                std::find_if(storage_.begin(), storage_.end(), [index](const Item& x) { return x.index == index; });
            if (at != storage_.end()) {
                at->amount = at->amount > amount ? at->amount - amount : 0;
                if (at->amount == 0) storage_.erase(at);
            }
            return Change{Where::Storage, index, -int64_t(amount)};
        }
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
