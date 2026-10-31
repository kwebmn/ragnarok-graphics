# Pad in the interface - implementation plan

> Executed inline (the owner 2026-10-31: «приступай к реализации в инлайн режиме»), one beta per stage or part,
> no stops between stages; betas only (`$S/ship-beta.sh`), the build gate (`$S/gate.sh`) before each.

**Goal:** the controller becomes part of uaRO Graphics' new interface: one program, hints on the interface's buttons,
the target as a ring (no fake state), the walking arrow, the windows driven by the pad.

**Spec:** Claude Docs «Контроллер в интерфейсе uaRO — концепт» https://claude.ai/artifact/3EatdWXW2sNZxyeHwa27Ve
(rev 33, after the review). The owner said «приступай» without answering its 9 questions: the doc's proposals are the
defaults (ruling R3); the monsters' HP bars wait for question 2 (R2).

## Global constraints

- Every new switch shows in the app's «Інтерфейс» (+ the console); UI names from strings.cpp's uk column.
- No server change: only the requests the game sends itself.
- Nothing old goes before the new is checked: the Start menu, Back as the pad's switch, the fake target mark until the
  ring is confirmed, the mouse walking until the switch.
- New visual effects restrained; `near` / `far` never identifiers (Windows macros); no std::lround / std::hypot in
  per-pixel loops (i686 x87 calls).

## Stages

1. **Friendly skill choice** (state «Выбор цели навыка»). Pad core `Controller`: a support skill (inf 16, not enemy or
   ground) pressed while party members are visible -> `choosing_` (members in the party panel's order, the character
   last); D-pad up/down moves, A casts on the chosen (castOn: send or walk up), the same skill's button again casts on
   the character, B or Start cancels, 8 s cancels; other buttons do nothing meanwhile; the stick still walks.
   `setFriends(ids)` from `partywin::hereIds()`; `choosing()` -> the party panel edges the chosen row (strong gold) and
   the others here (as beta.82). The runtime opens no menu on Start while choosing. Tests: test_pad_controller.
2. **Input mode + glyphs.** core/input_mode (intentional input: a press or the stick past half; back to the mouse on a
   click, a key or a move past 8 px; at most one switch per 1.5 s), core/pad_glyphs (Xbox A B X Y coloured letters in
   rings, PlayStation cross circle square triangle as line icons, LB/RB/LT/RT and L1..R2 pills, D-pad and sticks);
   hotbar cells get their button's badge in the top left, cells without one dimmed; the target card's hint line
   (A «Атака», R «Інша ціль»); [interface] pad_hints (1). Plus a passive log of 0x0195 HP % (the live check, R2).
3. **Target ring** under the pad's target (gold, the chevron over its head), the next one while the right stick points
   (dashed), a chosen friend (green, notches, «+»); the fake 0x0229 mark off while the ring is drawn. Research first:
   the actor's position in the client's memory, the ground drawing (core/ground_way) path.
4. **Walking without the mouse** behind a switch (off): the pad sends 0x00a7 to the last walkable cell along the
   stick (the .gat from the driver), the stop as now; the arrow on the ground, faint by default.
5. **The app page «Контролер»** (replaces «Геймпад»): the controller connected now, the layouts per character, the
   settings; uaro-pad.ini and uaro-pad-binds.ini moved in (R1); then Start opens the windows' tile.
6. **Windows by the pad:** the tile on Start, «Речі» / «Характеристики» with a gold frame, X = assign through a picture
   of the controller, the NPC hint line in the interface's style.
7. **Monsters' HP bars** - only after the owner's answer to question 2.

## Rulings

- R1: moving uaro-pad.ini / uaro-pad-binds.ini goes with stage 5, not stage 1 - the Start menu writes them until the
  page replaces it; moving earlier makes two sources. Cost if wrong: one more beta.
- R2: no HP bars before question 2; stage 2 only logs what 0x0195 brings (the live check the doc asks for).
- R3: unanswered questions 1, 3-9 taken as the doc proposes («админка» = the app page; Back stays; first press =
  choose, second = self; ring + chevron; faint arrow; glyphs by intentional input; standalone pad kept; pad first).

## Ledger

- Stage 1: complete - 3.0.11-beta.85 (c5794c80), tests ctl_support_* (7), party_panel_pad_choice,
  ctl_choice_hint_names_the_buttons; suite 1971/0.
- Stage 2: built as 3.0.11-beta.86 - core/input_mode, core/pad_glyphs (+8 line icons), hotbar badges, target card
  hints, [interface] pad_hints, 0x0195 HP log (EvActorName.hp); tests input_mode_*, pad_glyphs_*, hotbar_pad_badges,
  target_card_pad_hints, config_pad_hints, s2c_monster_name_carries_its_hp; suite 1981/0.
- Stage 3: built as 3.0.11-beta.87 - Controller::preview (light tilt 0.3-0.6), core ground_way ring/notches/chevron,
  win/game_camera readActorWorld (actor + 4) / readActorBox (pick list 0x74B5F0 + 0x28, id + 0x18), win/ground_marks,
  [interface] target_ring off|pad|always; the fake 0x0229 mark kept until the owner confirms the ring (doc).
  Ruling: no ring heights from the .gat - flat at the actor's own height with the depth bias (slopes under a monster are
  small) - cost if wrong: a ring sinking on steep ground, one beta.
- Stage 4: built as 3.0.11-beta.88 - Controller direct walk (setDirectWalk/setLook/setGround/walkTarget: 6 cells in
  half-cell steps, resend 150/350 ms, the stop as before), GroundGrid from walkable.bin, the look from the view matrix
  (as the minimap), core ground_way arrow, win/ground_marks arrow + end ring, [interface] pad_walk mouse|arrow (mouse).
  Ruling: the game cursor is not hidden (left where the mouse put it; hiding means every part's cursorDraw) - cost if
  wrong: a still cursor on screen, one beta.
- Stage 5: built as 3.0.11-beta.89 - the app page «Контролер» (was «Геймпад»): the last controller seen
  (uaro-graphics\pad-seen.txt written at connect), the interface's three controller rows, each character's 14 buttons
  (uapad parseProfiles/withLayout/formatProfiles, uagfx_pad linked into the app), walk radius slider, the old mark
  choice; the pad follows the interface's language (followDriverLanguage). Ruling R4: the pad's two files stay where
  they are (uaro-graphics\, edited by the page and the Start menu alike) instead of moving keys into uaro-graphics.ini
  - one settings place for the player without a migration's risk; cost if wrong: a migration beta later.
- Stage 6 part 1: built as 3.0.11-beta.90 - core/pad_tiles (3x3, Gamepad icon), win/pad_tiles (Blocker::PadTiles,
  modal mouse), Start -> the tile ([interface] pad_tiles, on), tiles open the game's windows (charpanel::pressGame
  0x80/0x7E/0x82/0x81/0x85/0x7F), the big map (minimapproto::showBig) or the map card, «Довідник», the pad's own menu
  (runtime openPadMenu). Next: «Речі»/«Характеристики» by the pad, X assign, NPC/choice hints in the interface's style.
- Stage 6 part 2: built as 3.0.11-beta.91 - «Речі» by the pad (itemswin::padCommand: frame = show.hover with the
  tip at once, pad texts in the tip, A useCell, Y card, X assign, LB/RB tabs, B/Start close; runtime itemsPad +
  stickSteps), core/pad_assign + win/pad_assign (Blocker::PadAssign; hotbarwin::assign = barNow + putBar,
  slotWords). Left for part 3: «Характеристики» by the pad, NPC/choice hints in the interface's style.
- Stage 6 part 3: built as 3.0.11-beta.92 - pad hint parts (npc_hint hintParts / npcHintParts / choiceHintParts),
  core/pad_hint + win/pad_hint (the line over the skill bar, runtime setOwnHints/hints, the GDI hint off while
  [interface] pad_hints), «Характеристики» by the pad (statswin::padCommand, runtime statsPad). Stage 6 done; stage 7
  (HP bars) waits for the owner's question 2.
