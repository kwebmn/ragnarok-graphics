#include "pad/core/proto/c2s.hpp"

namespace uapad {

Bytes buildWalk(Cell to) {
    Bytes b(8, 0);
    wr16(b, 0, 0x00a7);
    encodePos3(b, 5, to, 0);
    return b;
}

Bytes buildAction(uint32_t target, uint8_t type) {
    Bytes b(19, 0);
    wr16(b, 0, 0x0190);
    wr32(b, 5, target);
    wr8(b, 18, type);
    return b;
}

Bytes buildSkillToId(uint16_t skill, uint16_t lv, uint32_t target) {
    Bytes b(25, 0);
    wr16(b, 0, 0x0072);
    wr16(b, 6, lv);
    wr16(b, 10, skill);
    wr32(b, 21, target);
    return b;
}

Bytes buildSkillToPos(uint16_t skill, uint16_t lv, Cell at) {
    Bytes b(22, 0);
    wr16(b, 0, 0x0113);
    wr16(b, 5, lv);
    wr16(b, 9, skill);
    wr16(b, 12, uint16_t(at.x));
    wr16(b, 20, uint16_t(at.y));
    return b;
}

Bytes buildTakeItem(uint32_t floorId) {
    Bytes b(8, 0);
    wr16(b, 0, 0x00f5);
    wr32(b, 4, floorId);
    return b;
}

Bytes buildUseItem(uint16_t index, uint32_t accountId) {
    Bytes b(14, 0);
    wr16(b, 0, 0x009f);
    wr16(b, 4, index);
    wr32(b, 10, accountId);
    return b;
}

Bytes buildNpcClick(uint32_t npc) {
    Bytes b(7, 0);
    wr16(b, 0, 0x0090);
    wr32(b, 2, npc);
    wr8(b, 6, 1);
    return b;
}

Bytes buildNameRequest(uint32_t actor) {
    Bytes b(11, 0);
    wr16(b, 0, 0x008c);
    wr32(b, 7, actor);
    return b;
}

Bytes buildStopAttack() {
    Bytes b(2, 0);
    wr16(b, 0, 0x0118);
    return b;
}

std::optional<uint32_t> parseWantToConnection(const Bytes& p) {
    if (p.size() < 26 || rd16(p.data(), 0) != 0x009b) return std::nullopt;
    return rd32(p.data(), 4);
}

std::optional<uint32_t> parseWantToConnectionCharId(const Bytes& p) {
    if (p.size() < 26 || rd16(p.data(), 0) != 0x009b) return std::nullopt;
    return rd32(p.data(), 9);
}

std::optional<HotkeyChange> parseHotkeyChange(const Bytes& p) {
    if (p.size() < 11 || rd16(p.data(), 0) != 0x02ba) return std::nullopt;
    HotkeyChange c;
    c.index = rd16(p.data(), 2);
    c.key.type = rd8(p.data(), 4);
    c.key.id = rd32(p.data(), 5);
    c.key.lv = rd16(p.data(), 9);
    return c;
}

std::optional<Cell> parseWalkRequest(const Bytes& p) {
    if (p.size() < 8 || rd16(p.data(), 0) != 0x00a7) return std::nullopt;
    return decodePos3(p.data(), 5);
}

}  // namespace uapad
