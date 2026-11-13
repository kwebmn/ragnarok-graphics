#include "win/item_lists.hpp"

#include <cstdint>
#include <cstdlib>

#include "win/game_memory.hpp"

namespace uagfx::win::itemlists {
namespace {

constexpr uint32_t kSession = 0x771148, kBag = kSession + 0xF1C, kCart = kSession + 0xF4C, kStorage = kSession + 0xF40;
constexpr uint32_t kInfo = 8, kAmmoBit = 0x8000;
constexpr size_t kInfoSize = 0x48, kMostNodes = 1000;

uint32_t at32(const uint8_t* p, size_t at) {
    return uint32_t(p[at]) | uint32_t(p[at + 1]) << 8 | uint32_t(p[at + 2]) << 16 | uint32_t(p[at + 3]) << 24;
}

}  // namespace

std::map<int, int> counts(List list, bool* known) {
    std::map<int, int> out;
    const uint32_t head = read32(list == List::Bag ? kBag : list == List::Cart ? kCart : kStorage);
    if (known) *known = head != 0;
    if (!head) return out;
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        uint8_t raw[kInfoSize] = {};
        if (!readGame(node + kInfo, raw, sizeof raw)) break;
        const uint32_t index = at32(raw, 0x08), worn = at32(raw, 0x0C), ptr = at32(raw, 0x30), len = at32(raw, 0x34);
        if (!index || !ptr || !len || len > 16) continue;
        if (list == List::Bag && worn && !(worn & kAmmoBit)) continue;  // the gear worn is not in the bag
        char digits[17] = {};
        if (!readGame(ptr, digits, len)) continue;
        const int id = std::atoi(digits), count = int(at32(raw, 0x10));
        if (id > 0) out[id] += count > 0 ? count : 1;
    }
    return out;
}

}  // namespace uagfx::win::itemlists
