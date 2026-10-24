# Window switcher - implementation plan

**Goal:** switch between the player's game windows from inside the game - a key and a click on a panel of the
windows over the game - instead of Windows 11's Alt+Tab with every program in it.

**Spec:** the user's plan doc «Переключатель окон игры» (claude.ai/artifact/2xodiH6WRgtaXtjpcfJAEP, 2026-10-24) and
the user's words: «я хочу в игре мышкой переключатся и быстрой кнопкой», «вверху как правило скилы, надо чтобы
панельку с окнами можно было двигать», «кнопка быстрая показать спрятать», «делай современно», «у каждого свое
разрешение екрана», «приступай к реализации». The doc's four questions were not answered: its proposals stand.

## Global constraints

- Betas through ship-beta.sh, releases only on «выпускай»; the gate before each; code comments English, lines <= 120.
- ddraw.dll imports kernel32/user32/gdi32/msvcrt only: no GDI+, the panel's shapes are drawn by our own code.
- Nothing is written into the game's memory and nothing goes to the server: a switch is Windows' SetForegroundWindow
  from the game in front (Windows lets only the program in front give the front away).
- Keys taken in the game window's procedure (subclassed, ANSI) before the game sees them: Ctrl+Alt+digit is a chat
  macro in uaRO.exe (Alt is checked first, 0x4d96e0), F1-F9 fire the hotkey bar with any modifier.

## Task 1 (3.0.3-beta.6): the windows' list and the keys

- `core/game_windows`: `Entry` (64 bytes: seq, pid, window, number, frontAt, name[32] UTF-8, levels, hp/sp %) x 8 in
  `Local\uaro-graphics-windows`; a game's entry is its log slot's (1-8, the first started 1 = the window's number).
  `windows(table, alive)` the live ones by number; `previous(list, self)` the one in front last before this one
  (never in front: the next number); `byNumber`; `keyAction(vk, ctrl, alt, shift, keys)`: Ctrl+Tab back (not with
  Alt: Windows' own), Ctrl+Alt+1-8 (and the number pad's) window N, Ctrl+Alt+0 the panel (task 2).
- `win/game_windows`: the shared block, an entry written under its seq (odd while written), read back consistent;
  alive = the process runs and the window is still its own.
- `win/window_switch`: the game window subclassed once; a key acted on is not passed on; a switch restores a
  minimised window and SetForegroundWindow; this game's frontAt written before it gives the front away.
- `[windows] switch_keys = all | back | off` (all), «Клавіші вікон» on «Гра й чат», applies at once.

## Task 2 (3.0.3-beta.7): the panel

- `core/window_panel`: the layout (row or column; cards narrowed when wider than half the window), hit test (grip
  row, close, card i), the place as fractions of the free room (0 left/top .. 1 right/bottom: kept at its edge in any
  window), the shapes into a premultiplied BGRA picture (rounded plates, number badges, HP/SP bars red under 25 %
  as the game's gauges, 0x4490d0 / 25.0 at 0x696908).
- `win/window_panel`: a layered window owned by the game window, not activated by a click (the game stays in
  front), text by GDI (Segoe UI); dragged by its top row, «×» and Ctrl+Alt+0 hide it ([windows] panel_hidden,
  written); shown in the main game window while two or more run; follows the game window; scale = the game's
  interface scale (client width / game width) x [windows] panel_size %.
- The pad's mouse gate: a click over the panel is not the game's (its window class).
- `[windows] panel = 1`, `panel_layout = row | column`, `panel_size = 100` (75-150), `panel_place = "fx fy"`.

## Rulings

- The doc's betas 7 (panel) and 8 (HP/SP on the cards) are one beta: the cards' numbers come with the list in
  task 1, and a panel without them would be shipped only to be changed.
