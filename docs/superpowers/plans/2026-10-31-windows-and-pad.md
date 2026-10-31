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
- Stage 5 follow-up: built as 3.0.11-beta.101 after R-2REP95 (the owner on beta.100: «немого не понял фразу Buy A -
  enter» - the button's «·» written by a Python edit as «Â·»; «чтобы в диалоге покупки работали также кнопки на
  клавиатуре верх вниз, также чтобы можно было легко вернуть выбраную на продажу вещь, чтобы если что можно было ввести
  количество с клавиатуры. продумай хорошо механизм», «и чтобы описание вещи можно было посмотреть»). core/shop_window:
  mostFor (sold: the bag's; bought: the zeny and the weight left beside the other rows, <= 30000), put/takeBack on Data,
  Edit (typeDigit/eraseDigit/endEdit/cancelEdit: the first digit replaces, kept within the most, Esc back as it was),
  HoldRepeat (1, then 1 per 80 ms after 0.4 s, 10 after 1.5 s), the cart row − [amount field] + sum ✕, the confirm
  «Купити» with an «Enter» key cap (pad: Start), the footer's key caps (↑↓ ←→ Space 0–9 Del F1 Esc), the frame
  (Show.cartFocus/ware/line/framed). win/shop_window: keys (arrows, PgUp/PgDn/Home/End, ←→/Tab, Space/+/−, digits, Del,
  F1, Enter, Esc; the chat line empty; the next WM_CHAR eaten), the right click and F1/pad Y = the game's item card
  (0xC, mgr+0xC8, Move vtable+0x18 beside the shop), the amount clicked to type, ✕ takes back; pad A/X held, LT takes
  back, Y card, Start buys, B closes typing/card/deal. Ruling R6: right click = the description (as the game's shop),
  −1 by Ctrl+click or the cart's −; R7: pad Y = description as in «Речі»/«Спорядження», Start = buy. Tests
  shop_window_* (9); suite 2037/0.
- Walking (3.0.11-beta.102; the owner: «проработал передвижение с контролером, чтобы стрелочка показывалась как в
  концепции и оно не глючило при хождении ... если маштаб маленький то были рывки ... а если большой маштаб то шел
  дальше чем нужно»): cause - the mouse mode held the cursor a fixed number of pixels out (cells by the zoom). New
  pad/core/logic/stick_walk (wayOf by the camera's look, lead 3..6 by SP_SPEED (600 ms + 1), destination along the way
  stopping at walls, sliding ±45° or along an axis when blocked at once, stopCell = the next cell, askAgain: first,
  turn > 22.5° or the destination moved aside, within lead/2 of the last ask, 1 s refresh, never the same cell, 100 ms
  gap); Controller::walkDirect on it with setSelfCell (the game's cell, readPlayerCell); core/ground_way walkArrowShape
  (shaft, gold-edged head at the cell's near edge, gold cell frame, on the .gat heights) + placeTris; win/ground_marks
  heights cache; FrameRecord::dropCommand + the game's cursor hidden while the pad plays in arrow mode; [interface]
  pad_walk = arrow by default (dist template too). Ruling R8: the arrow's length is the speed-based lead, not the
  [stick] walk_radius of the concept (the radius was screen pixels - the cause). Tests stick_walk_* (6),
  ctl_direct_walk_* (3, updated), ground_way_walk_arrow_on_the_ground, frame_drops_one_command, config_pad_walk;
  suite 2045/0.
- Arrow looks (3.0.11-beta.103; the owner: «можешь сделать настройку, чтобы можно было выбрать эту стрелку в
  конфигураторе?» after the doc «Стрелка хода с контроллера - варианты», artifact 28xJTGnGqirNpwVXj5jNbP):
  [interface] walk_arrow = chevrons|slim|concept|ring|dots|cell|off (WalkLook; schema row Page::Interface Choice, the
  pad page's row after pad_walk, the console's sub-row, live). core/ground_way walkShape(look, WalkArrow, ...): Pen
  passes (a shade first - each shape grown 0.06 cell, black at half its alpha, heads and fills left out - then the
  light one), Frame along the way; the mockups' sizes (tools/render.py of the doc); Tri gains `part`. stick_walk
  destination(..., bool* cut) = the march stopped by a cell that does not walk; Controller walkWay()/walkCut();
  runtime walkWay(). win/ground_marks: the way turned to the cell softly (80 ms), fade in 120 / out 200 ms, the
  chevrons' phase 600 ms; log «walk arrow: the look X», «walk arrow: X - N triangles on the ground». Ruling R9: the
  default is the doc's advice (chevrons) - the owner did not answer its questions but asked for the choice; cost if
  wrong: one default to change. Ruling R10: a short way (span < 0.3 cell) gets one still chevron when the cell is
  1.6+ cells away (else the diamond alone) - checked offline (arrowcheck render over the doc's screenshot). Tests
  config_walk_arrow, ground_way_walk_looks, stick_walk_tells_a_wall_cutting_the_way_short,
  ctl_direct_walk_tells_the_arrow_its_way_and_a_wall; suite 2049/0.
- Arrow shake (3.0.11-beta.104; the owner: «стрелочки очень дергаются при движении»): cause found offline (scratch
  walksim on the real Controller + a server-like walker, 150/210 ms steps): walkTo_ = the game's cell + the lead,
  recomputed every frame, hopped a cell on each time the character's cell changed (6-7/s) while the feet glided to it
  - the end shook 0.19 cells a frame on average (peaks 0.9-1.3), the way to the rounded cell off by 6-7° at 30°. Fix:
  core/ground_way glide (WalkAim/WalkGlide): along the march's way (stick_walk destination `along`, a slide's along a
  wall), as long as the lead (Controller walkLead), a wall: up to the asked cell projected on the way, its end kept on
  the ground as the feet go on (no easing lag into the wall), turn 80 ms / length 100 ms eases, a warp taken anew;
  WalkArrow.len replaces the cell; ground_marks: QueryPerformanceCounter clock, a per-walk smoothness log line. Model
  after: 0 cells a frame. Tests ground_way_walk_glide_holds_still_against_the_feet, walk_glide_holds_still_with_the_
  controller, walk_glide_ends_at_a_wall_with_the_controller, ctl_direct_walk_tells_the_way_it_marches_and_the_lead,
  stick_walk_tells_the_way_it_marched; suite 2054/0. Ruling R11: the end mark glides (not snapped to the cell under
  the end: that ticks a cell every 150 ms); cost if wrong: «Лише клітинка» shows a cell-sized square, not a cell.
- End mark switch (3.0.11-beta.105; the owner: «а можно клетку куда идти над стрелкой не рисовать?»): [interface]
  walk_mark (Check, default 0; «Клітинка, куди йти» under «Стрілка ходу» in «Інтерфейс», on «Контролер», the console's
  sub-row, live) - WalkArrow.mark, each look's end mark (chevrons' diamond, slim's corners+fill, concept's cell frame+
  fill, ring's and dots' small rings) and its shade left out; the flat fallback's ring too. Ruling R12: off by default
  (the owner asked not to draw it; every part its own box, so it can come back); «Лише клітинка» keeps its cell (the
  look itself) - cost if wrong: one more condition. Tests config_walk_mark, ground_way_walk_looks_without_the_mark;
  suite 2056/0.
- Reports R-WCXUQP / R-JDFY25 / R-N6M9DY (3.0.11-beta.106; owner «проверь отчеты»): (1) skills not dragged onto the bar
  - win/skill_window began the game's drag (0x51) at the press, the press/release being the window's the game never
  saw the button up (no «drag let go» line at all in the log); now core/skill_window Press (drag past 10 px x scale)
  + the icon under the cursor, on release 0x51 then 0x12 at the game's cursor (as item_window since beta.43); (2) the
  pad's routing was a fixed chain (items first) - pad core WindowOrder::raise + runtime `own` order (OwnWindow) + a
  taken press in window_switch raises; (3) the arrow white on Prontera's white paving - shade grow 0.06 -> 0.10 cell,
  dark 0.5 -> 0.75 (checked offline over the report's own shot and the doc's grass); (4) a crash 1 ms after
  «DirectDraw released, renderer closed» (DINPUT.dll+0x8ade at exit) counted FailedEarly - safe_mode exitCrash: a
  crash after the last opening's close is the game's exit (Fine). The DINPUT crash itself: once in all reports, the
  system dll of Windows 26300, no dump - not chased. Tests window_order_a_click_raises_an_open_window,
  skill_window_a_press_becomes_a_drag_past_ten_pixels, ground_way_walk_shade_reads_on_white_ground,
  safe_mode_a_crash_after_the_renderer_closed_is_the_games_exit; suite 2060/0.
- Windows and inventory concept (owner 31.10: «я сделал стрелы и они в инвентаре не появились», «окна одно поверх
  другого не появляются», «где окно еквпировки чара то вообще ничего не появилось» -> «давай подумаем над этим всем в
  виде концепции окон и инвентаря?»): Claude Docs GV5qKobJpoTV6bT5oewBu5 «Концепция окон и инвентаря» (facts: draw
  order items<stats<skills<equip<npc<shop vs click order stats,skills,equip,shop,npc,items; our windows over the
  game's; ammo -> «Різне» per FUN_004ba410; no «new» mark; equip reads the bag +0xC); proposed: one window stack,
  «Персонаж» (gear + bag), a drag layer, arrows end to end; 8 questions - waiting for answers. 3.0.11-beta.107:
  diagnostics only - packets::Pickup gains index/type (0x00a0 bytes 2, 21), itemswin::packet logs each item come into
  the bag and its tab. Test game_packets_an_item_into_the_bag_tells_its_kind; suite 2061/0.
- Concept answers 31.10: 1 «два окна вплотную друг к другу как одно окно» (not one window), 2 «Спорядження» tab + a
  «Карти» tab «и, может, что-то ещё» (my proposal in the doc: Усе / Витратне / Спорядження / Карти / Різне / ★ Обране -
  question 9; question 10 the arrows' stock in the character block), 3 да, 4 ок, 5 да, 6 ок, 7 ок, 8 ок (order: window
  manager and drag first). Doc updated (decided sections, two docked windows mock-up).
- Stage 1 of the concept (3.0.11-beta.108): core/window_stack (Stack see/raise/top/topDown/marked), win/window_stack
  (draw bottom up in context's endScene/present, mouse top down raising on a press, keys: the top's own first, Esc
  closing the top without one, the rest after; the pad runtime routes by its top - replaces beta.106's `own`/turnedTo),
  uistyle::edge + WindowEdge (the gold edge of the top of two or more, band-only painting), pad core
  blockedAtOrdered + game_window setOrder (drops and the press gate the top first). Ruling R13: the gold edge only
  with two or more windows shown (one alone keeps the approved look) - cost if wrong: one condition. Not yet: Q4 the
  new window's place, Q5 the game's windows over ours (next beta). Tests window_stack_the_last_opened_or_clicked_on_top,
  ui_style_window_edge, gate_the_top_window_first; suite 2064/0.
- Stage 1 rest (3.0.11-beta.109): Q5 - FrameRecord takeTail/putBack + generation(); win/windows_draw's hook marks
  Context::markGameWindows (the first place a frame, under Guard); endScene takes the tail (the game's own windows and
  its cursor) out while the driver's interface is drawn and puts it back after windowstack (answer card, the window
  switch panel, pad tiles/assign/hint, hud edit stay over it). Checked in the decompile (FUN_00559750): after the
  windows' draw only the game's overlays it draws over its own windows (cursor, drag icon, top messages); the world and
  names come before. Q4 - core/window_stack place() (15 % shared = taken; right, else left, gap 8 game px; else 24 down
  and right; inside the screen) + box()/moveTo() on the five movable windows; windowstack::frame(w, h, k) places a
  window the frame it opens. Tests frame_takes_the_tail_and_puts_it_back,
  window_stack_a_new_window_beside_the_one_in_its_place; suite 2066/0.
- Stage 2 of the concept (3.0.11-beta.110): core/window_drag (Press/pressed/moved/released, moved out of skills; the
  dragged Thing set/clear/now); «Речі» and «Навички» tell their drags; «Спорядження»'s worn-thing drag rebuilt (it began
  the game's 0x16 at the press - the same never-let-go fault as the skills' before .106): own drag + icon, on release
  0x16 then 0x12 at the cursor; highlights: equip Show.dropFits (ew::fits per slot), hotbar Show.dropping (items,
  skills), «Речі» Show.dropping (worn) - each in its repaint key. Ruling R14: the pad's carry between windows (Q7) left
  to stage 5 (the doc's order) - the pad already wears (A), assigns (X) and takes off (A in «Спорядження»). Tests
  window_drag_*, equip_window_lights_the_slots_a_dragged_thing_fits, hotbar_lights_its_cells_while_a_thing_is_dragged,
  item_window_lights_the_bag_while_a_worn_thing_is_dragged; suite 2071/0.
- Stage 3, docking (3.0.11-beta.111, Q1): core winstack dockSide/opposite/docked; win/window_stack dockFrame - docked
  when «Речі» or «Спорядження» opens with the other shown (the gear left of the bag where there is room), the one
  dragged by its title leads (itemswin/equipwin moving()), the other follows (moveTo keep=false during the drag, kept
  when it ends), clamped to the screen; Shift while dragging - apart (until one closes). Ruling R15: undocking by
  Shift-drag (the doc said «за его край» - our windows move by their titles only); the doc's bullet updated. Test
  window_stack_docks_the_equipment_beside_the_bag; suite 2072/0.
- Stage 3, new things (3.0.11-beta.112, Q3): core itemwin Item.fresh + Texts.fresh - a gold pill «нове» over a fresh
  cell's corner (paint + words), each tab's count of fresh ones in gold over its name's end; win: the bag places from
  0x00a0 (itemswin::packet) kept new till pointed at or gone, «+N name» drawn for 2 s (itemswin::drawToast after the
  stack, over the bag or at the screen's lower middle, fading its last 0.4 s); strings ItemsFresh. Test
  item_window_marks_the_new_things; suite 2073/0. Tabs (Q9) and the arrows' stock (Q10) still wait for answers.
- Q9 «давай», Q10 «можна» (31.10). Tabs (3.0.11-beta.113): core itemwin Tab {All, Use, Gear, Cards, Etc, Starred},
  tabOf (ammo 10/0x11 into Gear, 6 Cards), Item.starred, shownOf (All / Starred), Part::Star (a cell's lower left
  corner, its thing there), ☆ while pointed at / ★ starred in words, starredOf/withStarred (uaro-graphics\
  items-starred.txt per character); strings ItemsTabAll/ItemsTabCards, ItemsTabItems «Витратне», ItemsTabEtc «Різне»;
  the kept tab's key «tabs6» (old «tab» ignored). Ruling R16: the star is clicked in the cell's corner, not by a right
  click (the right click stays the description, beta.101's R6) - noted in the doc's answer. Tests
  item_window_six_tabs_and_the_starred, item_window_starred_kept_per_character (+ the tab tests updated); suite 2075/0.
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
