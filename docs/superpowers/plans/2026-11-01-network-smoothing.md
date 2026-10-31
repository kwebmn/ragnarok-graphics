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
- Step 3 (3.0.11-beta.123): research - the client's 0x0088 (the packet loop 0x579f10): the character's - the world's
  player (world + 0x3C), another's - FUN_004ebd10(id); each told its message (vtable + 8, thiscall) 0x13 with the
  packet's x, y (buffer + 6, + 8) - put there, the walk stopped; 0x0086 tells message 0 with the decoded move
  (FUN_0057d420). Built: core/net_smooth (eases 1-3 cells, glideMs 120 + 40 a cell, eased cubic-out, at(), cellMiddle
  - tests net_smooth_which_fixes_glide, net_smooth_the_way_along, net_smooth_a_cells_middle), win/net_smooth (fix():
  the unit's world place (game_camera actorAddress, +4) against the packet's cell; near - the packet's cell made the
  one it stands on (the game stops it there, no jump), a glide to the server's cell; beginScene() from
  Device7::BeginScene - the place written along the way each frame, left to the game when it moved the unit
  elsewhere (not where we wrote, not the cell it was told), at the end the unit told 0x13 with the server's cell (the
  game's own state as the packet's would have been)); [interface] smooth_fixes (test config_interface_smooth_fixes);
  the watch's fixes now measured for every unit. Ruling N3: the end through the game's own 0x13 message from
  BeginScene (inside the frame's drawing, before any unit is drawn) - cost if wrong: a crash report naming
  net_smooth, the switch off meanwhile. Suite 2103/0.

