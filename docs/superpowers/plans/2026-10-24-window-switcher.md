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

## Task 3 (3.0.3-beta.8): the panel in the game's picture

The user: «в самой игре это самое то что я хотел», «расположить список ... как по горизонтали так и по вертикали»,
«в настройках консоли должна быть возможность включить или выключить данное окно, также отдельные настройки в
конфигураторе».

- The panel's picture, made in the target's pixels, goes with the frame record as a driver texture (createTexture,
  record.upload) and one quad (TL, interface, premultiplied blend, effect_exe::Kind::Panel: uiMode 0 in draw.cpp and
  renderer11.cpp, sampled as it is): every renderer plays it, the 64-bit one too. Its quad's edges on the target's
  pixel edges (a game x lands at x * k + 0.5).
- Drawn at the next present's start, before the flush, and only over a picture the game drew anew
  (FrameRecord::takeWork): the target keeps its pixels across a present.
- Its mouse in the game window's procedure: over it the clicks are the panel's, WM_SETCURSOR shows Windows' arrow (the
  game's own cursor is drawn under it); the pad's gate setBlocked(rect, all while dragged).
- A layout button beside its cross; [panel] a section of its own (show, layout, size, opacity, hidden, place) under
  «Панель вікон» on «Гра й чат»; the console's rows.

## Task 4 (3.0.3-beta.9): smooth switching, lighter background windows, their sounds, the game's cursor

The user: «можно сделать переключение между окнами более гладко», «на полный екран ... блымает в момент
переключения», «нагрузку фоновых окон надо уменьшить, не надо грузить просто так», «иногда я переключаюсь ... на
рабочий стол», «звуки идут с паралельного окна», «мышка когда наводишь на окно переключения окна меняет цвет».

- The blink: uaRO.exe on its full screen (0x6ebfec, ISFULLSCREENMODE the app writes for «на весь екран») calls
  ShowWindow(SW_MINIMIZE) on WM_ACTIVATE(inactive) and SW_MAXIMIZE on activation (0x6541e0). The subclass answers
  WM_ACTIVATE as the game does (1) without them; a timer then looks where the focus went
  (gamewindows::leaving): another game window - it stays; the desktop or another program - minimised as before;
  Windows' Alt+Tab still open - asked again (3 s at most). Activated while minimised: the game's own handler.
- Summon then show: a switch by the keys or the panel writes the target's process and a serial in the block's header
  (Local\uaro-graphics-windows-2, entries of 96 bytes) and wakes it (Local\uaro-graphics-wake-<pid>, the frame
  pacer's wait and the minimised wait end at it; the 64-bit renderer opens it by --parent). The target draws whole
  (main) while asked, its third whole present writes ready = serial into its entry; the window in front shows it then
  or after 400 ms. A minimised target is restored at once. On its full screen uaRO.exe draws only while its window
  is active ((0x6e4bdc != 0 || 0x6ebfec == 0) in its main loop, 0x4f... and 0x58...): such a window costs nothing
  behind another and cannot get ready - its entry says so (kDrawsBehind clear) and it is shown at once.
- Lighter background: another game window 5 frames a second, one wholly behind another game window of the player's
  (above it in Windows' order) 2, minimised 2 (light::Role).
- [windows] others_sounds (0): the other windows' sounds fade out as their music (sound_hook g_sounds, the sound
  engine's master).
- The panel under the game's own cursor: the frame's last small interface quad over the cursor's point
  (panel::cursorDraw) - the panel's upload and quad moved before it (FrameRecord::moveLastBefore); Windows' arrow only
  when it is not found.

## Task 5 (3.0.3-beta.10): the panel's menu (the right button)

The user: «хотелось бы чтобы при нажатии правой кнопкой мышки и там можно было опции разные задавать».

- The right button pressed and let go on the panel opens its menu at the cursor (right of it and below; no room there:
  left of it, above it; always inside the target). A second picture and quad after the panel's, both moved before the
  game's cursor; at the game's interface scale, not [panel] size (it would grow under the cursor when the size is
  picked in it); not faded by [panel] opacity.
- Its rows (core/window_panel menuRows, menuTexts, menuPick): «Вигляд» - «В ряд», «Стовпчиком»; «Розмір» 75, 100, 125,
  150 %; «Непрозорість» 60, 70, 80, 90, 100 %; the switches «Музика в інших вікнах гри», «Звуки в інших вікнах гри»,
  «Інші вікна гри — легкі»; «Сховати панель · Ctrl+Alt+0». A size or opacity set in the settings to another value:
  none of the segments chosen. A pick changes the running settings and the file at once; the menu stays open.
- It closes on a press off it (that press and its let-go are nobody's), on Esc (taken from the game, its key handler
  0x4d96e0 switches on WM_KEYDOWN's key), on «hide», when the game window is not in front, when the panel goes. The
  right button pressed on the panel while it is open opens it again there.
- While it is open the pad's mouse gate keeps every click from the game (setBlocked all).
- Tests: tests/test_window_panel.cpp - the menu's layout (340 wide at scale 1, each of 5 segments 34 at least for
  «100 %»), its hits, its paint, its rows from the settings, a pick's change, its words.

## Task 6 (3.0.3-beta.11): the game's cursor over the panel, the icons, the frame counter

The user: «мышка темная на менюшке переключения окон» (report R-45N3WS: prontera, full screen, 3840x2160), «супер
минималистичный режим в котором типа иконки маленькие были этих окон, чтобы не занимало много места», «кажется
счетчик кадров замер».

- The cursor: uaRO.exe draws it (FUN_004f60e0 -> FUN_004f5a90) as a face of the renderer's interface list +0xec (flags
  0x201) with the colour 0xFAFFFFFF, z 1e-6, rhw 0.999999. The Flush (FUN_0040e4d0) draws that list (FUN_0040f320)
  after ZENABLE 0, then sorts and draws the additive screen list +0x11c (FUN_0040f9e0); FUN_0055b3f0's digits are
  filed after the cursor too. beta.9's 8 last draws missed it on prontera: the panel stayed over the cursor, seen
  dark through it. Now panel::cursorDraw takes the last interface draw of that colour over the point among all the
  frame's draws first (the old way after it), and the panel moves the cursor's command to the frame's end
  (moveLastBefore of everything after it): the cursor over the panel, the tip and the menu, the draws after it under
  them.
- The icons: [panel] layout = icons (the settings' «Вигляд панелі», the console, the menu's «Вигляд»: «Іконки»):
  core/window_panel Layout::Icons - a grip (it drags the panel), then a 30x30 tile a window with its number and HP/SP
  bars, the current one blue; no title, cross or names. The name and levels of the tile under the mouse in a tip above
  it (under it at the window's top), a third picture (tipPlace, paintTip). The menu 360 wide for the three layouts.
- The frame counter: a WS_EX_TOPMOST window hidden only by the game's presents. On its full screen the game no longer
  presents behind another game window (beta.9: no longer minimised), so the counter stayed over the other window with
  its last numbers. WM_ACTIVATE(WA_INACTIVE) in any mode now hides it and the console (WindowSwitch::onLeave) and
  closes the panel's menu.
- Tests: test_window_panel (the cursor by its colour behind 30 draws, the icons' size, hits and paint, the tip's place
  and ground, the menu's three layouts), test_config (layout = icons).

## Task 7 (3.0.3-beta.12): the panel into the game's scene, the background windows' sounds

The user (report R-KVTEMT, 3.0.3-beta.11, full screen): «всеравно курсор не видно. звуки со второго окна слышно, надо
чтобы звуки со второго окна небыло слышно».

- The cursor: Device7::EndScene flushes the record (the GPU starts on the frame before the present), so at the present
  the record held nothing of the game's scene, and beta.9's and beta.11's cursorDraw never found the cursor. The panel
  now goes into the scene at the game's EndScene (Context::endScene; panel::FrameSlot - once a frame, the present only
  for a picture drawn without a scene), where the cursor's face is still in the record (FUN_0040d8d0: BeginScene,
  Flush, EndScene). The colour pass takes a quad up to 3 pixels off the point: FUN_004f5a90 puts the quad half a pixel
  right and down of it. A log line says whether the cursor was found (at most 6 lines).
- The sounds: on its full screen the game no longer presents behind another window, and soundFrame (the background's
  volume, the other windows' fade) ran only from presents - their sounds stayed loud (window 1 played 2361 sounds a
  minute behind). WindowSwitch sets a 100 ms timer at WM_ACTIVATE(WA_INACTIVE) in any mode, killed in front again;
  Context::behind runs the sounds' fade and the window's entry (its HP and SP for the other panels, every 250 ms).
- Tests: test_window_panel (FrameSlot; the cursor half a pixel off its quad).

## Rulings

- The doc's betas 7 (panel) and 8 (HP/SP on the cards) are one beta: the cards' numbers come with the list in
  task 1, and a panel without them would be shipped only to be changed.
- The windows are checked by the clock (every 250 ms), not every 15 frames: a light window draws 10 frames a second,
  and its HP on the panel would come every 1.5 s.
- The panel's window is per-monitor DPI aware and placed in real pixels (win/dpi RealPixels): uaRO.exe is not, and
  Windows would stretch the panel blurry under its scaling; the interface scale then carries the DPI too.
- A button held on the panel (a drag) is not the game's even off it: the pad's gate asks GetCapture as well.
- Ctrl+Alt+0 is the game's own (Alt+0's chat macro) while [windows] panel is off.
- 3.0.3-beta.8: beta.7's layered window is gone - the user wants the panel in the game's picture. beta.7's [windows]
  panel* keys moved to [panel] (a beta's keys, never released: a beta.7 file's are not read).
- 3.0.3-beta.9: the user did not answer the switch's delay or the background's frame rates - the proposals stand:
  400 ms at most, 5 and 2 frames a second. The block's layout changed (-2): a beta.8 game beside a beta.9 one does not
  see it.
- 3.0.3-beta.10: the menu opens only for a right button pressed on the panel as well: let go over it after the game's
  camera was turned with it, it is the game's.
- 3.0.3-beta.11: the game's cursor is moved to the frame's end (not the panel before it, as beta.9 did): the screen's
  additive faces drawn after the cursor stay under the panel. The icons lie in a row only.
- 3.0.3-beta.12: beta.11's cause for the dark cursor (the 8 last draws) was a guess made without checking what the
  record holds at the present - after EndScene's flush, nothing. The panel moved into the scene; the cursor's search
  logs its result now.
