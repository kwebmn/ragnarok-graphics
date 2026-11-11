# Game UI access (open, close, settings) - plan and ledger

Spec: Claude Doc «Концепция: игровой интерфейс» 3zHfHfPfhA5nCjqGU5xb1Z (owner answers 2026-11-11), mockup
S5uscWfANyf6REjTWWm47C. Owner: «делай». One beta per stage, inline.

Stages:
1. Lock hidden ([interface] lock_parts, no row; old lock_layout ignored), drag as it is.
2. ✕ and Esc on our windows (hunt, MVP timers, death recap, combo, party, target card, tracker); hidden ones remembered.
3. Second icon row in the Alt+V block (Довідник, Полювання, Таймери босів, Відстеження, Група, Що мене вбило,
   Налаштування) + keys Ctrl+H/B/J/G/K (checked against uaRO.exe first).
4. Esc menu: Налаштування, Розмістити інтерфейс, Тест графіки, Надіслати звіт.
5. «Налаштування» window (Alt+O in place of the game's): sections from the schema, search.
6. Section «Гра»: the game's Alt+O options inside (music/sounds + volumes, snap ×3, opaque) via its own controls.

Ledger:
- Stage 1 (3.0.18-beta.119): lock_layout row out of schema/navigation/strings/shipped ini; config reads
  interface.lock_parts only (test config_reads_the_layout_lock). Ruling: the concept's «8 px threshold» not added -
  the drag already needs 0.3 s held and 6 game px (hudlayout::dragStarts, 3.0.11-beta.147), stricter; «Усе як було»
  already exists (layout mode's Reset). Cost if wrong: one constant.
- Stages 2+3 (3.0.18-beta.120): core/closed_windows (closed set in uaro-graphics\closed-windows.txt, the row of own
  windows by switches, Ctrl keys - tests), win/own_windows (file, lit, press, keys with the control char swallowed),
  ✕ on «Полювання», «Таймери босів» (both looks), «Група» (uaUI head), «Картка цілі» (till the next target);
  «Відстеження» keeps its own hide (icon toggles it); new win/recap_window «Що мене вбило» (last death this run,
  ✕/Esc); the Alt+V block's second row (uaUI, full view, gold edge while open, tips with keys).
  Ruling: stages 2 and 3 in one beta - a ✕ with no icon would leave windows closed for good. Cost: none.
  Ruling: Esc closes «Що мене вбило» only - the always-on panels (hunt, MVP, party, tracker) keep Esc for the game's
  menu, ✕ alone hides them. Cost if wrong: one key check per panel.
  Ruling: the combo window is the hotbar's lit cells, not a window - no ✕. Icons only for windows switched on;
  settings icon opens the console until stage 5. Second row in the full view only (the game's row is there too);
  the pad gets it with stage 5's work. uaUI's look only (always on) - the old sheet of the block unchanged.
  Ctrl+H/B/J/G/K checked in uaRO.exe FUN_004d96e0: G/H/J taken with Alt only, B/K never.
- Stage 4 (3.0.18-beta.121): the menu (alive) gets ours over the game's - core menuwin Action Settings, Layout, Test,
  Report, ours(), firstFocus 6 (test). A press closes the game's menu by its own 0x77, then the context takes it:
  settings -> the console (till stage 5), layout -> hudedit::open, test -> testStart, report -> the Ctrl+Shift+F10
  path 3 presents later (the shot without the menu). The death screens unchanged.
- Stages 5+6 (3.0.18-beta.122): «Налаштування» = the game's options window (Alt+O, UI 0xD) in a new live look:
  core/settings_window (sections: «Гра» the game's six, «Як виглядає гра» the three choices, then the app's
  «Інтерфейс» sections; search by settingMatches/queryMatches - tests); win/setting_edit (the console's write, shared;
  the console now calls it); options_card's live doc rewritten (nav, search input, rows: switch / slider / choices),
  ours written to the ini, the context reads it at once (takeChanged); the icon and the menu open it by MakeWindow
  0xD (the console when [interface] options is off); «Усі налаштування графіки… (Alt+D)» opens the console.
  Ruling: stages 5 and 6 together - replacing Alt+O without «Гра» would lose the game's options. Cost: none.
  Ruling: hints over 160 chars left out of the rows (the app shows them). Cost if wrong: a cut-off.
  Ruling: the pad walks the window's .ctl controls (uilive); the old pad command still works for the game's six.
  Class names checked against the theme (sro, shn - .lw .srow and the sheet's .sh clashed), probed on the host.
- 3.0.18-beta.123 (R-7TVEJU): «Група»'s ✕ stood over its title (no place class) - .closeb right 2dp, the fold/view and
  the count moved left 26dp. The report itself sent by the menu's «Надіслати звіт» (works, no menu in the shot).
- 3.0.18-beta.124 (owner «1,2,3,4,5,6» - item 1, the pad): the Start tile takes our windows switched on («Полювання»,
  «Таймери босів», «Відстеження», «Що мене вбило») - core padtiles any count, three a row, a short last row in the
  middle, up/down to the nearest column (test); its «Налаштування» tile opens the new window; «Налаштування» on the
  pad walks its controls (the runtime's optionsPad only for the old look); − and + by every slider (pad and mouse).
- 3.0.18-beta.125 (item 2, «Клавіші»): [interface] key_hunt, key_mvp, key_tracker, key_party, key_recap - choices off
  or Ctrl+B D G H I J K L N U Y (the letters uaRO.exe leaves free with Ctrl; C X Z S the fields'); schema rows, the
  app's «Інтерфейс» section «Клавіші» (so «Налаштування» too), shipped ini, applied at once (tests); the icons' tips
  show the key given. Ruling: one letter for two windows - the first in the row wins (no check). Cost: a confusion.
- 3.0.18-beta.126 (item 3, «Спорядження»'s character): facts - uaRO.exe FUN_0046fd40 draws the body, the head, the
  three headgears, the body's palette (0x77190C - the clothes' dye, not the weapon: FUN_006399e0 "몸\%s%s_%d.pal") and
  the hair's, and the job's .imf (FUN_0062e920, data\imf\) - no weapon, no shield. Built: the clothes' dye
  (data\palette\몸\<+0x90c name><ending>_<n>.pal), the .imf order (core readImf/imfPriority, test; format checked on
  UaRO.grf's 성직자_여.imf: the body 1 over the head 0 standing - the veil). Ruling: no weapon / shield, as the
  original. Cost if the owner wants them: the weapon sprite tables (weapon view, job weapon names) to find.
- 3.0.18-beta.127 (item 4): «Картка цілі»'s «lines[4].colour» / «color: ;» (R-7TVEJU) - its details' list never
  shorter than before (at least 12, the spare rows empty and hidden by data-if): no row past the list's end.
- 3.0.18-beta.128 (owner «1,6,7,8» - item 7): the Alt+V block's small views (compact, rings, icon) - «…» right of the
  block, a click opens our windows' row beside it (lit while open, tips with keys); the body that much wider so its
  blocker takes those clicks.
- 3.0.18-beta.129 (item 6, the owner's own beyond the original): the weapon and the shield on «Спорядження»'s
  character as the world draws them - the player actor (mode 0x74B57C +0xC4 +0x3C) weapon +0x368 low half, shield
  +0x378; weapon «인간족\<0x7719D8[job]><0x7719E8[sex]>_<n>», else by kind (core/weapon_look kindOf = FUN_0064bc50,
  forJob = FUN_005b1ee0's remaps - test) «...<0x7719F8[kind]>»; shield «방패\<+0x890[job]><ending><+0x8F0[n]>»
  (FUN_00639c70). Paths checked in sprite.zip (페코페코_기사_남_1116, _검). Ruling: both over the body in every
  direction, no weapon glow (검광), no dual-wield left hand. Cost: a back view with the weapon in front.
- 3.0.18-beta.130 (R-DQKNDH; the owner: «в окне статов наводка на статах не двигается с джойстика только с мышки,
  посмотри остальные моменты», «кнопки rt lt ... не влезли»): readHoverCursor (none while the controller plays) in
  58 parts' hover (the stats' and skills' lit row and tip then follow the pad's choice); uaUI's documents get the
  mouse's leave while the pad plays (no :hover kept by the hidden cursor). The hotbar's shoulder badges (LB RB LT
  RT) as wide as their pills - uaUI's picture was square, the letters cut (test). R-DQKNDH's old stats window: the
  player's own [interface] status = 0 (no write by «Налаштування» in its logs).
