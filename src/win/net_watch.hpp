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
// 3.0.11-beta.131: the game read its socket (its connection's poll, each loop) - the held packets' lateness counted
// less the time between two reads (the game standing still is not the server's hold)
void polled();
void frame();
netwatch::Level level();
int rttMs();
std::vector<int> graph();  // 3.0.11-beta.127: the last minute's round trips (ms), the oldest first; -1 a stall
// 3.0.11-beta.128 (the network plan's step 5; the owner: «делать сеть, оно бывает глючит»): [debug] quick_acks - the
// game's socket acknowledges each of the server's packets at once (SIO_TCP_SET_ACK_FREQUENCY 1; Windows waits up to
// 200 ms for a lone segment, so the server's round trip looks long and a packet it lost is sent again later); the
// server's held packets measured (a moving or hitting unit's packet late by its server tick against the game's clock)
void setQuickAcks(bool on);
uint32_t serverNow();  // 3.0.11-beta.130: the game's server clock - timeGetTime() less the session's +0x5E4
uint64_t lastLossMs();  // 3.0.11-beta.137: when a packet was last lost (GetTickCount64; 0 none yet)

}  // namespace uagfx::win::netwatchwin
