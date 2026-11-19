# The guild window rebuilt - implementation plan

> Executed inline (the owner: «все в инлайн режиме», «все делай» - every stage, betas without stopping).

**Goal:** «Гільдія» (Alt+G) becomes a wide window like «Навички»: the guild skills with the game's icons, groups,
locked ones, server-true descriptions, the shared cooldown, «Використати» / «+» / drag; «Інформація» with the exp
bar, castles, allies and enemies, the notice for all; «Учасники» with contribution, search, sort and a card with
actions; the other tabs in the same frame; the controller on every tab.

**Architecture:** core (no Windows): guild_skills (the uaRO table, rows, the side panel's lines, the cooldown),
guild_tabs (the server's 0x0160/0x0162/0x0163/0x0166/0x016f + 0x014c, 0x0154), guild_card (the tabs' views, the window's
hit boxes and hit test). uikit/guild_live: one hand-made RML document `gdw` + its model, fill from core's views,
refit = the boxes of every data-hit into core's layout. win/guild_card: reads the game (as now), the packets
(net_watch), the input, the game's own commands (0x45 tab, 0x4A raise, 0x7E use, 0x51 + 0x12 drag, 0x83 notice,
6/0x87 close). pad/win/runtime guildPad: the buttons.

**Spec:** docs/superpowers/specs/2026-11-19-guild-window-design.md

## Global Constraints

- RML by hand; every R"RML parsed by an XML parser before shipping; a data-for element carries no data view of its
  own (a plain wrapper, the views a level down); class names unique (checked against core/ui_theme_css.cpp and
  tests/test_ui_scopes); sizes in rem of the model's base (12 x the window's scale); N-a-row = border-box + space-between.
- The repaint key covers everything the window shows.
- UI words in strings.cpp (en, uk, ru), appended in the order of Text; the game's skill names stay English.
- No server change; no new setting (the same [interface] guild).
- Lines <= 120 in src/tests; builds nice 19 / ionice idle / -j4; betas 3.0.22-beta.N via gship; grep «Â» first.

## Stages and tasks

1. core/guild_skills + tests: the table (14 shown, Glory off), rows from what the server sent (locked from the
   needs), the side panel's lines, the 5-minute shared cooldown.
2. core/guild_tabs: 0x0162's inf/sp/range, 0x014c alliances, 0x0154 members + tests.
3. core/guild_card: Show/Layout/hit boxes, the views of the six tabs, castles from the server's words + tests.
4. strings (en/uk/ru).
5. uikit/guild_live (RML, model, fill, refit) + theme CSS `gdw` + host tests (layout, hits, no overflow).
6. win/guild_card: the new window wired (data, mouse, keys, use/raise/drag, cooldown from 0x011a, notice editing,
   «Змінити»), net_watch routes 0x014c/0x0154/0x011a, guildPad. → beta (stage 1: the frame, «Навички»,
   «Інформація», the other tabs ported).
7. «Учасники» whole: sort, search (typed, Ctrl+F), the card, «Написати», «У друзі», your tax. → beta.
8. Research in the game's code, then: «Запросити в групу» (by account), «Посада», «Вигнати…» (reason), our own
   positions editor, the no-guild window. → beta(s).

## Ledger

- Ruling 1: the concept's open questions decided as it proposed (the owner: «все делай») - the wide window, Glory
  hidden, our own descriptions (the game's English text dropped: wrong for uaRO), «Використати» + drag, allies and
  enemies, the notice for all, the members' extras, «Наступна облога» not made (the schedule is empty).
- Ruling 2: the guild skills read from the game's own list (session + 0x2E4) - its record is what «use» (0x7E) and the
  drag (0x51) need; the server's 0x0162 only while the list is empty.
- Ruling 3: positions and expelling not sent by our own packets (0x0155 / 0x015b go through the game mode's 0x74 / 0x7C
  with the game's own lists and checks - unproven blind); the master's «Посада й вигнання…» shows the game's own
  members' window, as «Змінити» does for the positions. Party invites are the game mode's 0x41 (the packet 0x00fc).
- Tasks 1-6: shipped 3.0.22-beta.26 (b9d3245f) - the frame, all six tabs, «Навички», «Інформація», «Учасники» with
  search, sort and the card («Написати», «У друзі»), the controller. Host: 3224 tests.
- Tasks 7-8: 3.0.22-beta.27 - «Запросити в групу», «Посада й вигнання…» (the game's members' window), Alt+G with no
  guild. Not made: our own positions' editor (the game's «Змінити» stays).
