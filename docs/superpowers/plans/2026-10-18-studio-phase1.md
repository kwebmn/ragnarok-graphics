# uaRO Studio — Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> The user works inline («все в инлайн режиме»): executing-plans, no parallel agents.

**Goal:** `uaro-studio.exe`, a program of its own: every game texture with its pack picture side by side, weak
pictures flagged, made again by any model and chain (atlas tiles too) with Keep or Discard, materials per texture,
and the running game linked (X-ray, the texture under the cursor, A/B, what was drawn, changes live).

**Architecture:** Portable logic in `src/core` with Linux tests (JSON, the index's new lines, the catalog, metrics,
the chain, atlas tiles, the pipe's lines, the link's files, the session and the queue). The studio (`src/studio`,
Win32, 64-bit) owns the window, the catalog's threads and every write of the pack. Its engine is a copy of the
installed `uaro-render64.exe` run as `--studio`: the GPU's work (the networks, the material pass, full builds) over a
named pipe of JSON lines. The driver's part (the `gpu` code both `ddraw.dll` and `uaro-render64.exe` build, and
`win/context.cpp`) writes what it drew, tints by state, answers a pick, hides a texture for A/B and reloads on the
studio's event.

**Tech Stack:** C++17, MinGW-w64 (i686: ddraw.dll, uaro-graphics.exe; x86_64: uaro-render64.exe, uaro-studio.exe),
Win32 and comctl32, Direct3D 12, HLSL as FXC takes it, libwebp, the project's test runner (`tests/test.hpp`).

**Spec:** `docs/superpowers/specs/2026-10-18-texture-studio-design.md` (phase 1: G1-G5, T1-T7, L1-L4, P1 in part,
N1-N3).

## Global Constraints

- Work in the worktree `/root/ragnarok-graphics-studio`, branch `studio` cut from main after this plan's commit; main
  stays releasable (2.2.5). No push, no release until the user says «выпускай».
- C++17; `-Wall -Wextra` without a warning in the three builds; lines ≤ 120; comments in English, in the code's own
  voice, new ones tagged `2.3:`.
- The gate after every task: `bash $S/gate-studio.sh` prints `GATE OK` ($S = the session's scratchpad; verify4 with
  R=the worktree: the 32-bit build, check_dll, every shader by DXC -HV 2018, check_texts, the Linux tests; the 64-bit
  build; no long line).
- `ddraw.dll` imports kernel32, user32, gdi32 and msvcrt only; its part of the link is files and named events.
- Every text a user sees: `core/settings/strings` in English, Ukrainian and Russian; `tools/check_texts.py` passes.
- New index lines begin with `+`, `!` or `#`; a 2.2.5 reader skips them.
- One writer of the pack: each studio write takes `Local\uaro-texture-pack-builder` and `Local\uaro-textures-zip`
  without waiting, writes, closes the pack and lets both go.
- The studio holds no file of the driver's: it runs from `%LOCALAPPDATA%\uaroGraphics\studio\<build>\`, its engine
  is a copy there.
- Nothing heavy on the studio's window thread: catalog, thumbnails, metrics, pack writes on threads; results as
  window messages.
- The driver's cost: a file at a map change, one `WaitForSingleObject(event, 0)` a frame, the X-ray a constant; the
  X-ray off by default.
- The server's colours by default: hair 0-8, clothes 0-77.
- Commits: KWEB Team <kwebmn@users.noreply.github.com>, the message in Russian, ending with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. The pack busy (a game with `[ai] generate` holds `Local\uaro-textures-zip`, or a builder runs): a Keep, Take out
   or material change never writes; the job waits as "the pack is busy" and goes on when it is free — Task 9
   `queue_waits_while_the_pack_is_busy`.
2. A `catalog.bin` of an older format, cut short or broken: it is refused and the catalog built anew, never a crash —
   Task 7 `catalog_cache_refuses_bad_bytes`.
3. Names out of ASCII: CP949 file names and a Cyrillic `%LOCALAPPDATA%` cross the pipe, the session and the link's
   files intact — Task 1 `json_keeps_utf8_and_escapes`, Task 8 `pick_and_seen_keep_cp949_names`.
4. The engine gone in the middle of a job (a crash, or the update replaced it): the job is tried once more on a new
   engine, then failed; a newer engine waits for the job running — Task 9 `queue_retries_once_after_the_engine_is_lost`.
5. A pack picture of another size than 4 times (x2 runs, a lower detail's alias): metrics and the compare view scale
   it and never read past it — Task 5 `metrics_take_any_scale`, Task 15 `view_maps_any_picture_size`.

## Rulings in this plan (my calls where the spec leaves room)

- The studio writes the pack; the engine only computes (networks, material pass, full builds). A crashed engine
  never leaves a pack without its directory.
- The engine's shader cache is `<game>\uaro-graphics\studio-shaders-64.bin`: the renderer opens its cache by ANSI
  paths and `%LOCALAPPDATA%` may be out of the ANSI code page. Every other path the engine gets comes as UTF-8 on the
  pipe and is opened by wide APIs.
- The studio opens the game's archives for a job and closes them when idle 30 s, not only at Pause: the game's
  patcher may write data.grf.
- X-ray and the pick on Direct3D 12 (the driver's and the 64-bit renderer's); Direct3D 11 gets the reports, the
  game's own marks and the live reload.
- The Log tab in phase 1 shows the studio's and the engine's lines; the game's logs filtered are O1 (phase 3).
- "Laid as the map lays it" draws a patch of the map's ground cells that use the texture, each with its surface's UV.

---

## Milestone A — the core (Linux tests)

### Task 1: JSON

**Files:**
- Create: `src/core/json.hpp`, `src/core/json.cpp`, `tests/test_json.cpp`
- Setup (once, before the test): `git worktree add -b studio /root/ragnarok-graphics-studio main`;
  `$S/gate-studio.sh` = `$S/gate.sh` with the worktree's path and `-DCMAKE_TOOLCHAIN_FILE=cmake/mingw-x86_64.cmake`
  for build-win64.

**Interfaces:**
- Produces:
```cpp
namespace uagfx::json {
class Value {  // null, bool, number (double), string (UTF-8), array, object (members in their order)
public:
    enum Type : uint8_t { Null, Bool, Number, String, Array, Object };
    Value() = default;
    Value(bool b); Value(int n); Value(int64_t n); Value(double n); Value(const char* s); Value(std::string s);
    static Value array();
    static Value object();
    Type type() const;
    bool asBool(bool fallback = false) const;
    double asNumber(double fallback = 0) const;
    int64_t asInt(int64_t fallback = 0) const;
    const std::string& asString() const;                 // "" when not a string
    const std::vector<Value>& items() const;             // empty when not an array
    const std::vector<std::pair<std::string, Value>>& members() const;
    const Value& operator[](const std::string& key) const;  // a null value when missing
    const Value& operator[](size_t i) const;
    bool has(const std::string& key) const;
    Value& set(const std::string& key, Value v);  // replaces a member of that name
    Value& push(Value v);
};
bool parse(const std::string& text, Value& out, std::string& error);  // depth at most 64
std::string dump(const Value& v);  // one line, no spaces; UTF-8 as it is; " \ and controls escaped
}
```

- [ ] **Step 1: Write the failing tests** — `json_round_trips_nested` (an object with an array of objects, numbers
  `-1.5`, `3`, `1e3`, `true`, `null` dumps and parses back equal); `json_keeps_utf8_and_escapes` (`"Ж\n"` parses
  to `"\xD0\x96\n"`; `"😀"` to the 4 UTF-8 bytes `F0 9F 98 80`; CP949 bytes `\xB3\xAA` in a string survive
  a dump and parse unchanged; `dump(Value("a\"\\\t"))` is `"a\"\\\t"` escaped); `json_refuses_broken` (`{"a":}`,
  `[1,]`, `{"a":1} x`, 65 nested arrays: false with an error); `json_missing_member_is_null`.
- [ ] **Step 2:** `cmake --build build-host -j && ./build-host/uagfx_tests json` — FAIL (no core/json.hpp).
- [ ] **Step 3: Implement** a recursive-descent parser over the bytes (strings kept as bytes; `\uXXXX` and surrogate
  pairs to UTF-8); numbers by `strtod` on the C locale's form.
- [ ] **Step 4:** the same command — PASS; `bash $S/gate-studio.sh` — `GATE OK`.
- [ ] **Step 5: Commit** `feat: core/json - JSON для студии (труба, сессия, очередь)`.

### Task 2: The index's new lines and the entries' times

**Files:**
- Modify: `src/core/texture_pack.hpp`, `src/core/texture_pack.cpp`, `src/core/ai/ready.cpp` (skip the new lines)
- Test: `tests/test_texture_pack.cpp`, `tests/test_ai_ready.cpp`

**Interfaces:**
- Produces:
```cpp
namespace uagfx::pack {
struct Provenance {  // '+' lines: what a picture was made from
    std::string file, archive;  // the game's file (CP949) and its archive's name
    int64_t made = 0;           // Unix seconds
    std::string model, chain;   // ai::chainText
};
struct MaterialScales { float gloss = 1, relief = 1, height = 1; };  // '#' lines; 0 turns a part off
struct Index {  // + three members
    std::map<std::string, Provenance> made;            // picture name ->
    std::set<std::string> ownAlways;                   // keys ('!' lines): the game's own always
    std::map<std::string, MaterialScales> materials;   // keys ->
};
struct ZipEntry { /* + */ uint16_t dosDate = 0, dosTime = 0; };
int64_t unixOfDos(uint16_t date, uint16_t time);              // UTC; 0 for 1980-01-01 00:00
void dosOfUnix(int64_t t, uint16_t& date, uint16_t& time);
// Writer::add gains `int64_t when = -1` (-1: now) after `replace`; open(resume) keeps the entries' times.
int64_t Reader::timeOf(const std::string& name) const;        // 0: none, or 1980-01-01
}
```
- Line forms, exactly: `+<picture>\t<file>\t<archive>\t<made>\t<model>\t<chain>`, `!<key>`,
  `#<key>\tgloss=<x>\trelief=<x>\theight=<x>` (`%.2f`, all three always). Written after the alike lines.

- [ ] **Step 1: Write the failing tests** — `index_round_trips_its_new_lines` (made, ownAlways, materials through
  `indexText`/`parseIndexFile`); `index_new_lines_are_no_pictures_to_old_readers` (`parseIndex` of such a text gives
  the real pictures their kinds and nothing else a kind; `ReadyPictures::addPack` with it finds no picture named
  `+...`); `pack_entries_carry_their_time` (`add(..., false, 1760000000)` then `Reader::timeOf` is 1760000000 within 2
  s; an entry from before stays at its time after `open(resume)` and a checkpoint).
- [ ] **Step 2:** `./build-host/uagfx_tests index_ && ./build-host/uagfx_tests pack_entries` — FAIL.
- [ ] **Step 3: Implement** — `kDosDate` gives way to each entry's own date and time (local and central headers);
  `centralDirectoryOf` fills them.
- [ ] **Step 4:** PASS; gate `GATE OK`.
- [ ] **Step 5: Commit** `feat: индекс пакета - строки происхождения (+), «всегда своя игры» (!), материалы (#); у
  картинок пакета настоящее время записи`.

### Task 3: Every archive of a name; directories read once (N3)

**Files:**
- Modify: `src/core/grf/grf.hpp/.cpp` (entries in a `std::shared_ptr`, copied before a write), `src/core/pack_build.hpp/.cpp`
- Test: `tests/test_pack_build.cpp`, `tests/test_grf.cpp`

**Interfaces:**
- Produces:
```cpp
namespace uagfx::grf { bool Archive::openLike(const Archive& other, std::string& error); }  // own file, shared table
namespace uagfx::pack {
class GameFiles {  // +
    bool openLike(const GameFiles& other, std::string& error);  // a thread's reader: tables shared, its own handles
    const std::vector<std::string>& paths() const;               // the archives that opened, in DATA.INI's order
    std::vector<int> archivesOf(const std::string& name) const;  // the giving one first, then those it shadows
};
}
```

- [ ] **Step 1: Write the failing tests** — `game_files_name_every_archive_of_a_file` (two ZIP archives in DATA.INI
  both holding `data\texture\a.bmp`: `archivesOf` is `{0, 1}`, `read` gives the first's bytes); `game_files_open_like
  _shares_and_reads` (a second reader by `openLike` reads the same bytes; the first closed, the second still reads);
  `grf_open_like_reads_without_its_own_table`.
- [ ] **Step 2:** `./build-host/uagfx_tests game_files_ && ./build-host/uagfx_tests grf_open_like` — FAIL.
- [ ] **Step 3: Implement**; the builder's `listMain` threads take `openLike` of the builder's `files_` (one table read).
- [ ] **Step 4:** PASS; gate.
- [ ] **Step 5: Commit** `perf: таблицы архивов игры читаются один раз на все потоки; у файла видны все архивы, где он есть`.

### Task 4: Ground uses, atlas tiles, a patch of the map

**Files:**
- Modify: `src/core/map_textures.hpp/.cpp`
- Create: `src/core/atlas_tiles.hpp`, `src/core/atlas_tiles.cpp`, `tests/test_atlas_tiles.cpp`
- Test: `tests/test_map_textures.cpp`

**Interfaces:**
- Produces:
```cpp
namespace uagfx::map_textures {
struct UvRect { float u0 = 0, v0 = 0, u1 = 0, v1 = 0; };
// A GND (1.7 on): each ground texture's surfaces' UV rectangles - after the names: lightmaps (count, w, h, cells,
// then count * w * h * 4 bytes), surfaces (count, then 40 bytes each: u[4], v[4] floats, i16 texture, u16 lightmap,
// 4 bytes colour). Empty when it is no ground file or cut short.
std::map<std::string, std::vector<UvRect>> groundUses(const uint8_t* data, size_t size);
struct PatchCell { int x = 0, y = 0; UvRect uv; bool rotated = false; };  // a top surface: its cell and its UV
// Up to side x side cells of the map's top surfaces about the place where `texture` is laid most; [] none.
std::vector<PatchCell> groundPatch(const uint8_t* data, size_t size, const std::string& texture, int side = 8);
}
namespace uagfx::atlas_tiles {
struct Grid { int cols = 1, rows = 1; };
// 4x4, else 2x2, when every rectangle lies inside one cell (1/512 slack); else 1x1.
Grid gridOf(const std::vector<map_textures::UvRect>& uses);
// A cell with `pad` texels of itself wrapped about it (the tile repeated, never its neighbours).
std::vector<uint32_t> cutTile(const uint32_t* argb, int w, int h, Grid g, int col, int row, int pad, int& tw, int& th);
// An enlarged tile (pad * scale about it) back into its place of the enlarged texture (W x H).
void placeTile(std::vector<uint32_t>& out, int W, int H, Grid g, int col, int row, const std::vector<uint32_t>& tile,
               int tw, int th, int padScaled);
}
```

- [ ] **Step 1: Write the failing tests** — `ground_uses_of_its_surfaces` (a built GND with 2 textures, 3 surfaces:
  the rectangles per name); `ground_uses_cut_short_is_empty`; `ground_patch_lies_where_the_texture_is`;
  `atlas_grid_four_by_four` (rects inside 1/4 cells → 4x4; one rect across a 1/4 line → 2x2 when inside halves; a
  whole 0..1 rect → 1x1); `atlas_tile_wraps_itself` (a 64x64 texture of 16 distinct cells: `cutTile(..., 4, 4)` of
  cell (1,2) is 24x24 and its pad texels equal the cell's opposite edge, not the neighbour's);
  `atlas_tile_goes_back_in_place` (cut every tile, enlarge nearest x4, place: equals the nearest x4 of the whole).
- [ ] **Step 2:** `./build-host/uagfx_tests ground_ && ./build-host/uagfx_tests atlas_` — FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4:** PASS; gate.
- [ ] **Step 5: Commit** `feat: core/atlas_tiles и разбор поверхностей земли - атлас 4x4/2x2 по UV карт, плитка с
  обёрнутыми краями, кусок карты`.

### Task 5: Picture metrics (T3)

**Files:**
- Create: `src/core/ai/picture_metrics.hpp`, `src/core/ai/picture_metrics.cpp`, `tests/test_picture_metrics.cpp`

**Interfaces:**
- Produces:
```cpp
namespace uagfx::ai {
struct Metrics {
    float detail = 1;   // detail kept: the Laplacian energy of the picture shrunk to the original's size / the original's
    float tone = 0;     // the mean colour's OKLab distance
    float banding = 0;  // the share of smooth-gradient texels in flat runs of 4+ between steps
    float ringing = 0;  // mean overshoot past the original's range beside its edges (0..1)
    float fringe = 0;   // the share of edge texels by the colour key leaning to magenta
    float alpha = 0;    // the share of texels whose shrunk alpha differs from the original's
    float seams = 0;    // the wrap's difference across / inside (world pictures; 0 others)
};
enum Flag : uint16_t { Smoothed = 1, Washed = 2, Banded = 4, Ringing = 8, Fringe = 16, AlphaSpoiled = 32, Seams = 64 };
Metrics measure(const uint32_t* original, int w, int h, const uint32_t* picture, int pw, int ph, bool wraps);
uint16_t flagsOf(const Metrics& m);  // detail < 0.45, tone > 0.04, banding > 0.15, ringing > 0.08, fringe > 0.05,
                                     // alpha > 0.03, seams > 2.0 (starting points, set later on real pictures)
const char* flagName(Flag f);        // "smoothed", "washed", "banded", "ringing", "fringe", "alpha", "seams"
}
```

- [ ] **Step 1: Write the failing tests** (originals: a 32x32 noisy texture, a gradient, a keyed sprite) —
  `metrics_flag_a_blurred_picture` (box-blurred x4: Smoothed; nearest x4: detail ≥ 0.9, not Smoothed);
  `metrics_flag_a_tone_shift` (+30 on R: Washed); `metrics_flag_banding` (the gradient x4 quantised to 6 levels:
  Banded; bilinear x4: not); `metrics_flag_ringing`; `metrics_flag_magenta_fringe` (pink texels beside the key);
  `metrics_flag_spoiled_alpha`; `metrics_flag_seams` (a picture whose left and right edges differ hard);
  `metrics_take_any_scale` (pictures 2x, 3x, 4x and 4x minus one texel: no read past either, nearest x2 detail ≥ 0.9).
- [ ] **Step 2:** `./build-host/uagfx_tests metrics_` — FAIL.
- [ ] **Step 3: Implement** — shrink by area average to w x h; sRGB to OKLab for tone.
- [ ] **Step 4:** PASS; gate.
- [ ] **Step 5: Commit** `feat: core/ai/picture_metrics - флажки качества картинки (замылена, тон, полосы, ореолы,
  кайма ключа, альфа, швы)`.

### Task 6: The processing chain (T5) around the network

**Files:**
- Create: `src/core/ai/chain.hpp`, `src/core/ai/chain.cpp`, `tests/test_chain.cpp`

**Interfaces:**
- Consumes: `atlas_tiles::Grid`, `cutTile`, `placeTile` (Task 4); `ai::halve`, `ai::enlargeAlpha` (texture_cache).
- Produces:
```cpp
namespace uagfx::ai {
struct Chain {
    bool deblock = false, repairKey = false;            // before the network
    std::string model = "general"; int scale = 4; int denoise = -1;  // -1: the settings'
    bool tiles = false;                                 // atlas tiles when the catalog found a grid
    bool matchTone = false; float grain = 0, sharpen = 0; bool cleanAlpha = false;  // after it
    std::string blendModel;                             // "" none: a second model where the original has edges
    float original = 0;                                 // the original (bicubic) mixed in, 0..1
};
// "general x4" then the steps on: "deblock key tiles tone grain=0.30 sharpen=0.40 alpha blend=anime original=0.20
// denoise=50"
std::string chainText(const Chain& c);
bool parseChain(const std::string& text, Chain& out);
void deblock(std::vector<uint32_t>& argb, int w, int h);      // 8x8 block edges smoothed where both sides are flat
void repairKey(std::vector<uint32_t>& argb, int w, int h);    // near-magenta texels (a JPEG's) to the key
void matchTone(std::vector<uint32_t>& pic, int W, int H, const uint32_t* orig, int w, int h);  // OKLab mean, spread
void restoreGrain(std::vector<uint32_t>& pic, int W, int H, const uint32_t* orig, int w, int h, float strength);
void sharpenAdaptive(std::vector<uint32_t>& pic, int W, int H, float amount);  // contrast adaptive
void cleanAlpha(std::vector<uint32_t>& pic, int W, int H, const uint32_t* orig, int w, int h);
void blendByEdges(std::vector<uint32_t>& pic, const std::vector<uint32_t>& other, int W, int H, const uint32_t* orig,
                  int w, int h);
void mixOriginal(std::vector<uint32_t>& pic, int W, int H, const uint32_t* orig, int w, int h, float amount);
using Enlarge = std::function<bool(const std::string& model, int denoise, const std::vector<uint32_t>& argb, int w,
                                   int h, std::vector<uint32_t>& out, std::string& error)>;  // 4 times, the engine's
bool runChain(const Chain& c, const uint32_t* argb, int w, int h, atlas_tiles::Grid grid, const Enlarge& enlarge,
              std::vector<uint32_t>& out, int& W, int& H, std::string& error);
}
```

- [ ] **Step 1: Write the failing tests** — `chain_text_round_trips`; `chain_deblock_only_where_flat`;
  `chain_repair_key_takes_near_magenta`; `chain_match_tone_moves_the_mean`; `chain_restore_grain_adds_detail`
  (`measure(...).detail` rises); `chain_sharpen_raises_edges_not_flats`; `chain_clean_alpha_follows_the_key`;
  `chain_blend_by_edges_takes_the_other_on_edges`; `chain_tiles_ask_the_network_per_tile` (a fake `Enlarge` sees 16
  calls of (w/4 + 2 * 4) a side for a 4x4 grid); `chain_x2_halves_the_network_picture`.
- [ ] **Step 2:** `./build-host/uagfx_tests chain_` — FAIL.
- [ ] **Step 3: Implement** — tile pad 4 texels (16 after x4); `scale 2` halves the x4 picture.
- [ ] **Step 4:** PASS; gate.
- [ ] **Step 5: Commit** `feat: core/ai/chain - цепочка обработки вокруг сети (деблок, ключ, тон, зерно, резкость,
  альфа, смеси, плитки атласа)`.

### Task 7: The catalog (T1)

**Files:**
- Create: `src/core/texture_catalog.hpp`, `src/core/texture_catalog.cpp`, `tests/test_texture_catalog.cpp`

**Interfaces:**
- Consumes: `pack::sourceOf`, `pack::picturesOf`, `pack::palettesOf`, `pack::lowerDetail`, `ai::groupOfPicture`,
  `map_textures::filesOf`, `groundUses` (Task 4), `atlas_tiles::gridOf`, `pack::Index` (Task 2).
- Produces:
```cpp
namespace uagfx::catalog {
using Read = std::function<bool(const std::string& name, std::vector<uint8_t>& out)>;
struct ArchiveStamp { std::string path; uint64_t size = 0; int64_t time = 0; bool operator==(const ArchiveStamp&) const; };
struct PictureOf {
    std::string key;                  // ai::keyName
    uint8_t group = 0, aiGroup = 0;   // pack::Group, ai::Group
    uint16_t width = 0, height = 0;
    std::string palette;              // "" its own; else the palette file (a colour of the server's)
    std::vector<std::string> lowKeys; // the world's at 1/2 and 1/4
};
struct FileEntry {
    std::string name;                 // CP949, lower case, backslashes
    std::vector<uint16_t> archives;   // the giving one first, then the shadowed
    uint8_t type = 0, kind = 0;       // pack::Source
    std::vector<PictureOf> pictures;
    std::vector<uint32_t> maps;       // into Catalog::maps
    atlas_tiles::Grid grid;
};
struct Catalog {
    std::vector<ArchiveStamp> archives;
    std::vector<std::string> maps;
    std::vector<FileEntry> files;
    int maxHair = 8, maxCloth = 77;
};
// The files of the listing with a part in the pack; an old catalog's entry is kept when its giving archive's stamp
// and its archives are the same. toDecode: the files left to decode.
Catalog start(const std::vector<ArchiveStamp>& archives,
              const std::vector<std::pair<std::string, std::vector<uint16_t>>>& names, const pack::Choice& choice,
              const Catalog* old, std::vector<size_t>& toDecode);
bool decode(FileEntry& f, const std::vector<std::string>& allNames, const Read& read, const pack::Choice& choice,
            const std::atomic<bool>* stop);  // its pictures; false stopped
void mapsOf(Catalog& c, const Read& read);   // every data\*.rsw: the maps a file is on, the ground's grids
std::unordered_multimap<std::string, std::pair<size_t, size_t>> byKey(const Catalog& c);  // and lower keys
struct PackView {
    std::unordered_map<std::string, int64_t> pictures;  // name -> time
    pack::Index index;
};
enum class State : uint8_t { Missing, InPack, OwnAlways };
State stateOf(const PictureOf& p, const PackView& v);  // a lower key's alias counts as in the pack
std::vector<std::string> orphans(const Catalog& c, const PackView& v);  // pictures no key of the catalog names
std::string save(const Catalog& c);                    // "UCAT", version 1, then the fields
bool load(const std::string& bytes, Catalog& out);     // false: another version, cut short, broken
}
```

- [ ] **Step 1: Write the failing tests** (in memory: two archives, a world BMP in both, a TGA, a head sprite with
  hair palettes 0-12, a map `t.rsw` + `t.gnd` naming the BMP with 4x4 UVs) — `catalog_lists_every_texture`
  (giving and shadowed archives; keys equal `pack::picturesOf`'s); `catalog_takes_the_server_colours` (hair
  palettes 0-8 only); `catalog_names_the_maps_and_grid`; `catalog_keeps_what_did_not_change` (an old catalog: only
  the files of the changed archive in toDecode); `catalog_joins_the_pack` (Missing, InPack, OwnAlways, an alias);
  `catalog_finds_orphans`; `catalog_cache_round_trips`; `catalog_cache_refuses_bad_bytes` (another version, every cut
  length of a small save, flipped bytes in a count: false, no crash, no huge allocation).
- [ ] **Step 2:** `./build-host/uagfx_tests catalog_` — FAIL.
- [ ] **Step 3: Implement** — `load` checks every count against the bytes left before it allocates.
- [ ] **Step 4:** PASS; gate.
- [ ] **Step 5: Commit** `feat: core/texture_catalog - каталог текстур игры (архивы и перекрытые, ключи, цвета сервера,
  карты, сетка атласа, состояние в пакете, сироты, кэш)`.

### Task 8: The pipe's lines, the link's files, raw pictures

**Files:**
- Create: `src/core/studio_link.hpp`, `src/core/studio_link.cpp`, `tests/test_studio_link.cpp`

**Interfaces:**
- Consumes: `json::Value`, `parse`, `dump` (Task 1).
- Produces:
```cpp
namespace uagfx::studio {
struct Message {          // one JSON object a line; requests: hello, enlarge, material, build, stop, quit;
    int64_t id = 0;       // replies (same id): progress, result, error; events (id 0): log, engine
    std::string op;
    json::Value body;
};
std::string lineOf(const Message& m);  // ends in '\n'
class LineReader {
public:
    void feed(const char* p, size_t n);
    bool next(Message& out);  // a whole line; a line that is no message is skipped (and counted)
    size_t skipped() const;
};
constexpr const char* kPickEvent = "Local\\uaro-studio-pick";
constexpr const char* kChangedEvent = "Local\\uaro-studio-changed";
constexpr const char* kFolder = "uaroGraphics\\studio";  // under %LOCALAPPDATA%
struct Pick { std::string key, kind, how, map, picture; int x = 0, y = 0; };
std::string pickText(const Pick& p);
bool parsePick(const std::string& text, Pick& out);
std::string changedText(const std::vector<std::string>& keys);
std::vector<std::string> parseChanged(const std::string& text);
enum class How : uint8_t { Pack, Made, Missing, Soft, Alpha, Size, KindOff, Marked, Budget, Hidden };
const char* howName(How h);  // pack, made, missing, soft, alpha, size, kind-off, marked, budget, hidden
bool parseHow(const std::string& word, How& out);
struct Seen { std::string key; uint8_t kind = 0; How how = How::Missing; uint32_t count = 0; };
std::string seenText(const std::string& map, const std::vector<Seen>& seen);  // "map=..." then key\tkind\thow\tcount
bool parseSeen(const std::string& text, std::string& map, std::vector<Seen>& out);
std::vector<std::string> seenToDelete(std::vector<std::pair<std::string, int64_t>> files, size_t keep = 20);
std::string rawOf(int w, int h, const std::vector<uint32_t>& argb);  // "URAW", w, h, ARGB: the engine's in and out
bool parseRaw(const std::string& bytes, int& w, int& h, std::vector<uint32_t>& argb);  // sizes checked
}
```

- [ ] **Step 1: Write the failing tests** — `link_lines_split_and_join` (a message fed in three pieces, two lines in
  one piece, a broken line skipped); `pick_and_seen_keep_cp949_names` (map `\xC7\xC1` and a CP949 picture name round
  trip); `seen_keeps_the_last_twenty`; `changed_round_trips`; `raw_round_trips_and_refuses_lies` (a header claiming
  more pixels than the bytes: false).
- [ ] **Step 2:** `./build-host/uagfx_tests link_ ; ./build-host/uagfx_tests seen_ ; ...` — FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4:** PASS; gate.
- [ ] **Step 5: Commit** `feat: core/studio_link - строки трубы студии, файлы связи с игрой (pick, changed, seen),
  сырые картинки движка`.

### Task 9: Session, queue, builds

**Files:**
- Create: `src/core/studio_state.hpp`, `src/core/studio_state.cpp`, `tests/test_studio_state.cpp`

**Interfaces:**
- Produces:
```cpp
namespace uagfx::studio {
struct Session {
    std::string tab = "textures", nav, filter, sort;
    bool sortDown = false, paused = false;
    std::vector<std::string> selected;  // keys
    std::string shown;                  // the key in the inspector
    int zoom = 1;
    std::string view = "pair";          // pair, wipe, tiled, map, difference
    int x = -1, y = -1, w = 0, h = 0;   // the window
};
json::Value toJson(const Session& s);
Session sessionOf(const json::Value& v);  // missing members keep their defaults
struct Job {
    int64_t id = 0;
    std::string op;                     // make, keep, discard, takeout, own, material, compact, build, catalog
    json::Value args;
    enum State : uint8_t { Queued, Running, Done, Failed } state = Queued;
    int tries = 0;
    bool writesPack = false;
    std::string why;
};
class Queue {
public:
    int64_t add(const std::string& op, json::Value args, bool writesPack);
    Job* next(bool packFree);           // the first queued one it may run; a pack writer waits while busy
    void started(int64_t id);
    void done(int64_t id);
    void failed(int64_t id, const std::string& why);
    void engineLost(int64_t id);        // queued again once, then failed ("the engine stopped twice")
    const std::vector<Job>& jobs() const;
    json::Value toJson() const;         // a running job is written as queued
    static Queue fromJson(const json::Value& v);
};
std::string engineName(const std::string& versionLine, uint64_t size, int64_t time);  // engine-2.2.5-<8 hex>.exe
std::vector<std::string> buildsToDrop(const std::vector<std::pair<std::string, int64_t>>& builds,
                                      const std::string& running, size_t keep = 5);  // never the running one
}
```

- [ ] **Step 1: Write the failing tests** — `session_round_trips_and_defaults`; `queue_runs_in_order`;
  `queue_waits_while_the_pack_is_busy` (a pack writer first and a make second: `next(false)` gives the make,
  `next(true)` the writer); `queue_retries_once_after_the_engine_is_lost`; `queue_saves_running_as_queued`;
  `engine_name_follows_the_build`; `builds_drop_all_but_five_and_the_running`.
- [ ] **Step 2:** `./build-host/uagfx_tests session_ ; ./build-host/uagfx_tests queue_ ; ...` — FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4:** PASS; gate.
- [ ] **Step 5: Commit** `feat: core/studio_state - сессия, очередь задач (пакет занят, повтор после падения движка),
  сборки студии и движки`.

## Milestone B — the engine

### Task 10: `uaro-render64.exe --studio`

**Files:**
- Create: `src/host/studio_engine.hpp`, `src/host/studio_engine.cpp`
- Modify: `src/host/main.cpp` (`--studio --dir "<game>" --pipe <name>`), `CMakeLists.txt`,
  `src/win/gpu/renderer.hpp`, `src/win/gpu/device.cpp`, `src/win/gpu/ai_textures.hpp/.cpp`

**Interfaces:**
- Consumes: `studio::LineReader`, `lineOf`, `parseRaw`, `rawOf` (Task 8).
- Produces:
```cpp
namespace uagfx::host { int runStudio(const std::string& gameDir, const std::string& pipe); }
namespace uagfx::gpu {
// Renderer: the network of a model ("general", "anime", "anime6b"; denoise for general), loaded when first asked,
// its file closed after; 4 times the size, border as AiTextures::enlarge's.
bool Renderer::enlargeByModel(const std::string& model, int denoise, const uint32_t* argb, int w, int h,
                              uint32_t border, std::vector<uint32_t>& out, std::string& error);
}
```
- Requests and replies (the body's fields): `hello` → `{version, adapter, models:[...]}`; `enlarge {in, out, model,
  denoise, border}` → `{width, height, ms}`; `stop {of}`; `quit`. Paths as UTF-8, opened by `_wfopen`. An `error`
  reply carries `{why}`. The engine connects to `\\.\pipe\<name>` (the studio is the server), ends when the pipe
  breaks.

- [ ] **Step 1:** build check first — `cmake --build build-win64 -j` with `runStudio` declared and called from
  main: FAIL to link (not defined).
- [ ] **Step 2: Implement** — the networks start (`create`, `startAiForPack` with no group's model) only at the first
  `enlarge`; the shader cache `<game>\uaro-graphics\studio-shaders-64.bin`; requests handled one at a time on the
  main thread, a reader thread feeding a queue (`stop` seen between pictures).
- [ ] **Step 3:** gate `GATE OK`; a smoke test on Linux is not possible - note in the ledger; the user's check comes
  with Task 13.
- [ ] **Step 4: Commit** `feat: движок студии - uaro-render64.exe --studio: сеть любой модели по трубе (hello,
  enlarge, stop, quit)`.

### Task 11: Builds through the engine, with stages (N1, N2)

**Files:**
- Modify: `src/core/pack_build.hpp/.cpp` (Status), `src/host/pack_builder.hpp/.cpp`, `src/host/studio_engine.cpp`
- Test: `tests/test_pack_build.cpp`

**Interfaces:**
- Consumes: `catalog::load/save/start/decode` (Task 7), `GameFiles::openLike` (Task 3), `pack::Provenance` (Task 2).
- Produces:
```cpp
namespace uagfx::pack {
struct Status {  // +
    std::string stage;                 // archives, textures, colours, making, writing ("" older)
    std::vector<std::string> threads;  // each reading thread's file now ("thread=" lines)
};
}
namespace uagfx::host {
struct BuildSink {  // where a build tells how it goes: the status file, or the studio's pipe
    virtual ~BuildSink() = default;
    virtual void status(const pack::Status& s, bool now) = 0;
    virtual bool stopAsked() = 0;
};
struct PackArgs { /* + */ std::string catalog; };  // --catalog <file>: its reading pass is the catalog's update
int runPack(const PackArgs& a, BuildSink* sink = nullptr);  // null: the status file
}
```
- The builder writes a `+` line for every picture it makes (file, its giving archive, now, model,
  `model + " x4"`), leaves out the keys of `!` lines, and logs the timeline at the end: `texture pack: time - archives
  3 s, textures 41 s, colours 12 s, making 812 s, writing 4 s`. The engine's `build {kinds, ai, quality, fresh, map,
  mapKinds, remake, colours, catalog}` runs it with a pipe sink: `progress` replies carry the Status as JSON.

- [ ] **Step 1: Write the failing test** — `pack_status_names_its_stage_and_threads` (round trip of `stage=` and
  repeated `thread=` lines; a 2.2.5 status without them parses).
- [ ] **Step 2:** `./build-host/uagfx_tests pack_status` — FAIL.
- [ ] **Step 3: Implement** the Status lines, then the sink, the catalog path, the provenance, the timeline.
- [ ] **Step 4:** PASS; gate.
- [ ] **Step 5: Commit** `feat: сборка пакета называет этапы и файлы каждого потока, пишет откуда картинка, берёт
  каталог вместо чтения игры; движок студии запускает сборку`.

## Milestone C — the studio

### Task 12: The program: its own copy, one at a time, texts

**Files:**
- Create: `src/studio/main.cpp`, `src/studio/places.hpp`, `src/studio/places.cpp`, `src/studio/studio.rc`
- Modify: `CMakeLists.txt` (64-bit: `add_executable(uaro-studio WIN32 ...)` with `src/app/ui.cpp`,
  `src/app/text.cpp`, `src/app/utf16.cpp`, linked static, `-municode`), `tools/package.sh` (into `uaro-graphics\`),
  `tools/check_package.py`, `src/core/settings/strings.hpp/.cpp` (Studio texts)

**Interfaces:**
- Consumes: `buildsToDrop`, `engineName` (Task 9), `kFolder` (Task 8).
- Produces:
```cpp
namespace uagfx::studio {
std::wstring studioRoot();                 // %LOCALAPPDATA%\uaroGraphics\studio
std::wstring buildFolder();                // studioRoot()\<version>-<8 hex of the exe's size and time>
std::wstring linkFolder();                 // studioRoot()\link
bool runningFromCopy();                    // this exe is in its build folder
bool startCopy(const std::wstring& gameDir);  // copies itself there, starts it with --game, true: this one ends
std::wstring gameDirOf(const std::wstring& exePath);  // the folder above uaro-graphics\ (or --game)
}
```
- A second studio brings the first to the front (mutex `Local\uaro-studio`, the window found by its class).

- [ ] **Step 1:** gate with the target and an empty window: `GATE OK`, `build-win64/uaro-studio.exe` exists.
- [ ] **Step 2:** the texts (window title, tabs, bar, states, flags) in three languages; `check_texts.py` passes.
- [ ] **Step 3: Commit** `feat: uaro-studio.exe - отдельная программа: работает из своей копии в LOCALAPPDATA,
  одна за раз, в пакете обновления`.

### Task 13: The engine link, the queue, Pause

**Files:**
- Create: `src/studio/engine.hpp/.cpp` (the copy, the pipe server, a reader thread posting `WM_APP` messages, the
  restart), `src/studio/jobs.hpp/.cpp` (the Queue run: a job at a time per kind, the pack's mutexes probed), 
  `src/studio/pause.cpp`

**Interfaces:**
- Consumes: `Queue`, `Job` (Task 9), `Message`, `LineReader` (Task 8), `engineName` (Task 9).
- Produces:
```cpp
namespace uagfx::studio {
class Engine {
public:
    bool start(const std::wstring& gameDir, std::wstring& error);  // the newest engine copied and started
    void stop();                                                    // quit, then ended after 3 s
    int64_t send(const std::string& op, json::Value body);          // its id
    bool newerInstalled() const;   // version.txt or uaro-render64.exe changed (looked at every 2 s)
    std::string version() const;
};
// WM_APP + 1: a Message* from the engine (the window frees it); WM_APP + 2: the engine is gone
}
```
- The engine copies of the last five driver versions stay (F3 later), older ones go (`buildsToDrop`).
- Pause: the engine stopped, the catalog's threads stopped, the pack and the archives closed, `queue.json` and
  `session.json` written; Resume: the newest engine, the catalog brought up to date (Task 14), the queue goes on.

- [ ] **Step 1:** gate `GATE OK`.
- [ ] **Step 2: Manual check list** into `$S/notes/studio-checks.md`: start, Pause, update the driver while paused,
  Resume (the new engine's version on the bar), the engine killed in Task Manager mid-job (tried once more).
- [ ] **Step 3: Commit** `feat: студия - движок по трубе (копия установленного, перезапуск, смена после задачи),
  очередь, пауза и продолжение`.

### Task 14: The catalog in the studio

**Files:**
- Create: `src/studio/catalog_work.hpp/.cpp`

**Interfaces:**
- Consumes: `catalog::*` (Task 7), `GameFiles::open/openLike/archivesOf/paths` (Task 3).
- Produces:
```cpp
namespace uagfx::studio {
class CatalogWork {
public:
    void update(HWND notify, const std::wstring& gameDir);  // threads: listing, decoding, maps; studio\catalog.bin
    void stop();
    const catalog::Catalog* catalog() const;   // after WM_APP + 3 (done)
    catalog::PackView packView() const;        // the pack's names, index and times, read again after each write
    std::wstring stage() const;                // for the bar: archives, textures, colours, maps
};
}
```

- [ ] **Step 1:** gate.
- [ ] **Step 2: Commit** `feat: студия - каталог текстур в потоках, кэш catalog.bin, обновление по изменившимся
  архивам`.

### Task 15: The window, the navigator, the list

**Files:**
- Create: `src/studio/window.hpp/.cpp` (tabs Textures, Pack, Log; the bar), `src/studio/textures_tab.hpp/.cpp`
  (a tree view; a virtual list view with LVS_OWNERDATA), `src/studio/thumbs.hpp/.cpp` (64-pixel pairs made on a
  thread: the game's pixels and the pack's WebP), `src/core/studio_view.hpp/.cpp`, `tests/test_studio_view.cpp`

**Interfaces:**
- Produces (core, tested):
```cpp
namespace uagfx::studio {
struct View { float zoom = 1; float x = 0, y = 0; };  // the original's texel at the pane's top left
View zoomAt(View v, float factor, float cursorX, float cursorY);  // the texel under the cursor stays there
void sourceTexel(const View& v, int paneX, int paneY, int w, int h, int pw, int ph, int& sx, int& sy, bool& inside);
std::vector<uint32_t> differenceImage(const uint32_t* a, int aw, int ah, const uint32_t* b, int bw, int bh,
                                      int w, int h);  // both at w x h, |a-b| times 4
struct RowFilter { std::string nav, text; };
bool rowShown(const catalog::FileEntry& f, const catalog::PictureOf& p, catalog::State s, uint16_t flags,
              const RowFilter& r);
}
```
- Navigator: Maps; Folders; States (missing, orphans, by model, flagged, the game's own, with overrides); the game's
  reports by map (`uaro-graphics\seen\*.txt`). List columns: the pair, file, state, model, flags, detail kept, size,
  date.

- [ ] **Step 1: Write the failing tests** — `view_zoom_keeps_the_texel_under_the_cursor`;
  `view_maps_any_picture_size` (a pane texel maps into a picture of 2x, 4x and 3x+1 without passing its edge);
  `view_difference_marks_changes`; `rows_filter_by_navigator_and_text`.
- [ ] **Step 2:** `./build-host/uagfx_tests view_ ; ./build-host/uagfx_tests rows_` — FAIL.
- [ ] **Step 3: Implement** the core part, then the window.
- [ ] **Step 4:** PASS; gate.
- [ ] **Step 5: Commit** `feat: студия - окно (вкладки, полоса задач), навигатор (карты, папки, состояния, отчёты
  игры), список с миниатюрами`.

### Task 16: The inspector (T2, T3)

**Files:**
- Create: `src/studio/compare_view.hpp/.cpp` (a control: pair, wipe, tiled 3x3, as the map lays it, difference;
  zoom 1-16 by the wheel, pan by dragging, nearest or smooth; up to three pictures: the game's, the pack's, the new),
  `src/studio/inspector.hpp/.cpp` (the information, the flags with their numbers, the actions)

**Interfaces:**
- Consumes: `View`, `zoomAt`, `sourceTexel`, `differenceImage` (Task 15); `measure`, `flagsOf` (Task 5);
  `groundPatch` (Task 4); `PackView` (Task 7).
- Produces: `void Inspector::show(const catalog::FileEntry& f, size_t picture);` and the metrics of every row
  computed on the thumbnail thread (stored by key for the list's flags column).

- [ ] **Step 1:** gate.
- [ ] **Step 2: Commit** `feat: студия - инспектор: сравнение (рядом, шторка, плитка 3x3, как на карте, разница,
  масштаб колесом), сведения (архив и перекрытые, ключи, модель, дата, откуда, карты, атлас), флажки`.

### Task 17: Make again, Keep or Discard, Take out, the game's own (T4, T5, T6)

**Files:**
- Create: `src/studio/make_dialog.hpp/.cpp` (model, x4 or x2, denoise, the chain's steps, tiles when the grid is
  more than 1x1), `src/studio/making.hpp/.cpp` (a job a texture: its pixels from the game's files, `runChain` whose
  Enlarge is the engine's `enlarge` through `studio\work\`, the result in `studio\made\<key>.raw` with its metrics),
  `src/studio/pack_edit.hpp/.cpp` (Keep: WebP, `Writer::add(replace, now)`, the `+` line; Take out:
  `Writer::remove`; the game's own: a `!` line; each under the two mutexes, then `changed.txt` and the event)

**Interfaces:**
- Consumes: `runChain`, `Chain`, `chainText` (Task 6); `Engine::send` (Task 13); `Queue` (Task 9); `changedText`,
  `kChangedEvent` (Task 8); `Provenance` (Task 2).
- Produces: `bool packEdit(const std::wstring& pack, const std::function<void(pack::Writer&, pack::Index&)>& change,
  std::string& error);` — false with "the pack is busy" when a mutex is held.

- [ ] **Step 1:** gate.
- [ ] **Step 2:** the checks list: one texture made again by anime with tone and grain, Keep, the game shows it
  within a second (after Task 22); Discard leaves the pack untouched (its size and time).
- [ ] **Step 3: Commit** `feat: студия - пересоздать моделью и цепочкой (плитки атласа), посмотреть, оставить или
  отбросить; убрать из пакета; всегда своя игры`.

### Task 18: The Pack tab (P1 in part) and the Log tab

**Files:**
- Create: `src/studio/pack_tab.hpp/.cpp`, `src/studio/log_tab.hpp/.cpp`; `src/core/pack_overview.hpp/.cpp`,
  `tests/test_pack_overview.cpp`

**Interfaces:**
- Produces (core):
```cpp
namespace uagfx::pack {
struct Overview {
    std::map<std::string, uint32_t> byGroup, byModel, byKind;
    uint32_t pictures = 0, orphans = 0, missing = 0;
    uint64_t bytes = 0, unread = 0;  // the file's size; what no entry points at (replaced, taken out)
};
Overview overviewOf(const std::vector<ZipEntry>& entries, uint64_t fileSize, const Index& index,
                    uint32_t orphans, uint32_t missing);
}
```
- Compact: `pack::rewrite` into `uaro-textures.zip.new`, then a move over the pack, as a job (the pack's mutexes).

- [ ] **Step 1: Write the failing test** — `pack_overview_counts_and_unread_bytes`.
- [ ] **Step 2:** `./build-host/uagfx_tests pack_overview` — FAIL.
- [ ] **Step 3: Implement**, then the tabs.
- [ ] **Step 4:** PASS; gate.
- [ ] **Step 5: Commit** `feat: студия - вкладка пакета (по группам, моделям и видам, сироты, недостающие, размер и
  неиспользуемое место, «Сжать») и журнал`.

## Milestone D — the game linked

### Task 19: The game's own marks and what was drawn (L3)

**Files:**
- Modify: `src/core/ai/ready.hpp/.cpp` (marked keys), `src/win/gpu/ai_textures.hpp/.cpp`, `src/win/gpu/ai11.cpp`
- Test: `tests/test_ai_ready.cpp`

**Interfaces:**
- Consumes: `Index::ownAlways` (Task 2); `Seen`, `How`, `seenText`, `seenToDelete` (Task 8).
- Produces: `bool ReadyPictures::marked(const std::string& key) const;` (texture, cell and icon give no picture for
  it); AiTextures and Ai11 count each drawn texture's How a map and write `uaro-graphics\seen\<map>.txt` on the map's
  change and at the end, the last 20 kept.

- [ ] **Step 1: Write the failing test** — `ready_pictures_leave_marked_keys`.
- [ ] **Step 2:** `./build-host/uagfx_tests ready_pictures_leave` — FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4:** PASS; gate.
- [ ] **Step 5: Commit** `feat: драйвер - отчёт о нарисованном на карте (seen\<карта>.txt: ключ, вид, как показана,
  сколько раз) и «всегда своя игры» из пакета`.

### Task 20: X-ray (L1)

**Files:**
- Modify: `src/core/settings/schema.cpp` (`[debug] xray`, 0), `src/core/config.hpp/.cpp`, `src/win/context.cpp`
  (Ctrl+Shift+F9, written as F11 is), `src/core/bridge_protocol.hpp` (`Msg::SetStudio`: u8 flags, i32 x, y),
  `src/win/gpu/remote.cpp`, `src/win/bridge/*`, `src/win/gpu/renderer.hpp`, `src/win/gpu/draw.cpp`,
  `src/win/gpu/shaders.cpp`
- Test: `tests/test_config.cpp`, `tests/test_bridge.cpp`

**Interfaces:**
- Produces: `void Renderer::setStudio(uint8_t flags, int x, int y);` (bit 0 X-ray, bit 1 pick at x, y, bit 2 A/B
  toggle); a draw's colour mixed at 35 %: the pack's green (0.25, 0.85, 0.35), made in the game blue (0.25, 0.55,
  1.0), missing red (1.0, 0.25, 0.25), the game's own by a rule grey (0.6, 0.6, 0.6).

- [ ] **Step 1: Write the failing tests** — `config_reads_xray_off_by_default`; `bridge_carries_set_studio`.
- [ ] **Step 2:** `./build-host/uagfx_tests xray ; ./build-host/uagfx_tests set_studio` — FAIL.
- [ ] **Step 3: Implement**; the shader mix FXC-safe (`allshaders.sh`).
- [ ] **Step 4:** PASS; gate.
- [ ] **Step 5: Commit** `feat: драйвер - рентген (Ctrl+Shift+F9, консоль): текстуры окрашены по состоянию - из пакета,
  сделана в игре, нет картинки, своя игры`.

### Task 21: Under the cursor and A/B (L2)

**Files:**
- Create: `src/core/pick.hpp/.cpp`, `tests/test_pick.cpp`
- Modify: `src/win/context.cpp` (Ctrl+Shift+F8, Ctrl+Shift+F7), `src/win/gpu/device.cpp`/`draw.cpp` (the frame's
  draws searched at a pick), `src/win/gpu/ai_textures.hpp/.cpp` (hidden keys), `src/studio/*` (a thread waiting on
  `kPickEvent` opens the texture and raises the window)

**Interfaces:**
- Produces (core):
```cpp
namespace uagfx::pick {
// The last of a frame's draws (TL vertices: x, y at the start of each vertex, `stride` bytes apart; triangle lists)
// whose triangle holds the point; -1 none.
int lastDrawAt(const std::vector<std::pair<const uint8_t*, uint32_t>>& draws, uint32_t stride, float x, float y);
bool inTriangle(float ax, float ay, float bx, float by, float cx, float cy, float x, float y);
}
```
- The pick writes `<studio>\link\pick.txt` (key, kind, how, map, picture, x, y) and sets `kPickEvent`; Ctrl+Shift+F7
  hides (and shows again) the last picked texture's picture: its draws take the game's texture.

- [ ] **Step 1: Write the failing tests** — `pick_finds_the_last_draw_under_the_point`; `pick_edges_count_once`.
- [ ] **Step 2:** `./build-host/uagfx_tests pick_` — FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4:** PASS; gate.
- [ ] **Step 5: Commit** `feat: текстура под курсором (Ctrl+Shift+F8 открывает её в студии) и A/B одной текстуры в
  игре (Ctrl+Shift+F7)`.

### Task 22: Live (L4)

**Files:**
- Modify: `src/win/gpu/ai_textures.hpp/.cpp`, `src/win/gpu/ai11.cpp`, `src/win/gpu/device.cpp` (the event a frame)

**Interfaces:**
- Consumes: `kChangedEvent`, `parseChanged` (Task 8).
- Produces: `void AiTextures::reloadPack(const std::vector<std::string>& keys);` — the I/O thread opens the pack
  again (its directory, its index: kinds, aliases, the game's own, materials), the keys' copies and materials
  dropped; drawn again they come from the pack.

- [ ] **Step 1:** gate.
- [ ] **Step 2: Commit** `feat: драйвер - изменения студии видны в игре сразу: пакет открывается заново, изменённые
  текстуры и их материалы берутся снова`.

### Task 23: Materials by texture (T7)

**Files:**
- Modify: `src/win/gpu/shaders.cpp` (`material_ps` takes three scales), `src/win/gpu/materials.cpp`,
  `src/win/gpu/materials11.cpp`, `src/win/gpu/ai_textures.hpp/.cpp` (scales by key), `src/win/gpu/renderer.hpp`,
  `src/win/gpu/device.cpp`, `src/host/studio_engine.cpp` (`material {in, out, gloss, relief, height}`),
  create `src/studio/material_panel.hpp/.cpp`

**Interfaces:**
- Consumes: `Index::materials`, `MaterialScales` (Task 2).
- Produces: `bool Renderer::materialOf(const uint32_t* argb, int w, int h, const pack::MaterialScales& s,
  std::vector<uint32_t>& out, std::string& error);` — the material pass on a picture, read back (relief, height,
  gloss as R, G, B... as material_ps writes them); the panel shows the three maps and sliders 0-200 %, Save writes a
  `#` line through `packEdit`.

- [ ] **Step 1:** gate (`allshaders.sh` checks `material_ps`).
- [ ] **Step 2: Commit** `feat: материалы по текстуре - рельеф, высота и блеск, которые драйвер строит из картинки,
  ослабить или выключить для одной текстуры (в пакете, драйвер слушается); их карты в студии`.

## Milestone E — finish

### Task 24: Package, documents, checks

**Files:**
- Modify: `tools/package.sh`, `tools/check_package.py`, `README.md`/`README.*.md` (the studio in short),
  `docs/ARCHITECTURE.md` (the studio, the engine, the link), `dist/TEST.en.md`, `dist/TEST.uk.md`, `dist/TEST.ru.md`
  (how to test the studio), the spec's Status line

- [ ] **Step 1:** `bash tools/package.sh` puts `uaro-graphics/uaro-studio.exe` in the zip; `check_package.py` passes.
- [ ] **Step 2:** gate `GATE OK`.
- [ ] **Step 3: Commit** `docs: студия в пакете обновления, README, ARCHITECTURE, как проверять`.
