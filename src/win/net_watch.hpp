#pragma once
#include <cstdint>
#include <vector>

#include "core/net_watch.hpp"

namespace uagfx::win::netwatchwin {

// 3.0.11-beta.122 (the owner: «иногда бывают сетевые лаги ... персонаж и мобы телепортируются»; the network ideas 1,
// 2 and 7): the map server's connection - the game's own socket (its connection object + 4, the one the packet loop
// reads) with TCP_NODELAY set (uaRO.exe never sets it: its small commands waited for Nagle's algorithm, the server sets
// it on its side), read each second through SIO_TCP_INFO (Windows 10 1703 and later), the server's packets' pauses and
// its position fixes (0x0088: the character's own measured against where the game showed it) counted
// (core/net_watch); a line in the log each minute, a stall at once. On the game's thread.
void packet(void* connection, uint16_t id, const uint8_t* data);  // the packet tap's, each packet
void frame();
netwatch::Level level();
int rttMs();
std::vector<int> graph();  // 3.0.11-beta.127: the last minute's round trips (ms), the oldest first; -1 a stall

}  // namespace uagfx::win::netwatchwin
