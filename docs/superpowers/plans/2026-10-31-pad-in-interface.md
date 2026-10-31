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
