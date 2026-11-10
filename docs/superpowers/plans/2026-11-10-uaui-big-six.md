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
