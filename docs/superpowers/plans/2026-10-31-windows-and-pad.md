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

## Ledger

- Stage 1: built as 3.0.11-beta.95 - core/game_packets ActorInfo (0x0195 HP %), core/mob_hp (Table seen/gone/shown,
  fillColour, barOver, percentText), win/mob_hp (packet, frame over readActorBoxes, drawFill bars + the target's per cent
  picture, under the cast and the cursor), the target card's hp bar + per cent, the chevron over the target's bar,
  [interface] mob_hp (wounded; app «Інтерфейс» + «Контролер», console), pad Session::setRing (no fake mark, the name
  request kept) from runtime setRing(target_ring != off); tests game_packets_a_monsters_name_line_brings_its_hp,
  mob_hp_* (4), target_card_hp_bar, config_mob_hp, session_ring_* (2); suite 2010/0.
