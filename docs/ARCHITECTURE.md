# uaRO Graphics — how it is built

A map for a reviewer or a new session: which process does what, how a frame travels, where the settings live and
which rules keep it working. Version notes live in `dist/TEST.*.md`; the older designs in `docs/superpowers/`.

## Processes

| Program | Source | What it does |
|---|---|---|
| `ddraw.dll` (32-bit, in `uaRO.exe`) | `src/win` | DirectDraw 7 and Direct3D 7 of our own (`src/win/dx7`): the game's draws recorded and handed to a renderer; the game's hooks, the console, the frame counter, the statistics' lines |
| `uaro-render64.exe` (64-bit) | `src/host` | The same Direct3D 12 renderer in a process of its own — DLSS and frame generation (NVIDIA Streamline) exist only for 64-bit programs; also builds the AI texture pack (`--pack`) |
| `uaro-graphics.exe` | `src/app` | The settings app (Win32); `--watch <pid>` waits for a game to end, then sends the statistics and, when it went badly, a report |
| `uaro-graphics-setup.exe` | `src/setup` | The installer: the release zip appended to it |
| `uaro-studio.exe` (64-bit) | `src/studio` | uaRO Studio (2.3): the pack's textures and pictures side by side, made again, kept, linked to the running game; its engine is a copy of `uaro-render64.exe --studio` |

`src/core` holds everything without Windows headers — settings, effects and their groups, formats (GRF, RSW, RSM,
GND), the texture pack, the bridge's messages, the statistics — and is tested on Linux (`tests/`).

## A frame

1. **Recording.** The client draws pre-transformed vertices through `Device7` (`src/win/dx7/device7.cpp`). Each
   call becomes a command of `FrameRecord` (`core/frame.hpp`: Clear, Viewport, Draw, Upload, WriteTarget — plain
   data, no pointers). Hooks into `uaRO.exe` tell what a draw is: the sprite and its folder (`sprite_hook`), the
   game's effect (`effect_hook`), the cell under the mouse (`pick_hook`), hits and sounds (`hit_hook`,
   `sound_hook`); crisp text (`src/win/crisp`) swaps the windows' tiles for twins at the screen's pixels. The
   textures keep their pictures' own colours (`texture_hook`, `core/full_colour.hpp`): the game makes them 16-bit
   as it writes them, and their uploads carry the 8-bit texels wherever the 16-bit ones are still made of them.
2. **Present.** `Context::present` (`src/win/context.cpp`) flushes the record, reads the game's camera, plans the
   scale (`core/scale`), supersampling, TAA or DLSS, sets the effects — `Context::effects()`, the quality set and
   the groups made into switches (`core/effect_groups`) — and presents; then the frame counter, the statistics and
   the console.
3. **Drawing.** `gpu::Renderer` (`src/win/gpu/renderer.hpp`) has four back-ends: Direct3D 12 in the game's process
   (`device.cpp`, `draw.cpp`, `pipeline.cpp` and an effect pass a file: `ao.cpp`, `haze.cpp`, `water.cpp`,
   `torches.cpp`…), Direct3D 11 (`renderer11*.cpp`, fewer effects), RTX Remix (`remix9.cpp`) and `Remote`
   (`remote.cpp`), which sends every call to `uaro-render64.exe`. The draws replay through one shader that emulates
   Direct3D 7's fixed function (`draw_vs`/`draw_ps`); the effects are passes over the world between and after them,
   each timed on the card (`EffectTimer`, names in `core/effect_costs.hpp` `kTimedPasses`).

## The bridge to the 64-bit renderer

One block of shared memory with a ring each way (`core/bridge_ring.hpp`); the messages are `bridge::Msg`
(`core/bridge_protocol.hpp`), written and read with `WireWriter`/`WireReader` (`core/bridge_wire.hpp`). The host
checks `kVersion` and exits with code 2 on another one; the driver then draws in its own process. **Raise
`kVersion` with every change of a message's contents** (2.2.1 changed two without it).

The game's loop keeps the host's pace: it may draw two frames ahead of the screen (`Remote::present`). With DLSS
frame generation, whose Reflex holds each of the host's frames back until the card is about to be free, it draws
none ahead: its next frame starts after the host's present and Reflex's sleep (`Host::present` signals `presented`
after both), so the game takes its input when Reflex wants it (2.5.25). The host's frame pacer then keeps the
frames shown, the generated ones too, under the monitor's refresh (`core/frame_fit`).

## Settings

- `uaro-graphics.ini` → `parseConfig` (`core/config.cpp`) → `Config`. The keys the pages edit are the schema
  (`core/settings/schema.cpp`: section, key, control, range, tokens, texts); each `[effects]` entry also binds its
  `EffectSettings` field, and `parseConfig` and `tokenOf` read and write those keys through it — a new effect's
  key is one schema line, its field, its texts (`core/settings/strings.cpp`, three languages) and its group
  (`core/effect_groups.cpp`).
- **Live changes.** The driver looks at the file once a second (`Context::watchSettings`); `liveUpdate`
  (`core/live_config.cpp`) takes what applies now and lists what waits for the next start.
- **Who writes it.** The settings app (`Model::apply`, `Model::writeNow`; `app/files` `writeFileAtomic`), the
  console and Ctrl+Shift+F11 (`win/paths` `editIniFile`, which reads the file again when it changed meanwhile).
  Every writer replaces the file whole (a temporary file, then `MoveFileEx`).
- **Quality sets.** `effect_groups::effective` turns the set (auto by the card's level, off, low, medium, high,
  custom) and the ten groups with their strength into the switches the renderer draws; `shown` and `materialize`
  serve the Effects page and the console (a switch changed under a set makes the set the player's own, in one
  write).

## The settings app

`MainWindow` (`main_window.cpp`) shows one page at a time from the list. Pages get `WM_COMMAND` and `WM_NOTIFY` only
for control ids from 1000 on, and a page is made anew at every change: state that must survive lives in the
`Model` or in a static. `Model` (`model.cpp`) holds the ini, the game's registry settings, the player's pending
changes, backups and profiles; it is tested on Linux over `tests/fake_windows.cpp`.

## In the game: console, frame counter, statistics

- **Console (`~`, `src/win/console.cpp`):** a layered window over the game; keys are read with `GetAsyncKeyState`
  and the game's window is subclassed so that it gets none while the console is open. Its rows come from the schema
  and the effect groups; Tab held turns the chosen effect or group off while held.
- **Frame counter (`fps_overlay.cpp`):** frames a second and the card's own milliseconds (`core/gpu_timing.hpp`;
  from the 64-bit renderer by `Msg::Timing`).
- **Statistics (with the player's leave):** the driver writes a line per stretch of play and per change
  (`core/usage.hpp`) into `uaro-graphics\usage.jsonl`; the app renames it, sends it to uaro.kiev.ua in parts and
  deletes it (`report_send.cpp` `sendUsage`). The site's side (Laravel, not in this repository) keeps each line once.

## AI textures

`uaro-render64.exe --pack` builds `uaro-textures.zip` from the game's pictures with the Real-ESRGAN models in
`dist/ai` (`src/host/pack_builder.cpp`, `core/texture_pack.cpp`, `core/ai`); the renderers take its pictures by
kind (ground, objects, sprites, effects, icons — `aiKindsShown`, changed live through `SetDrawQuality`).

## uaRO Studio

The design is `docs/superpowers/specs/2026-10-18-texture-studio-design.md`, its first phase's plan
`docs/superpowers/plans/2026-10-18-studio-phase1.md`.

- **Its own copy.** Started from `uaro-graphics\`, it copies itself to
  `%LOCALAPPDATA%\uaroGraphics\studio\<version>-<hex>\` and runs that copy (the last five builds kept), so that
  `update.bat` replaces the shipped file freely; one runs at a time (`Local\uaro-studio`).
- **The engine.** A copy of the installed `uaro-render64.exe` (`engine-<version>-<hex>.exe`, the last five driver
  builds kept) run as `--studio <pipe> --dir <game>`: one JSON object a line both ways over a named pipe with
  overlapped I/O (`core/studio_link`, `core/json`) - `hello`, `enlarge` (any model, loaded when first asked), `build`
  (a pack build with its status as progress, `core/studio_status`), `stop`, `quit`. A newer installed engine takes
  over between two jobs; a lost one is started again and its job tried once more (`core/studio_state` Queue).
- **The jobs** run one at a time on a thread of their own (`src/studio/jobs.cpp`): make (the chain `core/ai/chain`
  about the engine's network, atlas tiles `core/atlas_tiles`; the picture waits in `studio\made\`), keep, take
  out, the game's own, material, compact. Every write of the pack takes the builder's mutex and
  `Local\uaro-textures-zip` (`src/studio/pack_edit.cpp`); a job that would write while a game or a builder writes
  waits. Pause ends the engine, closes the archives and writes `queue.json` and `session.json`.
- **The catalog** (`core/texture_catalog`): every texture file with the archive that gives it and those it
  shadows, its pictures' keys (lower details, the server's colours), its maps and a ground texture's atlas grid;
  joined with the pack's index; kept in `studio\catalog.bin` and read again only for archives that changed - the
  builder takes it as its reading pass (`--catalog`). Quality numbers: `core/ai/picture_metrics`.
- **The pack's index** got three kinds of lines an older reader skips: `+<picture>` what it was made from, when and
  how; `!<key>` the game's own always; `#<key>` a texture's material scales. The entries carry the time they were
  written. The builder, the game's own writer and Compact keep all three.
- **The game linked** (the `gpu` code both renderers share, `win/context.cpp` for the keys): the map's report
  `uaro-graphics\seen\<map>.txt` (`AiTextures::writeSeen`), the X-ray (`[debug] xray`, Ctrl+Shift+F9: the state's
  colour through the fog's constants), the pick (Ctrl+Shift+F8 reads the game's own cursor, `core/pick` finds the
  draw, `link\pick.txt` and `Local\uaro-studio-pick`), A/B (Ctrl+Shift+F7), the live reload (`link\changed.txt`
  and `Local\uaro-studio-changed`, looked at once a frame; the I/O thread opens the pack again) and the material's
  scales (`material_ps` through `sourceClamp.xyz`; the studio's preview is `core/material_maps`). Direct3D 12 only;
  the bridge carries the keys as `Msg::SetStudio`.

## Shaders

One HLSL source, `src/win/gpu/shaders.cpp`, compiled on the player's computer by `d3dcompiler_47.dll` (FXC) and kept
in `uaro-graphics\shaders-32.bin`, `shaders-64.bin` and `shaders-d3d11.bin`. A shader FXC will not take is compiled
again with `UAGFX_SAFE` (without the newest features; the cache remembers it, the statistics count it). Check every
change with DXC `-HV 2018`: FXC keeps a loop's variable after the loop and refuses keywords as names.

## Checks before a release

`tests/` (Linux), `tools/check_dll.py` (`ddraw.dll` imports only `kernel32`, `user32`, `gdi32`, `msvcrt`),
`tools/check_exe.py` (the settings app imports only what Windows 7 has), `tools/check_studio.py` (uaRO Studio is
64-bit and imports from Windows' own DLLs alone), `tools/check_texts.py` (every text is
shown somewhere), `tools/check_package.py` (the archive), DXC over every shader. Lines stay within 120 characters,
code comments in English; new visual effects come switched off.
