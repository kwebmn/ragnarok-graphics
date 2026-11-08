#include "core/upkeep.hpp"

#include <algorithm>
#include <cstring>

namespace uagfx::upkeep {
namespace {

uint16_t rd16(const uint8_t* p, size_t at) { return uint16_t(p[at] | p[at + 1] << 8); }
uint32_t rd32(const uint8_t* p, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, p + at, 4);
    return v;
}

}  // namespace

bool Watch::packet(uint16_t id, const uint8_t* p, size_t size) {
    if (!p) return false;
    const int petWas = pet_, homunWas = homun_;
    switch (id) {
        case 0x00a4: {
            if (size < 4) return false;
            const size_t len = std::min<size_t>(rd16(p, 2), size);
            std::set<int> now;
            for (size_t at = 4; at + 20 <= len; at += 20)
                if (p[at + 10] != 0) now.insert(rd16(p, at + 2));
            for (int item : now)
                if (listed_ && !broken_.count(item)) fresh_.push_back(item);
            const bool changed = now != broken_;
            broken_ = now;
            listed_ = true;
            return changed;
        }
        case 0x01a2:
            if (size >= 31) pet_ = rd16(p, 29);
            break;
        case 0x01a4:
            if (size < 11) return false;
            if (p[2] == 0) petUnit_ = rd32(p, 3);
            else if (p[2] == 2) pet_ = int(rd32(p, 7));
            break;
        case 0x022e:
            if (size < 31) return false;
            homun_ = (p[26] & 6) ? -1 : rd16(p, 29);
            break;
        case 0x0230:
            if (size < 12) return false;
            homunUnit_ = rd32(p, 4);
            if (rd16(p, 2) == 0x200) homun_ = int(rd32(p, 8));
            break;
        case 0x0080:
            if (size < 6) return false;
            if (petUnit_ && rd32(p, 2) == petUnit_) pet_ = -1, petUnit_ = 0;
            if (homunUnit_ && rd32(p, 2) == homunUnit_ && p[6] == 1) homun_ = -1, homunUnit_ = 0;  // died
            break;
        case 0x0091:
        case 0x0092:
            pet_ = homun_ = -1;
            break;
        default: return false;
    }
    return pet_ != petWas || homun_ != homunWas;
}

std::vector<int> Watch::takeFresh() {
    std::vector<int> out;
    out.swap(fresh_);
    return out;
}

uint32_t ownPetOf(uint16_t id, const uint8_t* p, size_t size, uint32_t known) {
    if (id != 0x01a4 || !p || size < 11 || known != 0 || p[2] == 0) return 0;
    return rd32(p, 3);
}

int petClassOf(uint16_t id, const uint8_t* p, size_t size, uint32_t& unit) {
    if (!p) return 0;
    if (id == 0x007c && size >= 22 && rd16(p, 14) == 100) {
        unit = rd32(p, 2);
        return rd16(p, 20);
    }
    if ((id == 0x0078 || id == 0x007b) && size >= 18 && rd16(p, 16) == 100) {
        unit = rd32(p, 2);
        return rd16(p, 14);
    }
    return 0;
}

int hungerLevel(int fullness) { return fullness < 0 ? 0 : fullness <= 10 ? 2 : fullness <= 25 ? 1 : 0; }

}  // namespace uagfx::upkeep
