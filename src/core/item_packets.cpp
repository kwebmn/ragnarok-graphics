#include "core/item_packets.hpp"

namespace uagfx::itempackets {
namespace {

void put16(Bytes& b, size_t at, uint16_t v) {
    b[at] = uint8_t(v), b[at + 1] = uint8_t(v >> 8);
}
void put32(Bytes& b, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[at + size_t(i)] = uint8_t(v >> (8 * i));
}

}  // namespace

Bytes toStorage(uint16_t bagIndex, uint32_t amount) {
    Bytes b(14, 0);
    put16(b, 0, 0x0094);
    put16(b, 7, bagIndex);
    put32(b, 10, amount);
    return b;
}

Bytes fromStorage(uint16_t storageIndex, uint32_t amount) {
    Bytes b(22, 0);
    put16(b, 0, 0x00f7);
    put16(b, 14, storageIndex);
    put32(b, 18, amount);
    return b;
}

Bytes closeStorage() { return Bytes{0x93, 0x01}; }

namespace {
Bytes eight(uint16_t id, uint16_t index, uint32_t amount) {
    Bytes b(8, 0);
    put16(b, 0, id);
    put16(b, 2, index);
    put32(b, 4, amount);
    return b;
}
}  // namespace

Bytes buyList(const Pairs& itemAmount) {
    Bytes b(4 + itemAmount.size() * 4, 0);
    put16(b, 0, 0x00c8);
    put16(b, 2, uint16_t(b.size()));
    for (size_t k = 0; k < itemAmount.size(); ++k) {
        put16(b, 4 + k * 4, itemAmount[k].second);  // the amount first
        put16(b, 6 + k * 4, itemAmount[k].first);
    }
    return b;
}

Bytes sellList(const Pairs& indexAmount) {
    Bytes b(4 + indexAmount.size() * 4, 0);
    put16(b, 0, 0x00c9);
    put16(b, 2, uint16_t(b.size()));
    for (size_t k = 0; k < indexAmount.size(); ++k) {
        put16(b, 4 + k * 4, indexAmount[k].first);
        put16(b, 6 + k * 4, indexAmount[k].second);
    }
    return b;
}

Bytes vendBuy(uint32_t merchant, const Pairs& placeAmount) {
    Bytes b(8 + placeAmount.size() * 4, 0);
    put16(b, 0, 0x0134);
    put16(b, 2, uint16_t(b.size()));
    put16(b, 4, uint16_t(merchant & 0xFFFF));
    put16(b, 6, uint16_t(merchant >> 16));
    for (size_t k = 0; k < placeAmount.size(); ++k) {
        put16(b, 8 + k * 4, placeAmount[k].second);  // the amount first
        put16(b, 10 + k * 4, placeAmount[k].first);
    }
    return b;
}

Bytes toCart(uint16_t bagIndex, uint32_t amount) { return eight(0x0126, bagIndex, amount); }
Bytes fromCart(uint16_t cartIndex, uint32_t amount) { return eight(0x0127, cartIndex, amount); }
Bytes storageToCart(uint16_t storageIndex, uint32_t amount) { return eight(0x0128, storageIndex, amount); }
Bytes cartToStorage(uint16_t cartIndex, uint32_t amount) { return eight(0x0129, cartIndex, amount); }

Bytes useItem(uint16_t index, uint32_t account) {
    Bytes b(14, 0);
    b[0] = 0x9f;
    b[4] = uint8_t(index), b[5] = uint8_t(index >> 8);
    for (int i = 0; i < 4; ++i) b[10 + size_t(i)] = uint8_t(account >> (8 * i));
    return b;
}

Bytes drop(uint16_t index, uint16_t amount) {
    Bytes b(10, 0);
    b[0] = 0x16, b[1] = 0x01;
    b[5] = uint8_t(index), b[6] = uint8_t(index >> 8);
    b[8] = uint8_t(amount), b[9] = uint8_t(amount >> 8);
    return b;
}

Bytes equip(uint16_t index, uint16_t places) {
    return Bytes{0xa9, 0x00, uint8_t(index), uint8_t(index >> 8), uint8_t(places), uint8_t(places >> 8)};
}

Bytes unequip(uint16_t index) { return Bytes{0xab, 0x00, uint8_t(index), uint8_t(index >> 8)}; }

int serverLength(uint16_t id) {
    switch (id) {
        case 0x01f0:  // the storage's stackables (18 bytes each from 4)
        case 0x00a6:  // the storage's equipment (20 each)
        case 0x01ee:  // the bag's stackables
        case 0x00a4:  // the bag's equipment
        case 0x01ef:  // 3.0.24-beta.8: the cart's stackables
        case 0x0122:  // the cart's equipment
            return -1;
        case 0x0121: return 14;  // the cart's count, most, weight, most weight
        case 0x0124: return 21;  // a thing into the cart
        case 0x0125: return 8;   // a thing out of the cart
        case 0x012b: return 2;   // the cart emptied
        case 0x00ca:             // 3.0.24-beta.9: the NPC deal's answer - bought
        case 0x00cb: return 3;   // sold
        case 0x0135: return 7;   // 3.0.24-beta.18: a vending purchase refused
        case 0x00f2: return 6;   // the storage's count and most
        case 0x00f4: return 21;  // a thing into the storage
        case 0x00f6: return 8;   // a thing out of the storage
        case 0x00f8: return 2;   // the storage closed
        case 0x00a0: return 23;  // a thing into the bag
        case 0x00af: return 6;   // a thing out of the bag
        case 0x01c8: return 13;  // a thing used: what is left
        case 0x00a8:             // 3.0.24-beta.20: a thing not used
        case 0x00aa:             // worn (or refused)
        case 0x00ac: return 7;   // taken off
        default: return 0;
    }
}

}  // namespace uagfx::itempackets
