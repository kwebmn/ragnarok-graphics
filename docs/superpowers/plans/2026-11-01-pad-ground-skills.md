# Ground skills on the pad and the mouse - implementation plan

> For agentic workers: executed inline (the owner's standing «все в инлайн режиме», betas via ship-beta.sh, no stops).

**Goal:** every ground skill (Fire Wall, Storm Gust, Sanctuary, traps...) placed fast on the pad - a tap puts it
where it usually goes, a hold aims it with the server's own cells shown first; the same cells shown for the mouse in
the game's own targeting; walls and traps counted on the hotbar.

**Spec:** Claude Docs «Концепция: маг, прист и хант на геймпаде» https://claude.ai/artifact/YVYxW5sE4kH47HKR3PLc8P -
answers 1-10 of 2026-11-01 in its «Решения» table (Q2 open: any stick aims - proposed, checked in a beta).

## Global constraints
- uaRO server facts (uAthena src/map/skill.c skill_init_unit_layout, map.c map_calc_dir, db/skill_unit_db.txt,
  db/skill_db.txt): Fire Wall dir = map_calc_dir(caster, cell); even dir 3 cells across, odd 5-cell staircase; caster's
  own cell -> dir 2; Ice Wall 5 always; maxcount FW 3, Quagmire 3, Fire Pillar 5; traps range 3, nobody within 1 cell.
- The client's ground request is 0x0113 (22 bytes: lv +5, skill +9, x +12, y +20); skill units 0x011f (16 bytes: unit
  +2, creator +6, x +10, y +12, type +14), 0x0120 (6 bytes: unit +2).
- One button = one action; no chords (feedback-controls-design). Q7: no presets. Q8: no second set.
- Every new visible part switchable (feedback-every-part-switchable): [interface] skill_preview, [interface]
  ground_counts; strings in the uk/ru/en table, rows in «Інтерфейс» and the console.

## Phases (each a beta)
1. core/skill_area: server shapes (dirOf, cells(skill, lv, caster, at)), kinds (wall, crowd, friend zone, trap, ahead);
   pad slots: tap rules - wall between (2 / 1 / on it / 2 ahead), crowd = the cell covering most monsters, ahead (Fire
   Pillar) like traps; flash of the cast's cells 0.4 s after any 0x0113 (pad or mouse). Tests: dirOf against the C
   rule, FW/IW layouts for all 8 dirs, areas, autoWall table, crowd best cell.
2. core/ground_units: own units from 0x011f/0x0120 grouped into casts (same type, 150 ms) -> walls up per skill; hotbar
   badges «N/3» (FW, Quagmire, Fire Pillar), traps «×N» (Trap 1065 in the bag), a flash when a wall burns out.
3. Mouse preview: the game's targeting (mode +0x3C4 state 1 ground / 5 trap, skill +0x3C8, level +0x3D0) + the cell
   the game picked under the mouse (win/pick_hook) -> the cells drawn under the cursor.
4. Pad aim: a ground skill's button held 0.25 s -> aim mode: walking paused, either stick moves the aim (walls/traps:
   side by the stick in the server's 8 directions, distance by tilt; areas: a cursor that sticks to monsters/friends),
   D-pad by one cell, release casts, stick click (L3/R3) or B (if not the held one) cancels; hint line; tap = phase 1.
