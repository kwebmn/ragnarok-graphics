# Texture studio — design

**Why.** On 2026-10-18 the user built a new texture pack overnight, started the settings app in the morning and could
not tell what had been made and what had not, nor why Comodo's beach (cmd_fild02) came out smooth: the pack's
picture, the model that made it, or a newer archive's version of the sand texture "overwrote the beach file". Nothing
in the settings app answers that today:

- the pack names a picture by the game texture's pixels (`ai::TextureKey`: FNV-1a over the size and the ARGB values,
  `world/256x256/<hash>.webp`) - never by the file it came from, so no screen can put a picture next to its file;
- of two archives in DATA.INI with the same file the first wins (`pack::GameFiles::open`) and the other version is
  dropped without a word;
- the pictures carry no date (the ZIP entries have 1980-01-01, `kDosDate`);
- seeing a picture beside its original, or making one again with another model, means a whole build; the AI page's
  preview (`--pack preview`) shows one sample picture of a group, 128 pixels of its middle;
- the game's own idea of a texture - drawn from the pack, made by the network, or left as the game's and why - is in
  the log as counts only ("not in the pack - ground 3, objects 12").

The studio is one place to see every texture's state, compare a picture with its original, make it again with any
model and look before keeping it, take it out, and understand why the game shows what it shows. The user: "мне надо
богатый функционал" (a rich tool, not the thin one there is now).

**Status.** Design only. Grounded in the code as of 2.2.5 (one map's textures on the Texture pack page,
`--pack drop`, `pack::Writer::remove`, `core/map_textures`).

## Goals

1. **Know every texture.** For each texture the game has: the archive it comes from, the other archives that hold a
   version of it (shadowed), its key, whether the pack has its picture, the model and group that made it, when, and
   the maps that use it (their ground and their models).
2. **See it.** The original and the picture side by side at any zoom, nearest or smooth, a wipe between them, the
   texture tiled 3x3 (its edges wrap on the ground), the difference, and numbers that flag a bad picture: its tone
   against the original's and how much of the original's detail it kept.
3. **Make it again and decide.** One texture or a selection, any model (and the general model's denoise): the
   result is shown first and goes into the pack only when kept; discarded otherwise. Batches over a map, a folder or
   a filter, in a queue with progress and a log.
4. **Take out, or keep the game's.** A picture out of the pack; a texture marked "the game's own always" that neither
   the pack nor the game's own making ever pictures again.
5. **Know what the game did.** Per map: every texture the game drew, as the game drew it - the pack's picture, the
   network's, or the game's own and why (not in the pack, soft light, soft alpha, out of the sides, its kind off,
   marked the game's).
6. **Know the pack.** Its pictures by group, model and kind; the pictures no current file has (orphans: a texture
   replaced by a newer archive); the textures without a picture (missing); its size and the space taken-out pictures
   still hold, with a compaction.

## Non-goals

- Painting pixels: the studio is no image editor (phase 3 exports a picture for an outside one and takes it back).
- Training or adding networks; the three models stay (general, anime, anime 6B).
- Replacing the builder: a whole pack is still made on the Texture pack page.
- Distributing packs (the site's download stays as it is).

## Anti-goals

- The game slowed down: the driver's part is a short file written when a map is left, nothing in a frame.
- A pack an older driver cannot read: new index lines are of a kind older readers skip (see Data).
- A picture lost without a word: every replacement and removal is an explicit Keep or Take out, and (phase 3) the
  replaced pictures are kept for undo.
- A tool that freezes: catalogs, thumbnails and networks run in the worker; the window only shows.

## Constraints

- `uaro-graphics.exe` is a 32-bit Win32 program (mingw i686) with its own UI kit (`app/ui.hpp`: panels, list views,
  checks, choices, buttons, images), texts in three languages (`core/settings/strings`, `tools/check_texts.py`),
  control ids from 1000 (its pages get WM_COMMAND only for those), pages rebuilt after every change.
- The networks run only in `uaro-render64.exe` (64-bit, Direct3D 12 compute) - the studio drives it as a worker.
- `ddraw.dll` imports only kernel32, user32, gdi32 and msvcrt: its report is a plain file.
- One writer of the pack at a time: the mutexes `Local\uaro-texture-pack-builder` and `Local\uaro-textures-zip` (a
  game with `[ai] generate` holds the second while it runs); a game reads the pack's directory at its start.
- The pack holds some 300 000 pictures, several GB: small changes never rewrite it (`Writer::add` with replace,
  `Writer::remove`); only the compaction does (`pack::rewrite`).
- Keys are of the pixels: two files with the same pixels share one picture; a texture changed by a newer archive is
  a new key.

## How it is made (my calls)

1. **A window of the settings app**, "Texture studio", opened by a button on the Texture pack page (and from the
   Ready textures page): one program for players and the developers, its texts and its look; a top-level window of
   its own, not a page (it needs room and a layout of panes).
2. **A worker**: `uaro-render64.exe --studio --dir "<game>" --pipe <name>`, started by the window and ended with it.
   It reads the game's archives (the builder's code), decodes textures, makes thumbnails, runs the networks (models
   loaded when first asked, then kept), writes the pack. One JSON object a line over a named pipe each way: requests
   from the window, progress and results back. Files it makes go into `uaro-graphics\studio\` (thumbnails, results
   waiting for Keep, the catalog's cache).
3. **A catalog** (`core/texture_catalog`, plain data and tests): the game's texture files (normal names) with their
   archive and shadowed archives, size, key(s) (and the lower texture details' keys), the builder's group, the maps
   that use them (from the maps' `.gnd`, `.rsw`, `.rsm` - `core/map_textures`), joined with the pack's index (picture
   there or not, model, group, date, provenance) and the orphans. Built by the worker on all cores like the
   builder's reading pass (minutes the first time), cached in `uaro-graphics\studio\catalog.bin` and brought up to date
   by the archives' sizes and times (an archive that changed is read again, the others not).
4. **The pack's index grows two kinds of lines** older readers skip (they take any unknown line for a picture of no
   kind that the pack does not have - as the 1.6 model lines):
   - `+<picture>\t<file>\t<archive>\t<made>` - what a picture was made from and when (ISO date), written by the
     builder and the studio for every picture they make;
   - `!<key>` - the game's own always: the driver shows that texture as the game has it, the builder and the game's
     own making leave it out.
   And the ZIP entries get the time they were written (the DOS date and time), not 1980-01-01.
5. **The driver's report of a map**: when a map is left (and at the game's end), `uaro-graphics\seen\<map>.txt` - a
   line a texture it drew there: key, size, kind, and how it was shown (`pack`, `made`, or the game's with the
   reason: `missing`, `soft`, `alpha`, `size`, `kind-off`, `marked`, `budget`). At most a few thousand lines, the last
   20 maps kept. The worker maps the keys back to files through the catalog.

## The window

```
+--------------------------------------------------------------------------------------------------------------+
| Texture studio — uaro-textures.zip: 302 443 pictures, 18.4 GB (1.2 GB taken out, to compact)   [Compact...]  |
+-------------------+-----------------------------------------------+------------------------------------------+
| Navigator         | Textures (cmd_fild02 — its ground: 11)        | 필드바닥\해변모래바닥01.bmp                 |
|  Maps   [search]  |  [thumb|thumb] 해변모래바닥01.bmp  ⚠ detail   |  [ original ][ picture ]  zoom 4x  ⟷ wipe  |
|   > cmd_fild02 ◀  |  [thumb|thumb] 해안모래경계_04.bmp            |  ( ) nearest (•) smooth  [ ] tiled 3x3       |
|     cmd_fild01    |  [thumb|  —  ] 해안경계절벽2_02.bmp  missing  |  [ ] difference                             |
|  Folders          |  ...                                          |  from: newdata.grf (also in data.grf ⚠)     |
|  States           |                                               |  key 256x256 0f3a…  picture: anime, 10-18 02:14 |
|   missing  1 204  |  sort: name | state | detail kept | model      |  maps: cmd_fild01, cmd_fild02, …            |
|   orphans    880  |  [x] ground [x] models                        |  tone shift +6 % · detail kept 22 % ⚠        |
|   game's own  14  |                                               |  [Make again ▾ general] [Keep] [Discard]    |
|  Seen in game     |                                               |  [Take out] [Game's own always]             |
+-------------------+-----------------------------------------------+------------------------------------------+
| Jobs: making again 11 pictures of cmd_fild02 by general — 4/11, 9 s left                [Stop]   log ▾      |
+--------------------------------------------------------------------------------------------------------------+
```

- **Navigator.**
  - Maps: all the game's maps (its `.rsw` files); a search box; the last map of the game's log first.
  - Folders: the texture folders as a tree.
  - States: in the pack, missing, orphans, made by each model, the game's own, flagged (detail kept under 40 % or
    tone shift over 10 %).
  - Seen in game: the maps of the driver's reports, newest first.
- **Textures.** A virtual list view: a thumbnail pair (original | picture, 64 px), the file, its size, its state and
  model, detail kept. Sorts by any column; checkboxes for a selection; the map's filter ground / models.
- **Inspector.** One texture.
  - The two images with one zoom (1x-16x, the wheel) and one pan (the drag), nearest or smooth.
  - A wipe: one image with a line to drag between the two.
  - Tiled 3x3, to see the seams where the ground repeats it.
  - Difference: |picture shrunk to the original's size - original|, as a heat map.
  - Information: the archive, the shadowed archives, the key, the picture's model, group and date, the maps that use
    it.
  - Numbers: tone shift is the mean colour of the picture against the original's. Detail kept is the high-frequency
    energy (a Laplacian) of the picture shrunk to the original's size, against the original's own, in %. The
    beach's smooth sand should read low; the threshold is set on real pictures.
  - Actions: Make again with a model (and denoise for general). The result replaces the right image, marked "not kept".
    Keep puts it into the pack, Discard drops it. Take out removes the picture. Game's own always / Allow again
    toggles the key's mark.
- **Jobs.** The queue of the worker's work with progress, time left and Stop; a log of each job (what was read, made,
  skipped and why).
- **The pack's line.** The pictures, the size, the space taken-out pictures still hold, and Compact (the
  `pack::rewrite` the Ready textures page uses, with a progress).

## How the user's morning reads in it

The names and numbers here are made up to show the flow.

1. Maps → cmd_fild02, its ground: 해변모래바닥01.bmp says "from newdata.grf, also in data.grf" (the guess about the new
   archive answered at once) and "picture: anime, 10-18 02:14, detail kept 22 % ⚠".
2. The inspector at 4x, smooth: the picture beside the original's grain; the wipe shows it lost.
3. Make again with general → the result on the right in a few seconds, detail kept 81 % → Keep. Or Game's own always,
   if no model does sand well.
4. Start the game: the beach with the new picture. Seen in game → cmd_fild02 confirms: 해변모래바닥01.bmp `pack`.
5. States → orphans: the pictures of the textures the new archive replaced (no file has their pixels now) - taken out
   in one go; States → missing: what the new archive brought that has no picture - made in a batch.

## Data and code

| Piece | Where | Tests |
|---|---|---|
| Catalog rows, joining, filters, sorting, the cache file | `core/texture_catalog.{hpp,cpp}` | with fake archives and a fake index |
| Archives with their shadowed versions | `pack::GameFiles` keeps every archive of a name, in order | the order and the shadowing |
| Map → textures | `core/map_textures` (2.2.5) | exists |
| Index lines `+` and `!`, entry times | `core/texture_pack` | written, read back, an older reader's view |
| Picture metrics (tone shift, detail kept) | `core/ai/picture_metrics` | on made-up pictures: a copy, a blur, a tint |
| The map's report: write and read | `core/seen_report` | a round trip, the caps |
| The worker's protocol (JSON lines) | `core/studio_protocol` | each request and reply round-trips |
| The worker | `host/studio_worker.cpp` (builder code reused) | by hand on Windows |
| The window, its image control (zoom, pan, wipe, tiles, difference) | `app/studio_window.cpp`, `app/image_compare.cpp` | by hand |
| The driver: the report, the `!` mark honoured | `win/gpu/ai_textures.cpp`, `ai11.cpp` | the mark's rule in core |

## Phases

The world's textures (`data\texture\`: the ground, the models) come first. Sprites, effects and the interface follow
in phase 2: one `.spr` file holds many frames, each its own key and picture, and a head's frames come in every hair
colour (`pack::palettesOf`); their rows are a file with its frames, opened like a folder.

1. **See** (read-only):
   - the catalog in the worker, with the shadowed archives;
   - the window with its navigator, list and inspector;
   - the metrics;
   - the pack's line;
   - the driver's report of a map and the "Seen in game" view;
   - the builder writing `+` lines and entry times.
2. **Act**:
   - Make again with a preview, then Keep or Discard;
   - Take out;
   - Game's own always (`!`), honoured by the driver and the builder;
   - batches and the jobs queue;
   - Compact.
3. **More**:
   - undo: replaced and taken-out pictures kept in `uaro-graphics\studio\undo.zip` for 14 days;
   - rules for whole builds (a folder or a kind to a model: sand and grass by general);
   - export to PNG and import of an edited picture;
   - a picture compared across all three models at once.

## Left to the builder

The pixel layout of the window; the pipe's message names; the catalog cache's format; thumbnail sizes and their cache;
the metrics' exact formulas and thresholds (the ones above are starting points); how many threads the worker reads
with; the text of every label (three languages, by the project's rules).
