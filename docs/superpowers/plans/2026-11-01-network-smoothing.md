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
- Step 4 (3.0.11-beta.124): research - the client already catches late moves up: 0x0086's server tick (packet + 12)
  goes with the decoded way (FUN_0057d420: src, sub-cell, dst) to the unit's message 0, and FUN_0062dbb0 keeps the
  session's server clock (+0x5E4): a move more than 144 ms late starts where the unit should be by now - a jump
  forward (after 4 late ones in a row the clock is moved back). Built: the jump faded instead - core/net_smooth
  easesJump (0.4-4 cells), Offset / offsetAt (an offset fading cubic-out; test net_smooth_a_moves_jump_fades);
  win/net_smooth moveSeen (0x0086 of another unit: where it is shown before the game takes it) -> at BeginScene the
  game's new place against it: a jump faded over 100 + 45 a cell ms, written over the game's place each frame (the
  game's place re-read when it walked it, kept when it stood). Ruling N4: the character's own moves (0x0087) not
  faded - the camera follows its place before BeginScene (the world would jump round a gliding character) - cost if
  wrong: the character's own jumps stay. Suite 2104/0.
- Numbers (R-4ZHJ3R, the owner, 18 min on c_tower2, .125): round trip 37-154 ms (about 50); 2 time-outs of OUR sent
  bytes (the uplink); own position fixes 5-29 a minute, nearly all 1 cell (uAthena unit_attack_timer_sub sends
  clif_fixpos for a player at each attack start) - glided by step 3; 1 late-move jump in 18 min. Ping plate with a
  graph 3.0.11-beta.127 (the owner's ask; confirmed).
- Step 5 (3.0.11-beta.128; the owner: «делать сеть, оно бывает глючит»): research - Windows delays the ACK of a lone
  segment up to 200 ms (TcpAckFrequency 2), so a thin stream's server sees round trips up to 200 ms too long: its RTO
  (SRTT + 4 RTTVAR, at least 200 ms) and its tail loss probe (2 SRTT + 200 ms with one packet in flight) late - a lost
  server packet holds the rest 0.5 s and more. Built: [debug] quick_acks (SIO_TCP_SET_ACK_FREQUENCY 1 on the game's
  socket; test config_debug_quick_acks); the server's held packets measured - 0x0086 (tick +12), 0x0087 (+2), 0x008a
  (+10) against the game's server clock (timeGetTime() - session +0x5E4, FUN_0062dbb0; the session 0x771148 by its
  callers' `mov ecx`), 250 ms late or more a hold (the late ones within a second one hold): netwatch late / held /
  holdMs, the minute's line «N holds by the server's lost packets (at most X ms)», «обрив» 5 s and a red bar on the
  graph (test net_watch_the_servers_held_packets). Suite 2112/0. Ruling N5: the own walk's prediction (the plan's
  step 5) not built - at 50 ms it would win 50 ms, and on every uplink stall (the felt glitch) it walks the character
  ahead of the server and pulls it back 1-2 cells; the ACKs and the measure attack the stalls themselves - cost if
  wrong: the own walk keeps its round trip's delay (offered to the owner as an opt-in).
- Step 6 (waits for the owner's leave to enter the game's VM, 192.168.122.16, Debian 13): proposed - sysctl
  net.ipv4.tcp_thin_linear_timeouts=1 (no doubling of the retry time on a thin stream); the clients' route (the VM's
  default via 192.168.122.1) with rto_min 80ms and quickack 1 (the server resends sooner, acknowledges the clients'
  packets at once - their own probes sooner). No restart (new connections take the route; the sysctl at once); each
  undone by one command; persisted in /etc/sysctl.d and the network config.
- Step 5's prediction (3.0.11-beta.130; the owner «делаем» to the opt-in offer - ruling N5 superseded): research - the
  client's walk request is 0x00a7 (the pad's session parses the game's sends: Session::takeWalkAsked); its answer
  0x0087 goes to FUN_0057e1f0 (this = the game mode; tick +2, the way +6 in x0 y0 x1 y1 10 bits + sub 4/4), which
  syncs the clock (FUN_0062dbb0) and tells the world's player (game mode +0xC4, +0x3C) its message 0 with seven dwords
  {tick, srcX, srcY, subX, subY, dstX, dstY} (the stack at ebp-0x1c..-0x4). Built: core/net_predict (readOwnMove,
  writeTick, Guess/answer, predicts within 20 cells, waitMs max(600, 4 RTT + 200); tests net_predict_*),
  win/net_predict (the guess at the next BeginScene when the player stood still 150 ms and has no cast gauge (+0x220,
  class 0x696C18); the 0x0087 of the same cells has its tick rewritten to the guess's before the game reads it; none in
  time - message 0x13 puts the player back; the server's answer before the guess cancels it), [interface]
  predict_walk OFF (test config_interface_predict_walk), session_hands_the_clients_walk_request_once. Suite 2120/0.
  Ruling N6: the joined answer's tick is the guess's (older by about a round trip) - with a round trip over 144 ms the
  game counts it late and after 4 in a row moves its server clock back 144 ms (any on-time packet resets the count) -
  cost if wrong: other units' moves read a little late after several own walks in a row on a slow line.
- R-6D43Y6 (owner, .130, cmd_fild02, the same session to 03:19): a real ~1 s network outage at 03:18:23 both ways (our
  TCP time-out + the server's packets held 1042 ms, fps steady 72); «held 914/726 ms» right after two reports'
  screenshots (fixed in .131). Prediction on: 1 joined (47 ms after the guess), 1 put back - five Seals hit the
  character at the click, it died 27 ms later. 3.0.11-beta.132: netpredict Act/readAct/holdUntil/heldNow (tests
  net_predict_the_servers_hold_on_the_walk, net_predict_reads_an_act) - no guess while the server holds the own walk
  (hit: tick + damage motion unless endured or missed; attacking: tick + attack motion; +50 ms), none and no put back
  for a dead character. Suite 2123/0.
- R-N2N7T8 (.132, stick walking): no guesses - the pad's own 0x00a7 go past the game's send (hooks::sendAll in
  flushPackets): 3.0.11-beta.133 flushPackets calls walkAsked too. R-U436CU (.132, mouse): 4 guesses - 2 joined (63
  ms), 2 «another way» (server src 1 and 4 cells from the shown cell: the client showing the character standing while
  the server walked it on); «held 352 ms» = the game back from 310 s behind other windows at 0.4 fps reading its
  backlog over several frames. 3.0.11-beta.134: netpredict walkEnds (10/14 a cell at 0x00b0 SP_SPEED, 150 untold;
  test net_predict_the_servers_walk_ends) - no guess until the server's last walk ends (+300 ms); the «another way»
  line names the walk before and the last own fix; netwatch inBacklog (300 ms gap -> 2 s of no holds; test
  net_watch_a_backlog_tells_no_hold). Suite 2125/0.
- R-DWCBHA (.134, a minute after login): a bad network minute - 3 TCP time-outs in 3 s (round trip 128 ms), the
  server's holds 288-1723 ms (fps 70.8: not the game); guesses confused (answers to older clicks while newer guesses
  waited); a bug: «still walks 1797700571 ms more» with no walk known (the server clock past 2^31: 0 + 300 looked
  ahead). 3.0.11-beta.135: netpredict walking (a walk known only) and inFlight (tests
  net_predict_no_guess_over_another_request); win: no guess while netwatch level is Stall or Bad, or while the request
  before waits for its answer. Suite 2126/0.
- R-JMHDUY (.135, 12 min cmd_fild02): 04:09-04:16 healthy (round trip ~60, no losses); from 04:17 degraded - round
  trip ~120 (peaks 317), bytes resent every minute, 3-6 server holds a minute up to 1159 ms. loki itself clean (0 %
  loss to 1.1.1.1/8.8.8.8/the VM; mtr to the player clean to Hetzner Warsaw, the ISP's hops silent) - the loss is past
  Warsaw. Guesses: 5 joined (47-250 ms), 6 «another way» (the shown cell short of the server's last walk's end: hit on
  the way), 1 put back (in a stall); skips worked (10 bad connection, 10 still walking, 7 in flight, 1 hold).
  3.0.11-beta.136: netpredict Placed/caughtUp (test net_predict_guess_only_where_the_server_has_it) - guessed only
  from the cell the server last put the character on (its last 0x0087's end, or a later own 0x0088); reset on another
  map. Suite 2127/0.
- R-QTN6TF (.136, 4 min, the line still lossy: 8 time-outs, ~18 server holds 0.3-1.1 s, round trip 79-143, peaks
  694): 5 joined (93-172 ms), 0 «another way» (caughtUp skipped one), 2 put back - both guessed in a calm moment, the
  request lost right after. 3.0.11-beta.137: netwatch lastLossMs (time-out, resent, hold; test net_watch_the_last_loss),
  netpredict calm (no loss 30 s, round trip < 150; test net_predict_only_on_a_calm_line) in place of the level check.
- R-VXZ36T (.137, 4 min): 1 joined (62 ms), 3 put back - 1.3 s after the map's entry (the server walks no one before
  the map is loaded) and twice in a 4 s uplink stall (the second click guessed before TCP_INFO showed the time-out).
  3.0.11-beta.138: netpredict settled (5 s on the map) and lastLoss (a put back is a loss: calm 30 s after it; test
  net_predict_settled_and_put_back_as_a_loss). Suite 2130/0.

