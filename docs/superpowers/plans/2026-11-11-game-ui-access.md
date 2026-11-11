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
