# uaRO Studio — design

A program of its own for making and testing the texture pack's pictures and the driver's picture: textures, frames,
the pack, the game linked live.

## Why

On 2026-10-18 the user built a new texture pack overnight and in the morning could not tell what had been made and
what had not, nor why Comodo's beach (cmd_fild02) came out smooth. The cause turned out to be the pack's picture of the
sand (made by the anime model). Nothing showed it:

- **The pack names pictures by pixels, not files.** A picture's name is the game texture's pixels (`ai::TextureKey`:
  FNV-1a over the size and the ARGB values, `world/256x256/<hash>.webp`), never its file. No screen can put a picture
  next to its file.
- **Shadowed archives vanish silently.** Of two archives in DATA.INI holding the same file, the first wins
  (`pack::GameFiles::open`) and the other version is dropped without a word.
- **Pictures have no date.** Their ZIP entries carry 1980-01-01 (`kDosDate`).
- **No way to look before a full build.** Seeing a picture beside its original, or making it again with another
  model, means a whole build. The AI page's preview shows one sample of a group, 128 pixels of its middle.
- **The game reports counts only.** What it showed - the pack's picture, the network's, or its own and why - reaches
  the log only as counts.
- **The settings app froze at Stop.** It read the whole pack on its window's thread, and a pack being written header
  by header (fixed in 2.2.5).
- **The builder's reading took long.** It went through every hair colour the client has (128 a style; 2.2.5 limits
  it to the server's 0-8).
- **The progress was opaque.** It said "files N of M" and nothing of what it was doing (2.2.5 shows the file).

The user asked for a rich tool of its own: «отдельный файл ... запустить и работать в нем, тестировать функционал,
ставить на паузу, обновлять драйвер и не перезапускать саму программу», with the driver's frames tested in it too,
and every idea of the review below («вноси все»).

**Status.** Phase 1 built on the branch `studio` (2026-10-18, not released yet), as planned in
`docs/superpowers/plans/2026-10-18-studio-phase1.md`; the calls made on the way are in its ledger and in
`docs/ARCHITECTURE.md`. Grounded in the code as of 2.2.5.

## Goals

**The program**

- G1. A program of its own, `uaro-studio.exe`, that runs all day beside the game.
- G2. It never holds the driver's files, so the driver can be updated while it runs.
- G3. Its engine - the networks and the replay, the installed driver's own - is switched to a new driver between two
  jobs, without restarting the studio. A crashed engine is started again.
- G4. Pause lets go of everything: the engine, the pack, the game's archives. Resume finds what changed and goes on
  with the queue.
- G5. The session is kept - the place, the selection, the filters, the queue. A new studio opens where the old one
  stood.

**Textures**

- T1. Know every texture:
  - its archive, and the archives it shadows;
  - its key, and its keys at the lower texture details and in the server's colours;
  - its picture in the pack: model, group, date, what it was made from;
  - the maps that use it, and how it lies on them (a ground atlas's tiles);
  - pictures without a file (orphans) and files without a picture (missing).
- T2. See it:
  - the original beside the picture, at one zoom and pan, nearest or smooth;
  - a wipe between them;
  - the texture tiled 3x3, and laid as the map lays it;
  - the difference;
  - the material the driver builds from it (relief, height, gloss).
- T3. Quality flags on every picture:
  - smoothed (detail kept);
  - washed (tone shift);
  - banded gradients;
  - ringing and halos;
  - a colour-key fringe;
  - spoiled alpha;
  - seams when tiled.
- T4. Make it again and decide:
  - one texture or a selection;
  - by any model, with a processing chain (below);
  - the result shown first, Keep or Discard.
- T5. The processing chain, per texture or by rule:
  - before the network: deblock and denoise a JPG, repair the colour key;
  - the network, x4 or x2, with the general model's denoise;
  - after it: match the tone to the original, restore the original's grain, sharpen, clean the alpha;
  - blends: two models by an edge mask, or the network's picture and the original by a slider.
- T6. Atlas tiles: a ground texture used as a 4x4 (or 2x2) atlas made tile by tile, each tile's edges wrapped, so that
  no tile takes its neighbour's edge. In the game a tile lies beside other tiles than in the atlas.
- T7. Materials by texture: the relief, the height and the gloss the driver makes from a picture, scaled or turned
  off for that texture (the sand that must not shine, the stone with more relief). Kept in the pack, honoured by the
  driver.
- T8. Rules by folder, kind or name: a model and a chain (sand and grass by general with the grain restored).
- T9. Export a picture to PNG for an outside editor, take the edited one back (its size and alpha checked).
- T10. Make first what the game draws most (by the game's reports).

**The game, linked**

- L1. X-ray: on a key, the game tints every texture by its state - the pack's (green), made in the game (blue),
  missing (red), the game's own by a rule (grey) - for the coverage of a place at a glance.
- L2. Under the cursor: the texture there - its file, archive, model, date. A key turns its picture off and on in the
  game (A/B of one texture); another sends it to the studio, which opens it.
- L3. What the game drew: per map, every texture as it showed it and why.
- L4. Live: what the studio keeps, takes out or changes in a material shows in the running game within a second,
  with no restart.
- L5. Coverage of a map: the share of its drawn textures with pictures, its missing ones made in one go.

**Frames**

- F1. Capture a frame in the game (the plan 2.3 item B2 design, `docs/superpowers/specs/2026-10-18-frame-capture-design.md`).
- F2. Replay it in the studio:
  - by the installed driver's engine;
  - with any effects, live by sliders;
  - a pixel difference between two replays;
  - the GPU's time of each pass.
- F3. Versions side by side: the engines of the last driver versions kept, one frame drawn by two of them - did the
  update fix it.
- F4. Scene sets: frames of the known places (c_tower2, gl_church, cmd_fild02, alde_dun03, rain, night) drawn by a new
  driver in one go, with the places where the picture changed marked - a check before a release.
- F5. A player's report (its code R-...) opened: its settings applied to a replay, its screenshots beside it.

**Generation**

- N1. Stages named, each with its progress, rate, time left and every thread's current file: archives, textures,
  colours, making, writing. At the end, a timeline of where the time went.
- N2. The catalog read once and kept: a run reads only the archives that changed, and making a few pictures takes
  seconds.
- N3. The server's colours only (2.2.5); the archives' directories opened once for all threads.
- N4. Pause and resume of a run, also across a driver update (the queue on disk).

**The pack**

- P1. An overview:
  - by group, model and kind;
  - orphans and missing;
  - the size, and the space taken-out pictures still hold, with Compact.
- P2. History: every replaced or taken-out picture kept 14 days, undone with a click.
- P3. Between two packs:
  - the difference (what a build changed);
  - a merge (packs made on two computers);
  - one map's pictures as a small pack to share, and taken back.
- P4. Duplicates found; pictures encoded again at another quality to save room.
- P5. Several packs; the game switches between them live (all general against all anime).
- P6. Memory and loading: a map's pictures in video memory, their loading time, stutter as they come.

**Logs**

- O1. The game's and the engine's logs live, filtered by map or texture; a texture's key opens it.

## Non-goals

- Painting pixels (T9 hands a picture to an outside editor).
- Training networks or adding models beyond general, anime and anime 6B.
- Distributing packs (the site's download stays as it is).
- Replacing the settings app's pages: players keep them; the studio is the developers' and testers' tool.

## Anti-goals

- **A slower game.** The driver's parts are a short file at a map's change, an event looked at once a frame, and the
  X-ray as a constant in a shader.
- **A pack an older driver cannot read.** New index lines are of a kind older readers skip.
- **A picture lost without a word.** Keep and Take out are explicit, and history keeps what they replaced.
- **A studio that freezes.** Nothing heavy on its window's thread: the catalog, thumbnails and files on threads, the
  networks and replays in the engine.
- **A studio in the way of an update.** No file of the driver's held open while it runs.

## Constraints

- **Code base.** The studio is C++ with the project's Win32 UI kit (`app/ui.hpp`) and texts in three languages
  (`core/settings/strings`, `tools/check_texts.py`), 64-bit, linked statically (no runtime DLLs).
- **Networks.** They run in the 64-bit renderer's code (`gpu::Renderer::startAiForPack`, `enlargeForPack`). The
  studio's engine is that renderer: `uaro-render64.exe --studio`.
- **The driver's imports.** `ddraw.dll` imports only kernel32, user32, gdi32 and msvcrt. Its part of the link is
  files and named events.
- **One writer of the pack.** The mutexes `Local\uaro-texture-pack-builder` and `Local\uaro-textures-zip` stand; a game
  with `[ai] generate` holds the second while it runs.
- **Size.** About 300 000 pictures, many GB. Small changes never rewrite the pack (`Writer::add` replace,
  `Writer::remove`); only Compact does (`pack::rewrite`).
- **Keys.** A key is the pixels: two files with the same pixels share a picture; a texture a newer archive changed is a
  new key.
- **Our server.** Hair colours 0-8 and clothes 0-77 (uAthena `battle/client.conf`) are the defaults; other values can
  be set.

## How it is made (my calls)

### The program and its engine

- **Where it lives.**
  - `uaro-studio.exe`, 64-bit, ships in the driver's zip in `uaro-graphics\`.
  - Started there, it copies itself to `%LOCALAPPDATA%\uaroGraphics\studio\<its build>\` and starts that copy, so
    `update.bat` can replace the installed one while it runs.
  - It keeps the last five builds' copies.
- **The engine.**
  - A copy of the installed `uaro-render64.exe` in the same place (`engine-<build>.exe`), run as
    `--studio --dir "<game>" --pipe <name>`. It has its own shader cache (`studio\shaders-64.bin`) and reads the
    models when first asked, then closes their files.
  - The studio watches `uaro-graphics\version.txt` and the installed engine's time.
  - When a driver update brings a new engine, the studio switches to it after the job running (or at once, if asked).
  - The engines of the last five driver versions are kept for F3.
- **The pipe.**
  - One JSON object a line, each way, over a named pipe (`core/studio_protocol`): requests with an id; progress and
    results with that id; events.
  - A broken pipe, or an engine gone, is the engine's crash: it is started again, and the job it had is tried once
    more, then marked failed.
- **Pause.**
  - The engine ends, and the pack and the archives are closed.
  - The queue (`studio\queue.json`) and the session (`studio\session.json`) are written.
  - Resume:
    - starts the newest engine;
    - finds what changed by sizes and times (the driver's version, DATA.INI, the archives, the pack);
    - brings the catalog up to date;
    - goes on with the queue.
- **Its window's thread** only shows. The catalog, thumbnails and metrics run on a pool of threads; results come back as
  window messages.

### The catalog

`core/texture_catalog` - plain data with tests:

- The game's archives in DATA.INI's order, each file's providing archive and the archives it shadows
  (`pack::GameFiles` keeps every archive of a name).
- Each texture file: its keys (its own, its lower details' 1/2 and 1/4, and for sprites a frame's key in each of the
  server's colours), size, the builder's group, the maps that use it (`core/map_textures`) and how: a ground texture's
  UV rectangles on each map (to find a 4x4 or 2x2 atlas and which tiles lie beside which).
- Joined with the pack's index: picture there or not, model, group, date, provenance, material overrides, the game's
  own mark.
- The orphans (pictures no current key has) and the missing (keys without a picture).
- Built by threads (each with its own handles to the archives, one shared directory of names), cached in
  `studio\catalog.bin` and brought up to date by the archives' sizes and times: an archive that changed is read again,
  the others not.
- The builder (`--pack build`) takes the same catalog - its reading pass is the catalog's update (N2).

### The pack's index

New lines older readers skip (they take an unknown line for a picture of no kind that the pack does not have):

- `+<picture>\t<file>\t<archive>\t<made>\t<model>\t<chain>`: what a picture was made from, when, by what. Written by the
  builder and the studio for every picture they make.
- `!<key>`: the game's own always. The driver shows that texture as the game has it; the builder and the game's own
  making leave it out.
- `#<key>\tgloss=<x>\trelief=<x>\theight=<x>`: material overrides (T7) - scales of the driver's material, 0 turns a part
  off.

The ZIP entries get the time they were written.

### The game's side (driver)

- **Reports (L3).** `uaro-graphics\seen\<map>.txt` on leaving a map and at the game's end: a line a drawn texture -
  key, size, kind, how shown (`pack`, `made`, or the game's: `missing`, `soft`, `alpha`, `size`, `kind-off`, `marked`,
  `budget`) and how often drawn. The last 20 maps kept.
- **X-ray (L1).** A console switch and a key (Ctrl+Shift+F9). The draw's state goes to the shader as a colour in a free
  constant, mixed over the texture at 35 %.
- **Under the cursor (L2).**
  - Ctrl+Shift+F8 finds the frame's last draw whose screen triangle holds the cursor's pixel (the record's transformed
    vertices), takes its texture's key and writes `studio\link\pick.txt`, then signals `Local\uaro-studio-pick`.
  - Ctrl+Shift+F7 turns that texture's picture off and on in the game (a list of keys the driver shows as the game's
    while set).
- **Live (L4).** The studio writes the changed keys to `studio\link\changed.txt` and signals
  `Local\uaro-studio-changed`. The engine of the game:
  - looks at the event once a frame;
  - opens the pack's directory again on its reading thread;
  - drops the changed keys' copies, which come back from the pack as they are drawn;
  - drops the changed keys' materials, which are made again with their overrides.
- **Overrides (T7).** The material pass (`material_ps`) takes three scales a texture.

### Making pictures (the engine)

- **The chain (T5).** Each step on the CPU or in a compute shader (in the engine):
  - before the network: deblock (a JPG's 8x8 edges smoothed where flat); colour-key repair (as `prepareInput`).
  - the network: x4 (x2 by halving).
  - tone match: mean and spread of each channel matched to the original's, in a perceptual space.
  - grain restore: the original's high frequencies, enlarged by bicubic, added back at a strength.
  - sharpen: contrast adaptive.
  - alpha cleanup: the network's alpha snapped to the original's colour key, the edge without a fringe.
  - blends: two models' results mixed by an edge mask (a Sobel on the original), or the result and the original by a
    slider.
- **Atlas tiles (T6).** The texture cut into the tiles the catalog found, each tile wrapped at its edges (tiled
  padding, as the seamless modes of ESRGAN tools), enlarged on its own, put back. The network never sees a
  neighbour's edge.
- **Metrics (T3)** (`core/ai/picture_metrics`, plain data with tests):
  - detail kept: the Laplacian energy of the picture shrunk to the original's size, against the original's;
  - tone shift: the mean colour's distance;
  - banding: runs of equal steps in smooth gradients;
  - ringing: overshoot beside edges;
  - fringe: magenta-leaning texels beside the colour key;
  - alpha: the shrunk alpha against the original's;
  - seams: the tiled picture's difference across its wrap.
  The thresholds are set on real pictures.

### Frames (the engine)

- **Capture.** As B2's design: the driver writes `uaro-graphics\captures\<map>-<time>.ucap`.
- **Replay.** The engine's `--studio` replays a capture into an image: with the effects asked, at the size asked,
  returning the image and the passes' GPU times.
- **Versions.** Two engines (two driver versions) replay the same frame for A/B.
- **Scene sets.** A named list of captures with the settings to draw them by; a run draws each with the current engine
  and keeps the images by version. The gallery shows a version against the last, with the changed pixels marked.
- **A report.** A report's settings (its `uaro-graphics.ini` lines) applied to a replay; its screenshots shown beside
  it. Taken from the site (the admin's address and token, set once) or from a file.

## The window

Tabs over one jobs bar:

- **Textures.**
  - The navigator: maps, folders, states (missing, orphans, by model, flagged, the game's own, with overrides), the
    game's reports by map.
  - The list: a virtual list view - thumbnail pair, file, state, model, flags, detail kept; sortable, with selection.
  - The inspector:
    - the compare control (two panes or a wipe; zoom 1x-16x by the wheel; pan by the drag; nearest or smooth;
      tiled 3x3 or as the map lays it; difference);
    - the information (archive and shadowed, keys, picture, provenance, maps, atlas);
    - the flags;
    - the material (its three maps, the overrides by sliders);
    - the actions: Make again (model, chain, atlas tiles) with Keep or Discard; Take out; Game's own always;
      Export / Import; History.
- **Frames.** Captures and scene sets; a capture replayed beside another replay (another engine version, other
  effects) with a wipe and the difference; effect sliders; the passes' times; a report's settings.
- **Pack.** The overview, Compact, the difference against another pack, merge, a map's small pack out and in,
  duplicates, encoding quality, several packs.
- **Rules.** Folder, kind or name to a model and a chain; applied to a selection or kept for builds.
- **Log.** The game's and the engine's logs live, filtered; keys clickable.
- **The bar.**
  - Pause / Resume.
  - The engine's version and state; a newer engine waiting.
  - The game: running or not, and its pack link on.
  - The jobs: each with its stage, progress, rate, time left and every thread's file, Stop, and its log.

## Phases

**Phase 1 - the program, textures, the game linked.**

- The program (G1-G5), the engine and its pipe.
- The catalog (T1), the inspector (T2), the flags (T3).
- Make again with Keep and Discard (T4), the chain (T5), atlas tiles (T6), materials by texture (T7).
- X-ray (L1), under the cursor with A/B and the studio's opening (L2), the game's reports (L3), live (L4).
- The pack's line with Compact (P1 in part).
- The generation's stages (N1-N3) in the engine's runs.

**Phase 2 - frames and the pack.**

- Frames: capture and replay (F1, F2), versions (F3), scene sets (F4), reports (F5).
- Generation: pause and resume of runs (N4).
- The pack: history (P2), between packs (P3), duplicates and quality (P4).
- Rules (T8), coverage (L5).

**Phase 3 - the rest.**

- Export and import (T9), most drawn first (T10).
- Several packs live (P5), memory and loading (P6).
- Logs (O1).

## Left to the builder

- The pixel layout.
- The pipe's message names and fields.
- The catalog cache's format.
- Thumbnail sizes.
- The metrics' formulas and thresholds (the ones above are starting points).
- The chain's parameters.
- The X-ray's colours.
- The hotkeys, if they clash with the client's.
- The texts in three languages, by the project's rules.

## Prior art

- **NVIDIA RTX Remix** (capture, AI texture tools with generated material maps, replacement in the running game, a
  REST API).
- **Seamless and tiled modes of ESRGAN tools**, and local padding against tile seams (arXiv 2309.02340).
- **chaiNNer's chains of image steps.**
