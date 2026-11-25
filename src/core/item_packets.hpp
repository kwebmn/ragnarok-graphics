#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::itempackets {

// 3.0.24-beta.4 (docs/superpowers/specs/2026-11-25-items-via-packets-design.md, stage 1 «Склад»): the packets the
// driver sends for the things itself, in kRO Sakexe 2007-05-07's numbering (PACKETVER 7: uAthena's packet_db.txt rows
// for that client - movetokafra 0x0094 14 bytes, the index at 7, the amount at 10; movefromkafra 0x00f7 22, at 14
// and 18; closekafra 0x0193 2), and the server's packets' lengths (−1 their own, at +2; 0 not one of these)
using Bytes = std::vector<uint8_t>;
Bytes toStorage(uint16_t bagIndex, uint32_t amount);
Bytes fromStorage(uint16_t storageIndex, uint32_t amount);
Bytes closeStorage();
int serverLength(uint16_t id);

}  // namespace uagfx::itempackets
