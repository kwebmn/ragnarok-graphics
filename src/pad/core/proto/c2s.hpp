#pragma once
#include <cstdint>
#include <optional>

#include "pad/core/bytes.hpp"

namespace uapad {

// Client -> map server requests, packet_ver 22 layouts (uAthena db/packet_db.txt).
enum ActionType : uint8_t { kActionAttackOnce = 0, kActionSit = 2, kActionStand = 3, kActionAttack = 7 };

Bytes buildWalk(Cell to);                                            // 0x00a7 walktoxy
Bytes buildAction(uint32_t target, uint8_t type);                    // 0x0190 actionrequest
Bytes buildSkillToId(uint16_t skill, uint16_t lv, uint32_t target);  // 0x0072 useskilltoid
Bytes buildSkillToPos(uint16_t skill, uint16_t lv, Cell at);         // 0x0113 useskilltopos
Bytes buildTakeItem(uint32_t floorId);                               // 0x00f5 takeitem
Bytes buildUseItem(uint16_t index, uint32_t accountId);              // 0x009f useitem
Bytes buildNameRequest(uint32_t actor);                              // 0x008c getcharnamerequest
Bytes buildStopAttack();                                             // 0x0118 stopattack
Bytes buildNpcClick(uint32_t npc);                                   // 0x0090 npcclicked (0.7)

struct Hotkey {
    uint8_t type = 0;  // 0 item, 1 skill
    uint32_t id = 0;   // 0 = empty slot
    uint16_t lv = 0;
};
inline bool operator==(const Hotkey& a, const Hotkey& b) { return a.type == b.type && a.id == b.id && a.lv == b.lv; }

struct HotkeyChange {
    uint16_t index = 0;
    Hotkey key;
};

std::optional<uint32_t> parseWantToConnection(const Bytes& p);  // 0x009b -> account id
std::optional<uint32_t> parseWantToConnectionCharId(const Bytes& p);  // 0x009b -> character id
std::optional<HotkeyChange> parseHotkeyChange(const Bytes& p);  // 0x02ba
std::optional<Cell> parseWalkRequest(const Bytes& p);           // 0x00a7

}  // namespace uapad
