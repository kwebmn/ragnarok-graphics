# Frame capture and replay — design (plan 2.3, item B2)

**Why.** Every change of the picture is checked by starting the game, walking to the place and looking. The glints
of 2.1.5–2.2.2 took five builds this way. A frame captured once and drawn again by any later build, without the
game, makes such a check a minute's work and a before/after picture on fixed scenes (c_tower2's floor, umbala's
decks, rain, night, a field with grass).

**Status.** Not implemented: it can only be developed with a Windows machine and a graphics card at hand (the replay
draws with Direct3D 12). Everything below is grounded in the code as of 2.2.2.

## What a frame needs

| Piece | Where it is now | Captured as |
|---|---|---|
| The draws between two presents | `FrameRecord` (`core/frame.hpp`) — plain data, already serialised for the bridge (`bridge::writeRecord`) | the record(s) of the frame |
| The textures they use | each texture surface's `TextureLevels` (`core/texture_levels.hpp`): every level as ARGB in memory | id, size, levels, colour key and the pixels of each level the frame's draws name |
| The camera | `readGameCamera`, `readCameraTargetDepth` (`win/game_camera`) | valid, `view[12]`, `targetZ` |
| The map | `readGameMap` | its name (the replay reads its files from the game folder, as the renderer does) |
| The effects | `Context::effects()` | the `[effects]` section as ini text (by the schema's tokens) — a later build parses it with its own keys |
| Size and scale | `planScale`, the window's pixels | game size, output size, the plan's k and target |
| Time | the renderer's own clocks (water, grass, critters, weather, sky) | the seconds since the game started, the hour and minute |
| Twins of crisp text | `win/crisp` | none: capture with crisp text off (a key's option) or leave the interface out |

## Capture (driver)

- **Key:** Ctrl+Shift+F10 in the game (beside F11's frame counter and F12's screenshot); the next whole frame is
  captured. Logged with the file's name.
- **File:** `uaro-graphics\captures\<map>-<yyyymmdd-hhmmss>.ucap` — `UCAP`, a format version, the driver's version,
  then the pieces above as sections (`WireWriter`), deflated with miniz (already linked). A frame of a field names
  some 200–400 textures: 20–80 MB raw, 8–30 MB in the file.
- **Code:** `core/capture.{hpp,cpp}` — the format, written and read without Windows (round-trip tests in `tests/`);
  the driver fills it in `Context::present` from the record about to be executed (`Context::flush`) and the
  surfaces' `TextureLevels`.

## Replay (64-bit renderer)

```
uaro-render64.exe --replay <file.ucap> --dir "<game folder>" --out <file.png> [--ini <file.ini>]
```

1. A hidden window of the captured output size; `Renderer::create` as for the game.
2. `createTextureAs` for each texture and an upload record of its levels.
3. `setMap`, `setEffects` (the captured `[effects]`, or `--ini` to compare: the same frame with an effect off),
   `setScale`, `setCamera`, the clock frozen (below).
4. `execute` each record, `present` with `PresentOptions::screenshot = <file.png>`; exit.

**Frozen clock.** The animated passes read their own time (`waterTime()`, the grass's sway, critters, weather, the
sky). A `Renderer::setClock(seconds)` that every one of them reads instead of the system's makes two runs of the
same capture give the same picture — the step that touches most files, worth its own review.

## Use

- `tools/replay.bat` (next to the client): every capture in `uaro-graphics\captures` drawn by this build into
  `captures\out\<name>.png`; run once with the old build and once with the new one, compare the pictures.
- The Diagnostics page can offer to attach the last capture to a report (large: asked for).

## Order of work

1. `core/capture` with round-trip tests (Linux) — a day.
2. The driver's key and writer — a day; checked on Windows by the file's size and the log.
3. The replay mode — one to two days with a person running it on Windows.
4. The frozen clock — a day.
5. `tools/replay.bat` and a section in `docs/ARCHITECTURE.md`.
