# The network's jumps - plan

> Executed inline (the owner 2026-11-01: «иногда бывают сетевые лаги ... ощущается какбуто персонаж и мобы
> телепортируются ... можешь предложить какието идеи?» -> the ideas below -> «начинай по очереди»), one beta per step,
> no stops between steps; betas only (`$S/ship-beta.sh`), the build gate (`$S/gate.sh`) before each.

**Goal:** the character and the monsters no longer jump about on a slow or lossy connection - measured first, then the
server's position fixes eased, the late moves caught up, maybe the own walk started at once.

**Facts (2026-11-01):** uaRO.exe never sets TCP_NODELAY (no setsockopt in it; only ioctlsocket FIONBIO in its
connect FUN_00418af0, the socket at its connection object + 4); uAthena sets it on its side (socket.c). The server
puts units on a cell (clif_fixpos, 0x0088) before each attack of a unit that chased its target («Aegis uses one
before every attack»), when a walking monster casts, when a walk stops (unit.c 208, 249, 435, 451, 671, 1426); the
client shows units only after the server's packets (no prediction), so they lag a round trip and jump to the fixed
cell. RO's TCP: one lost packet holds the rest until it is sent again (0.2-1 s on Windows) - all jump at once after.

## Steps

1. (+2, +7) **Measure, NODELAY, the badge.** core/net_watch (Watch: TCP readings - the round trip, the bytes sent
   again, the time-outs; the server's packets' pauses; the position fixes; the badge's level; a log line a minute),
   win/net_watch (the game's socket from its connection object, TCP_NODELAY set, SIO_TCP_INFO each second, 0x0088 for
   the own character measured against its cell), [interface] net_badge - «● 45 мс» on the minimap's bar.
3. **Ease the fixes.** A fix of 1-3 cells walked quickly instead of a jump (the game's own move code); farther ones as
   before (a teleport, a knock-back). Research first: the client's 0x0088 handler and its actors' move functions.
4. **Catch up late moves.** A move packet's server tick against the synced clock: a late one walked on from where it
   should be by now. Research first: does the client use the tick already.
5. **The own walk at once** (prediction) - after 3, with the numbers.
6. **The server** (uAthena: the fix before each attack; Linux thin-stream time-outs) - only with the owner's leave
   (the game's VM).

## Ledger

- Step 1 (3.0.11-beta.122): core/net_watch + tests net_watch_levels_by_the_round_trip,
  net_watch_stalls_and_resent_bytes, net_watch_the_logs_line; win/net_watch (TCP_INFO_v0 declared locally with
  MSVC's layout - mstcpip.h keeps it for Windows 10 1703 builds; ws2_32's setsockopt / WSAIoctl by GetProcAddress -
  no new import in ddraw.dll), fed by win/packet_tap's take for each packet; the badge: minimap Texts.net / netRgb
  (test minimap_proto_net_badge_on_the_bar), [interface] net_badge (test config_interface_net_badge). Levels: under
  120 ms green, under 250 ms amber, red above or bytes sent again in the last 10 s, «обрив» red 5 s after a TCP
  time-out. Ruling N1: TCP_NODELAY set without a switch (the server sets it too; a standard option, nothing for a
  player to choose) - cost if wrong: a beta to take it back. Ruling N2: the others' fixes counted without a distance
  (their cell before it not read yet) - step 3's research reads the actors. Suite 2099/0.
