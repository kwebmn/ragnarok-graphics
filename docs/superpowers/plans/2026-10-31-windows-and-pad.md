# The interface's windows and the controller in them - plan

> Executed inline (the owner 2026-10-31: «давай доделаем геймпад, доделаем окна интерфейсов и сделаем поддержку в
> них геймпада»), one beta per stage or part, no stops between stages; betas only (`$S/ship-beta.sh`, stage CMakeLists
> and docs yourself), the build gate (`$S/gate.sh`) before each.

**Goal:** finish the controller's part of the concept (monsters' HP bars, the ring in place of the fake mark) and the
new interface's remaining windows - «Навички», «Спорядження», the NPC talk, the shop, the storage, the trade - each
our own window in style A, driven by the mouse and by the controller.

**Spec:** Claude Docs «Концепция 2.3» https://claude.ai/artifact/NGSwk5vjux2rXQoWe2Yxgd - the archive tab's window
mockups («Снаряжение», «Навыки», «Разговор с Кафрой», «Магазин», «Склад и обмен») in concept 2.3's style rules (windows
are the panels, denser: opacity 90 %, a shadow only on windows); «Контроллер в интерфейсе uaRO»
https://claude.ai/artifact/3EatdWXW2sNZxyeHwa27Ve - «Полоски HP монстров» and the windows' button table.
The owner 31.10: «Полоски HP мобов - всем показывай», «Кольцо под целью - устраивает».

## Global constraints

- Every new switch in the app's «Інтерфейс» (+ the console); UI names from strings.cpp's uk column.
- No server change: our windows send what the game's own windows send, through the game's own functions or messages.
- Facts before code: each window's stage starts with its research in the decomp (the window's id and manager slot,
  its data in the session, the calls its buttons make); a log line proves each action path on Windows.
- The game's window hidden by its shown flag while ours shows (as «Речі» / «Характеристики»), back when ours is off.
- The controller in every window: the D-pad or the left stick moves a gold frame, A chooses, X assigns to a button,
  Y the card, LB / RB the tabs, B or Start close; the hint line says the window's buttons.
- `near` / `far` never identifiers; no std::lround / std::hypot in per-pixel loops; lines up to 120 characters.

## Stages

1. **HP bars + the ring in place of the mark.** core/game_packets 0x0195 (`ActorInfo`: id, HP %), core/mob_hp (who
   shows a bar: off / the target / the target and the wounded for 8 s after their last change / all known; forgotten
   at 0x0080; bar colours green, red below 25 %; the target's bar wider with «57%»), win/mob_hp (over each sprite's box
   from the pick list, the target's percent as a small picture), the target card's HP bar + percent; [interface] mob_hp
   = off|target|wounded|all (wounded); app «Смужки HP монстрів». Pad: Session::setRing - with the ring drawn no fake
   mark, the name request on a new target stays (it refreshes the HP %).
2. **«Навички».** Research: window 0x25 (mgr + 0x118), the session's skill list (+0x2D8), the raise request, skill
   use as the hotbar does. Our window: tabs by job (the AEGIS prefix), rows (icon, name, type / SP / range, level
   dots, «+»), free points, drag to the bar; pad: rows, A uses, Y raises, X assigns, LB / RB tabs.
3. **«Спорядження».** Research: window 10 (mgr + 0x94), worn items (ITEM_INFO +0xC), wear / take off calls. Slots
   around the character (head top / mid / low, armour, weapon, shield, garment, shoes, two accessories, ammo), card
   dots, drag from «Речі»; pad: slots, A takes off or lists what fits to put on, Y the card.
4. **NPC talk.** Research: the say / menu / next / close / number windows (pad client_npc facts), cut-ins (0x01b3).
   Our window: name and map, the text with its colours, numbered choices in two columns, the cut-in beside; keys 1-8,
   Enter, Esc; pad as the NPC hints say.
5. **Shop.** Research: 0x00c4 / 0x00c6 / 0x00c7 and the game's buy / sell windows. List and cart, totals, zeny and
   weight after; pad: list, A adds, quantity on the D-pad, X buys.
6. **Storage.** Research: the storage window and its add / take calls. Tabs, search, the slots' count, grid as
   «Речі»; pad as «Речі».
7. **Trade.** Research: the trade window, add item / zeny, ok, trade. Two columns, zeny apart, changes after
   «Готово» in red; pad.

## Rulings

- R1: the HP bars' default «Ціль і поранені» (the concept's «Когда видна»); «всем» = every player, with or without the
  controller. Cost if wrong: one default in the app.
- R2: with the ring drawn the pad's fake mark is off, its menu choice kept for the ring off; the name request on a new
  target stays (it brings the target's HP %). Cost if wrong: one beta.
- R3: the windows' order by how often a player opens them (skills, equipment, NPC talk, shop, storage, trade).
- R4 (the owner 31.10: «а можно чтобы над мобами всегда хп висело?»): [interface] mob_hp's default «Усі», and «Усі»
  asks the server for each monster on the screen not known (the game's own name request FUN_00560650, 8 a second, the
  same one after 15 s). Cost if wrong: one default and a few packets a second.
- R5: the shop opens on «Купити» at once (the deal's choice pressed for the player, «Продати» a tab: the game's cancel,
  then the game mode's 0x28 - the packet 0x00c5 again). Cost if wrong: one round trip on «Продати».

## Ledger

- Stage 1: built as 3.0.11-beta.95 - core/game_packets ActorInfo (0x0195 HP %), core/mob_hp (Table seen/gone/shown,
  fillColour, barOver, percentText), win/mob_hp (packet, frame over readActorBoxes, drawFill bars + the target's per cent
  picture, under the cast and the cursor), the target card's hp bar + per cent, the chevron over the target's bar,
  [interface] mob_hp (wounded; app «Інтерфейс» + «Контролер», console), pad Session::setRing (no fake mark, the name
  request kept) from runtime setRing(target_ring != off); tests game_packets_a_monsters_name_line_brings_its_hp,
  mob_hp_* (4), target_card_hp_bar, config_mob_hp, session_ring_* (2); suite 2010/0.
- Stage 2: built as 3.0.11-beta.96 - research: window 0x25 (mgr + 0x118, class 0x69B9D0, SendMsg 0x4BDD90: 6/0x87
  close, 6/0xAD.. «+» -> mode 0x4A (packet 0x0112, mode[0x42] guards), 0x3A use -> mode 0x7E (record, use level), press
  -> mode 0x51 (id, AEGIS name, use level: drag type 8)), use levels session + 0x534 by id, the job session + 0xE44.
  core/skill_tree (+ tools/skill_tree.py -> skill_tree.inc from db/skill_tree.txt: most levels, chains by job, home
  job, needs), core/skill_window (tabs, rows, dots, «+», use arrows, scroll, pad footer), win/skill_window (session's
  list, hold the game's window, mouse: dbl-click use, press drag, «+», ‹ ›, wheel, title), [interface] skills (on with
  the minimap), pad skillsPad (A use, Y raise, X assign, LB/RB, ←→, B/Start). Tests skill_tree_* (3),
  skill_window_* (5), config_interface_minimap updated; suite 2018/0.
- Stage 3: built as 3.0.11-beta.97 - research: window 10 (mgr + 0x94, class 0x698668, SendMsg 0x470F90 6/0x87 close,
  drop 0x46F550 wears by location: mode 0x14 (index, location), ammo types 10/16/17 mode 100), dbl-click take off =
  mode 0x15 (the worn bit's number, FUN_00628bd0; 0x8000 -> 15), press = mode 0x16 (worn location: drag type 2);
  ITEM_INFO cards +0x1C..+0x28 (0x00a4 handler FUN_00582420), refine +0x40; card slots from the reference pack.
  core/equip_window (slots by bits, two columns + centre, card dots, pick list, pad footer), win/equip_window (bag,
  portrait via headpicture, stats, take off / drag / card / drop / pick and wear), [interface] equip, Blocker::Equip +
  world_mouse drop, pad equipPad. Tests equip_window_* (3), config_interface_minimap updated; suite 2021/0.
- Stage 4: built as 3.0.11-beta.98 - research: 0x00b4 -> window 0x10 SendMsg 0x1A (text, npc; +0x88 set = a Next
  pressed: the next text clears the page), 0x00b7 -> window 0x11 (strtok ':' options via 0x1B, rows' option numbers
  vector +0x68..+0x6C, list +0x74 chosen +0x64; OK 6/0x76 -> mode 0x19 (number+1), cancel 0x77). core/game_packets
  NpcSay / NpcMenu, core/npc_talk (^RRGGBB runs, dark ones lit, «[Name]», wrap, two-column numbered options, button,
  keys / pad footer), win/npc_talk (words from packets, game windows hidden, choose = list +0x64 then the menu's OK via
  the pad's clientnpc, keys 1-9 / Enter / Esc / arrows while the chat line is empty (chatwin::inputEmpty), the number
  field kept the game's), the pad hint line hidden while it shows, [interface] npc_talk. Tests
  game_packets_an_npcs_words_and_menu, npc_talk_* (3); suite 2025/0.
- Stage 1 follow-up: built as 3.0.11-beta.99 (the owner: «hp для мобов должно быть доступно и в обычном режиме») - the
  bars' target with the mouse no longer needed the target card shown (targetpanel::current() was 0 with the card off,
  no reference pack or an unknown monster: no wide bar, «Лише ціль» empty): core/mob_hp pointed() (the controller's
  target; with the mouse the monster under the cursor while its per cent is known; else targetpanel::fighting(), the
  one the character fights, card on or not), win/world_mouse pointedActor() (the pick's record id +0x18 at 0x566DFA,
  not under a window: the frame's answer at 0x566C9A != 1), bars moved where the world ends (record.firstInterface():
  under the game's windows, names, cursor), the chevron over any bar, the ring «Завжди» on fighting(); the row only in
  «Інтерфейс» (off the «Контролер» page), its hint says mouse and controller alike. Test
  mob_hp_the_target_with_the_mouse_and_with_the_controller; suite 2026/0.
- Stage 5: built as 3.0.11-beta.100 - core/shop_window (Ware/Line, put within the most, sums: total, zeny after,
  weight after in tenths, ok; shownOf, scrollFor, matchShop by wares or the nearest within 15 cells; layout 1000 wide:
  tabs, search, list 520 with price and weight, cart with − n +, totals, «Купити · Enter», «Очистити», note, pad
  footer and LB/RB at the tabs), win/shop_window (windows 0x16/0x17/0x18/0x19 held hidden; the chooser's 0x9E pressed in
  beforeGameDraw; the deal list +0xF28 read every 200 ms: price +0x18, bag count +0x10 when sold; confirm: the ITEM_INFO's
  +0x10 set, FUN_00629750 / FUN_0062a4b0 (bytes checked), the cart's 6/0x76; cancel 0x77; the other tab: cancel + mode
  0x28/0x29 with the NPC from the chooser's +0x60, «loading» up to 3 s; mouse, keys (Enter, Esc, the search), pad
  shopPad), [interface] shop (on with the minimap), Blocker::Shop, pad hint hidden. HP bars «Усі» by default with the
  asker (core/mob_hp Asker, win/mob_hp beforeGameDraw; readActorBoxes classes +0x1C). Tests shop_window_* (6),
  config_interface_shop, mob_hp_asks_the_server_for_the_monsters_not_known; config_mob_hp, config_interface_minimap
  updated; suite 2034/0.
- Stage 5 research: 0x00c4 deal type; 0x00c6 buy list (11 B each: price.L discount.L type.B nameid.W)
  -> session deal list +0xF28 (count +0xF2C, FUN_0062bbb0(rec, 0); FUN_004ffd30 copies the record's text at +0x11 into
  m_itemName) + windows 0x16 (mgr+0xEC, class 0x69B3E0, SendMsg 0x4AFC00: 0x24 0 buy / 1 sell) and 0x17 (buy cart,
  mgr+0xF0, class 0x69B5A8, SendMsg 0x4B35D0: 6/0x76 OK -> when [0x28]==0 mode 0x25 = packet 0x00c8 from the purchase
  list +0xF58 (count +0xF5C; FUN_00629650 reads, FUN_00629750(session, ITEM_INFO*) adds by name, its +0x10 the count),
  6/0x77 cancel -> close 0x16/0x17, mode 0x2B, clear FUN_0061e3d0). 0x00c7 sell list (10 B: index.W price.L
  overcharge.L) -> FUN_0062bbb0(rec, 1) into +0xF28 too, windows 0x16 (0x24 1) and 0x18 (sell cart, mgr+0xF4, class
  0x69B6D8, SendMsg 0x4B6B70: 6/0x76 -> FUN_0062a390() != 0 then mode 0x26 = packet 0x00c9 from the sell list +0xF34
  (count +0xF38; FUN_0062a4b0(session, ITEM_INFO*) adds by bag index +8, count +0x10 capped at the bag's)). Plan: our
  window fills those lists through FUN_00629750 / FUN_0062a4b0 (the deal record's count +0x10 set for the call and put
  back) and presses the cart's own OK; prices: the deal record's fields still to map (FUN_0062bbb0 copies).
