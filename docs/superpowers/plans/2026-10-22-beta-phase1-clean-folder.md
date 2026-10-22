# Beta Phase 1: Clean Game Folder Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> The user works inline («все в инлайн режиме», «не надо кучу агентов одновременно»): executing-plans, no parallel
> agents; the final review is a self-review.

**Goal:** Every file of uaRO Graphics lives in `uaro-graphics\` of the game's folder; a 2.x install moves there by
itself at the first start of the driver or the settings app; logs are capped and the previous game's log is kept.

**Architecture:** One table in `src/core/layout` says where each file of ours lives now and where 2.x kept it;
`src/core/layout_migration` moves a 2.x install over an abstract `Disk` (Windows' A-calls in the driver, the app's
`files.hpp` in the settings app, memory in the tests). Every component asks the table for a path (`locate`: the new
place, or the old one when a move could not happen). The log writer (`src/win/log.cpp`, driver and 64-bit host)
keeps the previous game's log and starts a new part at 32 MiB.

**Tech Stack:** C++17, MinGW-w64 i686/x86_64, Win32, the project's runner `tests/test.hpp` (`TEST`, `CHECK`,
`CHECK_EQ`), bash + Python 3 for the package.

**Spec:** `docs/superpowers/specs/2026-10-22-beta-release-design.md` (section «Phase 1»), the page
https://claude.ai/artifact/QjTrLqpnyyw8qKiUZQ3i24.

## Global Constraints

- Work on main in /root/ragnarok-graphics; no push, no version bump: `bash $S/ship.sh 2.8.0 "Выпуск 2.8.0"` only on
  the user's word ($S = /tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad).
- Gate after every task: `bash $S/gate.sh` → `GATE OK` (32/64-bit builds without warnings, 79 shaders with DXC
  -HV 2018, host tests, added lines ≤ 120, Studio, pure-virtual check).
- `ddraw.dll` imports only kernel32, user32, gdi32, msvcrt (`tools/check_dll.py`).
- Comments in English, in the code's own voice; commits by KWEB Team ending with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Root after this phase, of ours: `ddraw.dll`, `uaro-graphics.exe`, the pad's `dinput.dll`; still there until later
  phases: the `.bat` files (phase 2), the pad's `uaro-pad.ini`, `uaro-pad.log`, `uaro-pad.moved` (the pad's own
  release, needs the user's leave). `uaro-flags.grf` is the client's file now: never moved or removed.
- Nothing of the player's is deleted. Removed: only our own root `TEST.*.md` (they start with `# uaRO Graphics`). The
  texture pack is never deleted, also when two exist.
- The package never contains `uaro-graphics/uaro-graphics.ini`: an older update.bat copies `uaro-graphics\` of the
  archive over the installed one, and that path now holds the player's settings.
- The names of the files a report uploads stay the 2.x ones (`uaro-graphics.log`, `uaro-render64.log`, ...): the
  site's admin and the report tools read them.

## Review Focus

1. A game under Program Files or a pack held open (uaro-render64 making a pack, Studio): the moves fail and every
   program must go on using the old places - Task 2 `migration_leaves_what_it_cannot_move_and_locate_keeps_it`,
   Task 6 `model_a_pack_that_could_not_move_is_used_where_it_is`.
2. The game and the settings app starting together, both moving: nothing lost, no false «could not move» - Task 2
   `migration_running_at_once_in_two_programs_loses_nothing`.
3. The update from 2.7.58 by its own update.bat: the player's root ini must win over the 2.x template already at
   `uaro-graphics\uaro-graphics.ini` - Task 2 `migration_moves_a_2x_install_into_uaro_graphics` (the template is
   replaced), Task 7 `check_package.py` (no template at that path in the archive).
4. A fresh install with no 2.x files: install.bat must not write a root `uaro-graphics.ini` fragment, and the template
   lands in `uaro-graphics\` - Task 7 `check_package.py` (the guard before the first `>>"uaro-graphics.ini"`).
5. Restoring a backup made by 2.x (its `files.txt` names the root ini): the settings go to the new ini - Task 6
   `model_restoring_a_2x_backup_writes_the_ini_in_uaro_graphics`.

---

### Task 1: One table of where our files live

**Files:**
- Create: `src/core/layout.hpp`, `src/core/layout.cpp`
- Test: `tests/test_layout.cpp`

**Interfaces:**
- Produces (namespace `uagfx::layout`):
  - `enum class File : uint8_t { Ini, DriverLog, Render64Log, PackLog, StudioEngineLog, Report, Pipelines,
    Shaders32, Shaders64, ShadersD3d11, StudioShaders64, Pack, Backups, DgVoodoo };` and
    `constexpr int kFileCount = 14;`
  - `constexpr const char* kLogs = "uaro-graphics\\logs";`, `kCache = "uaro-graphics\\cache"`,
    `kTextures = "uaro-graphics\\textures"`, `kOurs = "uaro-graphics"`
  - `const char* now(File f);` `const char* before(File f);` `bool isFolder(File f);`
  - `std::string notes(const std::string& langToken);` → `uaro-graphics\notes\TEST.<lang>.md`
  - `std::string join(const std::string& gameDir, const std::string& relative);` → `gameDir + "\\" + relative`
  - `std::string locate(const std::string& gameDir, File f, const std::function<bool(const std::string&)>& exists);`
    now's path when it exists or before's does not; else before's
  - `std::string relativeNow(const std::string& relative);` a 2.x relative path (any case) → now's; others unchanged

| File | before (2.x) | now (2.8) |
|---|---|---|
| Ini | `uaro-graphics.ini` | `uaro-graphics\uaro-graphics.ini` |
| DriverLog | `uaro-graphics.log` | `uaro-graphics\logs\uaro-graphics.log` |
| Render64Log | `uaro-render64.log` | `uaro-graphics\logs\uaro-render64.log` |
| PackLog | `uaro-pack.log` | `uaro-graphics\logs\uaro-pack.log` |
| StudioEngineLog | `uaro-studio-engine.log` | `uaro-graphics\logs\uaro-studio-engine.log` |
| Report | `uaro-graphics-report.txt` | `uaro-graphics\logs\uaro-graphics-report.txt` |
| Pipelines | `uaro-graphics-pipelines.txt` | `uaro-graphics\cache\pipelines.txt` |
| the four shader caches | `uaro-graphics\shaders-*.bin` | `uaro-graphics\cache\shaders-*.bin` |
| Pack | `uaro-textures.zip` | `uaro-graphics\textures\uaro-textures.zip` |
| Backups (folder) | `uaro-graphics-backup` | `uaro-graphics\backup` |
| DgVoodoo (folder) | `dgvoodoo` | `uaro-graphics\dgvoodoo` |

- [ ] **Step 1: Write the failing tests** in `tests/test_layout.cpp`

```cpp
TEST(layout_every_file_of_ours_lives_in_uaro_graphics) {
    for (int i = 0; i < layout::kFileCount; ++i) {
        const std::string now = layout::now(layout::File(i));
        CHECK(now.rfind("uaro-graphics\\", 0) == 0);
        CHECK(now != layout::before(layout::File(i)));
    }
}

TEST(layout_places_of_the_files_2x_kept_in_the_root) {
    CHECK_EQ(std::string(layout::before(layout::File::Ini)), std::string("uaro-graphics.ini"));
    CHECK_EQ(std::string(layout::now(layout::File::Ini)), std::string("uaro-graphics\\uaro-graphics.ini"));
    CHECK_EQ(std::string(layout::now(layout::File::DriverLog)), std::string("uaro-graphics\\logs\\uaro-graphics.log"));
    CHECK_EQ(std::string(layout::before(layout::File::Pipelines)), std::string("uaro-graphics-pipelines.txt"));
    CHECK_EQ(std::string(layout::now(layout::File::Pipelines)), std::string("uaro-graphics\\cache\\pipelines.txt"));
    CHECK_EQ(std::string(layout::now(layout::File::Pack)), std::string("uaro-graphics\\textures\\uaro-textures.zip"));
    CHECK_EQ(std::string(layout::before(layout::File::Backups)), std::string("uaro-graphics-backup"));
    CHECK_EQ(std::string(layout::now(layout::File::DgVoodoo)), std::string("uaro-graphics\\dgvoodoo"));
    CHECK(layout::isFolder(layout::File::Backups) && !layout::isFolder(layout::File::Pack));
    CHECK_EQ(layout::notes("uk"), std::string("uaro-graphics\\notes\\TEST.uk.md"));
}

TEST(layout_locate_prefers_the_new_place_and_keeps_an_unmoved_file) {
    std::set<std::string> there;
    const auto exists = [&](const std::string& p) { return there.count(p) > 0; };
    const std::string fresh = "C:\\uaRO\\uaro-graphics\\textures\\uaro-textures.zip";
    CHECK_EQ(layout::locate("C:\\uaRO", layout::File::Pack, exists), fresh);  // a fresh install
    there.insert("C:\\uaRO\\uaro-textures.zip");
    CHECK_EQ(layout::locate("C:\\uaRO", layout::File::Pack, exists), std::string("C:\\uaRO\\uaro-textures.zip"));
    there.insert(fresh);
    CHECK_EQ(layout::locate("C:\\uaRO", layout::File::Pack, exists), fresh);
}

TEST(layout_a_2x_backup_path_goes_where_the_file_is_now) {
    CHECK_EQ(layout::relativeNow("uaro-graphics.ini"), std::string("uaro-graphics\\uaro-graphics.ini"));
    CHECK_EQ(layout::relativeNow("UARO-GRAPHICS.INI"), std::string("uaro-graphics\\uaro-graphics.ini"));
    CHECK_EQ(layout::relativeNow("dinput8.ini"), std::string("dinput8.ini"));
    CHECK_EQ(layout::relativeNow("uaro-graphics\\uaro-graphics.ini"), std::string("uaro-graphics\\uaro-graphics.ini"));
}
```

- [ ] **Step 2: Run them to see them fail**

Run: `cmake --build build-host -j8 2>&1 | tail -3`
Expected: compile error, `core/layout.hpp: No such file or directory`.

- [ ] **Step 3: Implement `src/core/layout.hpp/.cpp`** with the table above as one array indexed by `File`.

- [ ] **Step 4: Run the tests**

Run: `cmake --build build-host -j8 && ./build-host/uagfx_tests 2>&1 | tail -2`
Expected: all pass, the count 4 higher than before.

- [ ] **Step 5: Gate and commit**

Run: `bash $S/gate.sh` → `GATE OK`, then
`git add src/core/layout.* tests/test_layout.cpp && git commit` with
`feat(layout): one table of where uaRO Graphics keeps its files - all in uaro-graphics\ (2.8)`.

### Task 2: Moving a 2.x install

**Files:**
- Create: `src/core/layout_migration.hpp`, `src/core/layout_migration.cpp`
- Test: `tests/test_layout_migration.cpp` (a `MemoryDisk` in the test file)

**Interfaces:**
- Consumes: Task 1's `File`, `now`, `before`, `isFolder`, `join`, `kLogs`, `kCache`, `kTextures`, `kOurs`.
- Produces:

```cpp
namespace uagfx::layout {
struct Disk {  // the file system as the move sees it
    virtual ~Disk() = default;
    virtual bool isFile(const std::string& path) = 0;
    virtual bool isFolder(const std::string& path) = 0;
    virtual bool makeFolder(const std::string& path) = 0;  // one level; true when it is there
    // one rename; replace: a file already at `to` goes. A folder moves whole. false: nothing moved
    virtual bool move(const std::string& from, const std::string& to, bool replace) = 0;
    virtual std::vector<std::string> names(const std::string& folder) = 0;  // its files and folders, sorted
    virtual bool removeFile(const std::string& path) = 0;
    virtual bool removeEmptyFolder(const std::string& path) = 0;
    virtual bool readStart(const std::string& path, size_t most, std::string& out) = 0;
};
// One line for each thing moved, removed or left ("layout: ..."); empty when there was nothing to do.
std::vector<std::string> migrate(const std::string& gameDir, Disk& disk);
}
```

What `migrate` does, in this order (the signature and tests do not fix it):
1. Makes `kOurs`, `kLogs`, `kCache`, `kTextures` when a move needs them (each one level).
2. Files `Ini`, the logs, `Report`, `Pipelines`, the shader caches: before is a file → `move(before, now, true)`.
   The ini replaces what is at now's place: in every 2.x install that is the package's template.
3. `Pack`: before is a file and now's is not → `move(.., false)`; both → leave both, line
   `layout: two texture packs - <now> is used, <before> is left`.
4. Folders `Backups`, `DgVoodoo`: now's missing → move the folder whole; else move each name now's folder lacks, then
   `removeEmptyFolder(before)`.
5. Root `TEST.en.md`, `TEST.uk.md`, `TEST.ru.md` whose first bytes are `# uaRO Graphics` → `removeFile`, line
   `layout: removed <name> - the notes are in uaro-graphics\notes`.
6. A move that returns false: when before is gone and now's is there (the other program moved it), nothing; else line
   `layout: could not move <before> - it is used where it is`.
Lines for moves: `layout: moved <before> to <now>`. Nothing else in the folder is touched.

- [ ] **Step 1: Write the failing tests** (`MemoryDisk`: `std::map<std::string, std::string> files`,
  `std::set<std::string> folders`, `std::set<std::string> locked` whose moves fail,
  `std::function<void(const std::string&)> beforeMove` called first by `move`; a folder move moves every key under it)

```cpp
TEST(migration_moves_a_2x_install_into_uaro_graphics) {
    MemoryDisk d = twoX();  // C:\uaRO with C:\uaRO\uaro-graphics and, as 2.7.58 left them:
    // uaro-graphics.ini "[quality]\r\nmsaa = 2\r\n", uaro-graphics\uaro-graphics.ini "; template",
    // uaro-graphics.log, uaro-render64.log, uaro-graphics-pipelines.txt, uaro-graphics-report.txt,
    // uaro-textures.zip "pack", uaro-graphics\shaders-32.bin
    CHECK(!layout::migrate("C:\\uaRO", d).empty());
    CHECK_EQ(d.files["C:\\uaRO\\uaro-graphics\\uaro-graphics.ini"], std::string("[quality]\r\nmsaa = 2\r\n"));
    CHECK(d.files.count("C:\\uaRO\\uaro-graphics\\logs\\uaro-graphics.log") > 0);
    CHECK(d.files.count("C:\\uaRO\\uaro-graphics\\logs\\uaro-render64.log") > 0);
    CHECK(d.files.count("C:\\uaRO\\uaro-graphics\\logs\\uaro-graphics-report.txt") > 0);
    CHECK(d.files.count("C:\\uaRO\\uaro-graphics\\cache\\pipelines.txt") > 0);
    CHECK(d.files.count("C:\\uaRO\\uaro-graphics\\cache\\shaders-32.bin") > 0);
    CHECK_EQ(d.files["C:\\uaRO\\uaro-graphics\\textures\\uaro-textures.zip"], std::string("pack"));
    for (const char* old : {"uaro-graphics.ini", "uaro-graphics.log", "uaro-render64.log", "uaro-textures.zip",
                            "uaro-graphics-pipelines.txt", "uaro-graphics-report.txt"})
        CHECK(d.files.count(std::string("C:\\uaRO\\") + old) == 0);
    CHECK(layout::migrate("C:\\uaRO", d).empty());  // a second start finds nothing to do
}

TEST(migration_merges_the_backup_and_dgvoodoo_folders) {
    // root uaro-graphics-backup\2026-10-01-120000\files.txt "a", uaro-graphics\backup\2026-10-02-120000\files.txt
    // "b", root dgvoodoo\DDraw.dll "dg" -> both backups under uaro-graphics\backup, the root folders gone,
    // uaro-graphics\dgvoodoo\DDraw.dll "dg"
}

TEST(migration_keeps_both_texture_packs_and_says_so) {
    // root and new pack both there -> both still there, one line containing "two texture packs"
}

TEST(migration_leaves_what_it_cannot_move_and_locate_keeps_it) {
    // d.locked = {"C:\\uaRO\\uaro-textures.zip"} -> it stays, a line containing
    // "could not move uaro-textures.zip"; layout::locate(.., File::Pack, isFile) is "C:\\uaRO\\uaro-textures.zip"
}

TEST(migration_running_at_once_in_two_programs_loses_nothing) {
    // beforeMove moves the root ini itself the first time (the other program) -> the new ini holds the player's
    // text, no line containing "could not move"
}

TEST(migration_removes_only_our_test_notes) {
    // root TEST.ru.md "# uaRO Graphics 2.7.58 — проверка\n" removed; TEST.en.md "my notes" kept
}

TEST(migration_leaves_the_client_files_and_the_bats) {
    // root uaro-flags.grf, update.bat, dinput.dll, uaro-pad.ini all still there after migrate
}
```

Write the five sketched tests out with `CHECK`/`CHECK_EQ` on exactly what each comment names.

- [ ] **Step 2: Run them to see them fail**

Run: `cmake --build build-host -j8 2>&1 | tail -3`
Expected: compile error, `core/layout_migration.hpp: No such file or directory`.

- [ ] **Step 3: Implement `layout::migrate`** as the six rules above.

- [ ] **Step 4: Run the tests** → all pass (`./build-host/uagfx_tests 2>&1 | tail -2`).

- [ ] **Step 5: Gate and commit** `feat(layout): a 2.x install moves into uaro-graphics\ by itself - what cannot move
  is used where it is`.

### Task 3: Logs that keep the previous game and stop growing

**Files:**
- Create: `src/core/log_files.hpp`, `src/core/log_files.cpp`
- Modify: `src/win/log.cpp` (`open`, `line`)
- Test: `tests/test_log_files.cpp`

**Interfaces:**
- Produces (namespace `uagfx::logfiles`): `constexpr uint64_t kCap = 32ull << 20;`
  `std::string previous(const std::string& path);` (`x\a.log` → `x\a.prev.log`; no `.log`: append `.prev`)
  `std::string part(const std::string& path);` (`x\a.log` → `x\a.1.log`)
  `std::string folderOf(const std::string& path);` (`x\y\a.log` → `x\y`; `""` without a backslash)

- [ ] **Step 1: Write the failing test**

```cpp
TEST(log_files_previous_and_part_names) {
    const std::string log = "C:\\uaRO\\uaro-graphics\\logs\\uaro-graphics.log";
    CHECK_EQ(logfiles::previous(log), std::string("C:\\uaRO\\uaro-graphics\\logs\\uaro-graphics.prev.log"));
    CHECK_EQ(logfiles::part(log), std::string("C:\\uaRO\\uaro-graphics\\logs\\uaro-graphics.1.log"));
    CHECK_EQ(logfiles::previous("C:\\a"), std::string("C:\\a.prev"));
    CHECK_EQ(logfiles::folderOf(log), std::string("C:\\uaRO\\uaro-graphics\\logs"));
    CHECK(logfiles::kCap == 32ull << 20);
}
```

- [ ] **Step 2: Run it to see it fail** (compile error: no `core/log_files.hpp`).
- [ ] **Step 3: Implement `log_files`, then `win/log.cpp`:** `open` makes `folderOf(folderOf(path))` and
  `folderOf(path)` (CreateDirectoryA), moves an existing file to `previous(path)` (MoveFileExA, REPLACE_EXISTING),
  opens `"w"` and counts bytes; `line` writes into the count, and when it would pass `kCap`: closes, moves the file to
  `part(path)`, opens it anew and writes first
  `<time> INFO  log continued: the start of this game's log is in <part's name>`.
- [ ] **Step 4: Run the tests** → pass; `bash $S/gate.sh` → `GATE OK` (driver and host both build `win/log.cpp`).
- [ ] **Step 5: Commit** `feat(log): the previous game's log kept, a new part at 32 MiB`.

### Task 4: The driver uses the table

**Files:**
- Create: `src/win/disk.hpp`, `src/win/disk.cpp` (`class WinDisk : public layout::Disk`, A-calls)
- Modify: `src/win/paths.hpp/.cpp`, `src/win/real_ddraw.cpp` (`decide`), `src/win/context.cpp`,
  `src/win/sound_hook.cpp`, `src/win/gpu/ai11.cpp`, `src/win/gpu/ai_textures.cpp`

**Interfaces:**
- Consumes: Task 1 (`locate`, `join`, `now`, `kCache`), Task 2 (`Disk`, `migrate`), Task 3 (`wlog::open` makes
  the folders).
- Produces (namespace `uagfx::win`): `std::string ourFile(const std::string& gameDir, layout::File f);` (`locate` with
  GetFileAttributesA) and `std::string ourFile(layout::File f);` (with `gameDir()`);
  `bool makeFolders(const std::string& path);` (each level, true when there).

- [ ] **Step 1: `WinDisk`:** GetFileAttributesA, CreateDirectoryA, MoveFileExA (`MOVEFILE_REPLACE_EXISTING` only when
  `replace`; never `MOVEFILE_COPY_ALLOWED`: a pack is never copied), FindFirstFileA/FindNextFileA sorted without
  `.`/`..`, DeleteFileA, RemoveDirectoryA, CreateFileA + ReadFile for `readStart`.
- [ ] **Step 2: `decide()`:** `WinDisk disk; const auto moved = layout::migrate(dir, disk);` before the ini is read;
  the ini from `ourFile(dir, File::Ini)`; `wlog::open(layout::join(dir, layout::now(File::DriverLog)), ..)`; then
  `LOG_INFO("%s", line)` for each moved line; `makeFolders(layout::join(dir, layout::kCache))`; the `settings:` line
  names the ini's path.
- [ ] **Step 3: Every other path of ours:** the five `gameDir() + "\\uaro-graphics.ini"` of `context.cpp` and the one
  of `sound_hook.cpp` → `ourFile(File::Ini)`; `uaro-graphics-pipelines.txt` → `ourFile(File::Pipelines)`;
  `shaders-32.bin`, `shaders-d3d11.bin` → `ourFile(File::Shaders32)`, `ourFile(File::ShadersD3d11)`; the pack in
  `ai11.cpp` and `ai_textures.cpp` → `ourFile(gameDir, File::Pack)`.
- [ ] **Step 4: Check nothing old is left in the driver**

Run: `grep -rnE '"\\\\(uaro-graphics\.ini|uaro-graphics\.log)' src/win` and
`grep -rnE '"\\\\(uaro-graphics-pipelines|uaro-textures\.zip)' src/win`
Expected: no output. Then `bash $S/gate.sh` → `GATE OK`.
- [ ] **Step 5: Commit** `feat(driver): its files in uaro-graphics\ - a 2.x install moved at the game's start`.

### Task 5: The 64-bit host and Studio use the table

**Files:**
- Modify: `src/host/main.cpp` (the three logs, `shaders-64.bin`), `src/host/pack_builder.cpp` (the pack's default
  path, `shaders-64.bin` twice), `src/host/studio_engine.cpp` (`studio-shaders-64.bin`),
  `src/studio/catalog_work.hpp` (`packPath`), `src/studio/catalog_work.cpp`, `src/studio/main.cpp` (the ini)

**Interfaces:**
- Consumes: Task 4's `win::ourFile(gameDir, File)` (Studio links `src/win/paths.cpp`; it converts with `app::ansi`
  and `app::wide`), Task 1's `join`, `now`.

- [ ] **Step 1:** the host's logs → `layout::join(a.dir, layout::now(File::PackLog))`, `StudioEngineLog`,
  `Render64Log` (always the new place; `wlog::open` makes the folders); the shader caches → `ourFile(a.dir, ..)`;
  the pack builder's default output → `ourFile(a.dir, File::Pack)` (a pack that could not move goes on being built
  where it is).
- [ ] **Step 2:** Studio: `packPath()` → `app::wide(win::ourFile(app::ansi(gameDir_), layout::File::Pack))`; the
  two ini reads → `File::Ini` the same way.
- [ ] **Step 3: Check and gate**

Run: `grep -rnE '"\\\\(uaro-(render64|pack|studio-engine)\.log)' src/host src/studio`,
`grep -rnE '"\\\\(uaro-textures\.zip|uaro-graphics\.ini)' src/host src/studio`
and `grep -rnE 'shaders-(32|64|d3d11)\.bin' src/host src/studio`
Expected: no output. `bash $S/gate.sh` → `GATE OK`.
- [ ] **Step 4: Commit** `feat(host, studio): logs, caches and the pack in uaro-graphics\`.

### Task 6: The settings app moves and uses the table

**Files:**
- Modify: `src/app/files.hpp/.cpp` (`moveFile`), `tests/fake_windows.hpp/.cpp` (`moveFile`, `locked`)
- Create: `src/app/app_disk.hpp`, `src/app/app_disk.cpp` (`class AppDisk : public layout::Disk` over `files.hpp`,
  UTF-8 ↔ wide), and `ourFile` for the app
- Modify: `src/app/model.hpp/.cpp`, `src/app/report_send.cpp`, `src/app/status_info.cpp`, `src/app/page_pack.cpp`,
  `src/app/page_ai_cache.cpp`, `src/app/page_ai.cpp`, `CMakeLists.txt` (app_disk.cpp into `uagfx_app` and
  `uagfx_tests`)
- Test: `tests/test_model.cpp`

**Interfaces:**
- Consumes: Tasks 1-3 (`logfiles::previous` names the previous log a report attaches).
- Produces: `bool app::moveFile(const std::wstring& from, const std::wstring& to, bool replace);` (MoveFileExW;
  REPLACE_EXISTING only when `replace`); the fake moves a file or a folder's whole subtree and fails for a source in
  `fake::Windows::locked` (`std::set<std::wstring>`); `std::wstring app::ourFile(const std::wstring& dir,
  layout::File f);`; `app::Paths` gains `std::wstring pack;` - `ini`, `backups`, `pack` from `ourFile`, `log` and
  `report` always the new places.

- [ ] **Step 1: Update the model tests to the new layout, add the new ones (failing)**

In `tests/test_model.cpp`: `const std::wstring kIni = L"uaro-graphics\\uaro-graphics.ini";`, every
`fake::path(L"uaro-graphics.ini")` → `fake::path(kIni)`, `kBackups = L"uaro-graphics\\backup"`; the `backup()` helper
keeps writing the 2.x line `uaro-graphics.ini\tuaro-graphics.ini` (so the restore tests restore a 2.x backup). New:

```cpp
TEST(model_moves_a_2x_install_into_uaro_graphics) {
    fake::Windows& w = fake::reset();
    w.folders.insert(fake::path(L"uaro-graphics"));
    w.files[fake::path(L"uaro-graphics.ini")] = "[quality]\r\nsharpen = 70\r\n";
    w.files[fake::path(kIni)] = "; the 2.x package's template\r\n";
    w.files[fake::path(L"uaro-textures.zip")] = "pack";
    app::Model m;
    CHECK(m.paths().ini == fake::path(kIni));
    CHECK_EQ(m.token(setting("quality.sharpen")), std::string("70"));
    CHECK(w.files.count(fake::path(L"uaro-graphics.ini")) == 0);
    CHECK(m.paths().pack == fake::path(L"uaro-graphics\\textures\\uaro-textures.zip"));
    CHECK(w.files.count(m.paths().pack) > 0);
}

TEST(model_a_pack_that_could_not_move_is_used_where_it_is) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(L"uaro-textures.zip")] = "pack";
    w.locked.insert(fake::path(L"uaro-textures.zip"));
    app::Model m;
    CHECK(m.paths().pack == fake::path(L"uaro-textures.zip"));
}

TEST(model_restoring_a_2x_backup_writes_the_ini_in_uaro_graphics) {
    // as model_restoring_the_oldest_of_twenty_backups_keeps_it with one backup: after restore the new ini holds
    // its text and there is no root uaro-graphics.ini
}

TEST(model_a_new_backup_names_the_ini_where_it_is_now) {
    // after an apply, the newest backup's files.txt holds "uaro-graphics.ini\tuaro-graphics\\uaro-graphics.ini\r\n"
}
```

- [ ] **Step 2: Run to see them fail** (`./build-host/uagfx_tests 2>&1 | grep -E "FAIL|passed" | head`): the new tests
  and the updated ones fail (the model still reads the root ini).
- [ ] **Step 3: Implement:** `Model::Model()` runs `layout::migrate(utf8(dir), AppDisk)` before the paths, makes
  `uaro-graphics` and `uaro-graphics\logs`; `backupOnce` keeps the ini by its relative path now; `restore` writes a
  line naming the ini (old or new relative) to `paths_.ini` and others to `layout::relativeNow(..)`; `machine()` and
  the pack pages use `paths().pack`; `status_info` reads «Що нового» from `layout::notes(lang)` and the pack from
  `ourFile`; `report_send`: the uploaded files read from `logs\` (`uaro-graphics.log` 1 MiB, its `previous` as
  `uaro-graphics.prev.log` 512 KiB, `uaro-render64.log`, `uaro-pack.log`) and `paths().ini`, the watcher reads
  `logs\uaro-graphics.log`, `sendUsage` reads the ini by `ourFile`.
- [ ] **Step 4: Run the tests** → all pass; `bash $S/gate.sh` → `GATE OK`.
- [ ] **Step 5: Commit** `feat(app): the settings app's files in uaro-graphics\ - a 2.x install moved, 2.x backups
  restored into the new ini`.

### Task 7: Package, scripts and a check that keeps paths in one place

**Files:**
- Create: `tools/check_layout.py`
- Modify: `tools/package.sh`, `tools/check_package.py`, `dist/install.bat`, `dist/uninstall.bat`, `$S/gate.sh`

**Interfaces:**
- Consumes: the layout of Task 1 (the package's template at `uaro-graphics/defaults/uaro-graphics.ini`, the notes at
  `uaro-graphics/notes/TEST.<lang>.md`).

- [ ] **Step 1: `tools/check_layout.py`:** fails, naming file and line, when any `src/**/*.cpp|hpp` other than
  `src/core/layout.cpp` has a path literal to a file of ours:
  `\\\\(uaro-graphics\.ini|uaro-graphics\.log|uaro-render64\.log|uaro-pack\.log|uaro-studio-engine\.log|`
  `uaro-graphics-pipelines\.txt|uaro-graphics-report\.txt|uaro-textures\.zip|uaro-graphics-backup|TEST\.)` or
  `uaro-graphics\\\\(shaders-|studio-shaders-)`. Run `python3 -I tools/check_layout.py` → `OK` (Tasks 4-6 left none).
- [ ] **Step 2: `check_package.py` first (watch it fail on today's archive):** `EXPECTED` without the root
  `TEST.*.md`, `uaro-flags.grf`, `uaro-graphics/uaro-graphics.ini`, with `uaro-graphics/notes/TEST.en.md`, `.uk.md`,
  `.ru.md` and `uaro-graphics/defaults/uaro-graphics.ini`; and two checks of install.bat: the line
  `if not exist "uaro-graphics.ini" goto :old_ini_done` comes before the first `>>"uaro-graphics.ini"`, and it holds
  `move /y "ddraw.dll" "uaro-graphics\dgvoodoo\DDraw.dll"`.
  Run: `python3 -I tools/check_package.py build-win/uaro-graphics.zip` → `FAIL: layout ...`.
- [ ] **Step 3: `package.sh`:** the TEST files into `uaro-graphics/notes/`, `dist/uaro-graphics.ini` into
  `uaro-graphics/defaults/`, no `uaro-flags.grf`; `python3 -I tools/check_layout.py` before packing.
- [ ] **Step 4: `install.bat`:** dgVoodoo into `uaro-graphics\dgvoodoo` (`mkdir`, `move /y "ddraw.dll"
  "uaro-graphics\dgvoodoo\DDraw.dll"`, D3DImm.dll likewise, dgVoodoo.conf copied there); the template only when no
  ini anywhere: `if not exist "uaro-graphics\uaro-graphics.ini" if not exist "uaro-graphics.ini" copy
  "uaro-graphics\defaults\uaro-graphics.ini" "uaro-graphics\uaro-graphics.ini" >nul`; the 0.3-0.39 key additions
  guarded by `if not exist "uaro-graphics.ini" goto :old_ini_done` ... `:old_ini_done` (they edit a 2.x root ini before
  the driver moves it); the root ini's "older than 0.3 → template" replacement and root template copies removed; the
  closing text points at `uaro-graphics\logs`.
- [ ] **Step 5: `uninstall.bat`:** dgVoodoo back from `uaro-graphics\dgvoodoo` (the old root `dgvoodoo` too); removes
  `uaro-graphics\cache`; its closing text: the settings `uaro-graphics\uaro-graphics.ini`, the logs
  `uaro-graphics\logs`, the pack `uaro-graphics\textures\uaro-textures.zip` stay - delete `uaro-graphics` if you like.
- [ ] **Step 6: Build and check the package**

Run: `bash tools/package.sh` → ends with `OK: build-win/uaro-graphics.zip: ...` and `packed ...`.
- [ ] **Step 7:** add `python3 -I tools/check_layout.py || { echo "GATE FAILED (layout)"; exit 1; }` to `$S/gate.sh`;
  `bash $S/gate.sh` → `GATE OK`; commit `feat(package): notes and the template in uaro-graphics\, dgVoodoo kept in
  uaro-graphics\dgvoodoo, no uaro-flags.grf; a check keeps our paths in core/layout`.

### Task 8: Notes for the tester and the last checks

**Files:**
- Create: `$S/notes/2.8.0.ru.md`, `$S/notes/2.8.0.uk.md`, `$S/notes/2.8.0.en.md`

- [ ] **Step 1: The notes** (the `TEST` format of the earlier versions): what moved where, and what to check on
  Windows after the update from 2.7.58 by «Оновити зараз»:
  1. the root holds, of ours, `ddraw.dll`, `uaro-graphics.exe`, `dinput.dll` with the pad, the `.bat` files and
     `uaro-flags.grf` - no `uaro-graphics.ini`, logs, `uaro-graphics-pipelines.txt`, `uaro-textures.zip`,
     `uaro-graphics-backup`, `dgvoodoo`, `TEST.*.md`;
  2. the settings are as before (quality, effects, the pad);
  3. HD textures shown (the pack in `uaro-graphics\textures\`), Studio sees the pack;
  4. `uaro-graphics\logs\` has the logs, `uaro-graphics.prev.log` after the second start;
  5. «Що нового в 2.8.0» opens; «Повідомити про проблему» sends a report;
  6. «Сервіс» restores a backup made before the update;
  7. uninstall.bat brings dgVoodoo back.
- [ ] **Step 2:** `bash $S/gate.sh` → `GATE OK`; self-review of `git diff <start>..HEAD` against this plan's Review
  Focus; tell the user it is ready to ship as 2.8.0 and wait for their word.
