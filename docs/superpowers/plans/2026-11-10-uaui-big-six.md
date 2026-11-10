# uaUI - the six big pieces (plan and ledger)

Owner 2026-11-10 night: «сделай 1,2,3,4,5,6 не прерываясь, без остановки, в инлайн режиме. меня не спрашивай» -
stage 6 approved (uaUI on by default, the old painters go). Betas only; the owner tests in the morning.

Order (safest first; the cleanup last, once nothing needs the old code):
- A (item 1, part 1): [interface] uaui on by default (config, schema, dist ini).
- B (item 4): the last pictures - monsters' HP bars, the way-in veil, the shelf's toast, the cursor - on uaUI; the
  recording code (core/ui_scene record, panel::setRecording) removed.
- C (item 3): speed - a sheet re-said only when its content changed (compare, not a generation), single variables
  dirtied, hidden documents not updated; the log's cost line kept for the before/after.
- D (item 5): themes through «оновлення» - theme.rcss, palette.txt and <part>.rml read from the admin's update GRFs
  (under the player's own files in uaro-graphics\ui).
- E (item 6): our own text fields - the parts' fields typed through uaUI with the keyboard closed to the game while one
  has the focus (no double letters, no battle-mode typing).
- F (item 2): input through the library, window by window from the simple ones: active documents (RML + data events),
  hover by RCSS, layout by flexbox; the core hit tests left for the windows not yet moved.
- G (item 1, part 2): the old painters removed - each part's picture path (blank/paint/drawPart/texture) and its
  `if (!sheetShown)` branch; the core layouts and word lists the sheets use stay.

## Ledger
- A beta.68: [interface] uaui on by default (config true; schema was already 1)
- B beta.69: monsters' HP bars + NPC dots on world sheets; low HP edge = uaUI doc with a radial gradient (gradients no
  longer need uaui_effects); the way-in veil and the shelf's note on sheets; new layers Top (z 1000 marker) and Cursor
  (z 9000) drawn after the game's windows; pad overlays + HUD editor moved to Top (they had dropped under the game's
  windows with the sheets); the cursor = a uaUI document in the Cursor layer (drawn where the game's cursor faces were
  dropped). The recording code goes with G.
- C beta.70: sheets dirty only the lists that changed (Content compared), Doc::place/prop set on change (transform, grounds), per-geometry mesh cache (remeshed count in the cost log line)
- D beta.71: gamefiles::Reader(dir, updatesOnly); uaui shipped(name) reads data\uaro-graphics\ui\<name> from the data folder or the update GRFs: theme.rcss after the built-in rules, palette.txt before the player's, <part>.rml when the player has none; README tells admins
- E beta.72: core/text_field (UTF-8 typing, whole-letter Backspace, Ctrl+A selection, one-line paste) + win/ui_field
  (keys -> field, clipboard; every key but Alt's eaten) for the searches of «Речі», «Склад», «Візок», the shop and
  «Довідник» (they took Latin letters only via letterOf); RmlUi's fields get the Windows clipboard.
  Ruling: the chat's line stays the game's own edit (its sending, its IME, the battle-typing guard already in place)
  - cost if wrong: the chat keeps the game's field.
- F beta.73: Doc::pointer (a mouse message into RmlUi over this document; «ctl» elements are controls, the rest its
  ground), Doc::boxesOf (pad targets by selector), uisheet::NavSet (pad walk for sheets and live windows alike),
  win/ui_live (live window helper: make/frame/draw/mouse/box). Live: emotes (flex grid, click puts, dblclick sends,
  drag by its ground), commands (tabs + two-column rows), the popup menu (rows, separators; a press elsewhere closes).
  The other windows stay sheets for now.
