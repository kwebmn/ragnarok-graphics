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
