#pragma once
#include <cstdint>

namespace uapad {

// Packet length lookup. >0: fixed length, -1: variable (uint16 length at offset 2), 0: unknown id.
int s2cLength(uint16_t id);  // map server -> client: uAthena clif.c packet_len_table (PACKETVER 7)
int c2sLength(uint16_t id);  // client -> map server: uAthena db/packet_db.txt, packet_ver 22

}  // namespace uapad
