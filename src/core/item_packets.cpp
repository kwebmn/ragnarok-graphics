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

int serverLength(uint16_t id) {
    switch (id) {
        case 0x01f0:  // the storage's stackables (18 bytes each from 4)
        case 0x00a6:  // the storage's equipment (20 each)
        case 0x01ee:  // the bag's stackables
        case 0x00a4:  // the bag's equipment
            return -1;
        case 0x00f2: return 6;   // the storage's count and most
        case 0x00f4: return 21;  // a thing into the storage
        case 0x00f6: return 8;   // a thing out of the storage
        case 0x00f8: return 2;   // the storage closed
        case 0x00a0: return 23;  // a thing into the bag
        case 0x00af: return 6;   // a thing out of the bag
        case 0x01c8: return 13;  // a thing used: what is left
        default: return 0;
    }
}

}  // namespace uagfx::itempackets
