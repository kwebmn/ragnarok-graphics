# Beta Phase 2: Install and Update Without .bat Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> The user works inline («делай все фазы без остановки», «в инлайн режиме»): executing-plans, no subagents, no stop
> between phases; the final review is a self-review.

**Goal:** uaRO Graphics installs, updates (asked at the settings app's and the game's start), goes back to the
previous version and removes itself with its own windows - no `.bat`, no console - and its releases come from
uaro.kiev.ua.

**Architecture:** The site serves static files from `/var/www/uaro.kiev.ua/web/files/graphics/` (nginx, Range
works): `update.ini` (the newest release, format 1) and the archives beside it, written at a release by
`tools/publish_site.py`. Pure parts go to core with host tests (SHA-256, the manifest and the offer, the upgrade of
old settings files that install.bat did, zip from memory); the installer, the downloads and the update run go to the
settings app over `app/files` (host tests through `tests/fake_windows`). `uaro-graphics.exe` gets modes on its command
line (install, update, ask, rollback, repair, uninstall) and becomes the setup itself when a package is appended to
it. The driver never goes to the network: at the game's start the watcher it already starts checks the site, and the
next start asks through `uaro-graphics.exe --ask-update`.

**Tech Stack:** C++17, MinGW-w64 i686, Win32 + WinHTTP + comctl32 (the settings app), miniz (tests' zips), Python 3
(`tools/publish_site.py`), nginx static files.

**Spec:** `docs/superpowers/specs/2026-10-22-beta-release-design.md` (phase 2), the page QjTrLqpnyyw8qKiUZQ3i24
(section «Обновления без консоли», its dialogs), decisions 4 (no channels: rollback matters), 11 (no signing), 14
(updates from uaro.kiev.ua), 18 (name «uaRO»).

## Global Constraints

- Branch `beta/phase-2` from main; at the end main is fast-forwarded to it. Gate after every task
  (`bash $S/gate.sh` → `GATE OK`); release only on the user's word; comments in English, lines ≤ 120; commits by
  KWEB Team with the trailer.
- No `.bat` runs and no console window opens in any flow. The package keeps one tiny `install.bat` only for the
  older `update.bat` (2.x) that downloads this package for the last time; the settings app removes every `.bat` of
  ours (its bytes name "uaRO Graphics") at its next start.
- Updates: `https://uaro.kiev.ua/files/graphics/update.ini` and the archives named in it; every release to everyone;
  a version listed in `withdrawn` is offered the site's version even when that is older.
- All or nothing: a failed or cancelled install, update or rollback leaves every file of the game's folder as it was.
- The game's own files are never touched (uaRO.exe, data, GRFs, DATA.INI); dgVoodoo's and ROExt's files are kept and
  put back on removal.
- `ddraw.dll` imports only kernel32, user32, gdi32, msvcrt and never goes to the network; `uaro-graphics.exe` imports
  only what `tools/check_exe.py` allows (functions Windows 7 has).
- Texts of the new windows in uk/ru/en in `core/settings/strings.cpp`; their language is the one chosen in the app,
  else Windows' (uk, ru, else en). The Apps entry is «uaRO Graphics» (it removes the add-on, not the game).
- The site: only files under `web/files/graphics/` (owner `web71:client1`), only at a release (`ship.sh` calls
  `tools/publish_site.py`); development runs it on a temporary folder.

## Review Focus

1. A game folder that needs administrator rights (Program Files): update, setup and removal ask Windows for them
   (ShellExecuteEx "runas") instead of failing half-way - Task 9 (`elevationNeeded`), manual check in the notes.
2. The connection drops in the middle of the 36 MB package or the 34 MB DLSS: the next try continues the `.part`
   (Range) and a file whose size or SHA-256 is not the site's never reaches the folder - Task 8
   `update_refuses_a_package_that_is_not_the_sites`; the Range continuation is manual (WinHTTP).
3. The game or the texture pack builder holds a file while the update replaces files: nothing is half-replaced -
   Task 5 `installer_undoes_everything_when_a_file_cannot_be_put`.
4. dgVoodoo, ROExt or an old pad in the folder: their files survive install, update and removal - Task 5
   `installer_moves_a_foreign_ddraw_to_dgvoodoo`, Task 7 `uninstall_leaves_the_folder_as_before_the_install`.
5. The site answers an error page, a cut file or a newer format: no offer, no crash - Task 2
   `manifest_refuses_what_is_not_format_1`.

---

### Task 1: SHA-256

**Files:** Create `src/core/sha256.hpp`, `src/core/sha256.cpp`, `tests/test_sha256.cpp`.

**Interfaces:** Produces `namespace uagfx::sha256 { class Hasher { public: void add(const void* data, size_t n);
std::string hex(); }; std::string hexOf(const std::string& bytes); }` - 64 lower-case hex digits; `hex()` ends the
hash (call once).

- [ ] **Step 1: Write the failing tests** `sha256_of_the_standard_messages`: `hexOf("")` =
  `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`, `hexOf("abc")` =
  `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`,
  `hexOf("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")` =
  `248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1`; `sha256_in_parts_is_the_same`: one million
  `'a'` added in parts of 997 bytes = `cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0`.
- [ ] **Step 2: Run** `cmake --build build-host -j8 && ./build-host/uagfx_tests` - Expected: compile error, no
  `core/sha256.hpp`.
- [ ] **Step 3: Implement** FIPS 180-4 SHA-256 (64-byte blocks, big-endian length), no Windows calls.
- [ ] **Step 4: Run** the tests - Expected: PASS.
- [ ] **Step 5: Commit** `feat(core): SHA-256 - the installer checks the site's archives by it`.

### Task 2: The site's update.ini and the offer

**Files:** Modify `src/core/update_check.hpp/.cpp`, `tests/test_update_check.cpp`.

**Interfaces:**
- Produces (the GitHub functions stay until Task 9 stops using them):
  ```cpp
  constexpr const char* kSite = "uaro.kiev.ua";
  constexpr const char* kFiles = "/files/graphics/";  // update.ini and the archives
  constexpr const char* kManifest = "update.ini";
  struct Archive { std::string version, file, sha256; uint64_t size = 0; };  // file "": none
  struct Manifest { Archive package, dlss, models; std::vector<std::string> withdrawn; };
  bool parseManifest(const std::string& text, Manifest& out, std::string& error);
  std::string offer(const Manifest& m, const std::string& installed, const std::string& skip);  // "" none
  std::string bare(const std::string& version);  // "2.10.0 build 2026-10-22 18:00" -> "2.10.0"
  ```
- The format (`core/settings/ini_edit.hpp` `readIni`): `[release] format = 1, version, withdrawn` (versions
  separated by spaces); `[package] file, size, sha256`; `[dlss]` and `[models]` the same plus `version` (their own
  line, as `dist/dlss-version.txt` and `dist/models-version.txt`). Other sections (the app's `[local]`) are ignored.
  A file name is `[A-Za-z0-9._-]{1,80}`; a sha256 64 hex digits (kept lower case); size a decimal > 0.
- `newer` learns pre-releases: `3.0.0-beta.2` > `3.0.0-beta.1` > `2.10.0`, and `3.0.0` > `3.0.0-beta.2`.

- [ ] **Step 1: Write the failing tests**
  - `manifest_reads_the_newest_release`: a full text → `package.version == "2.10.0"`, `package.file ==
    "uaro-graphics-2.10.0.zip"`, `package.size == 35690731`, `dlss.version == "streamline 2.14.1"`, `models.file`
    set, `withdrawn == {"2.9.1", "2.9.2"}`.
  - `manifest_refuses_what_is_not_format_1`: `"<html>502 Bad Gateway</html>"`, `format = 2`, a missing
    `[package] sha256`, `size = -5`, `file = ../ddraw.dll`, a text cut in the middle of the sha → each false with an
    error naming the field.
  - `offer_is_a_newer_version_or_a_way_back_from_a_withdrawn_one`: site 2.10.0 / installed 2.9.0 → "2.10.0";
    installed 2.10.0 → ""; installed 2.11.0 (a developer's) → ""; skip 2.10.0 → ""; installed 2.10.1 withdrawn →
    "2.10.0"; installed "2.9.0 build 2026-10-22 18:00" → "2.10.0".
  - `update_check_compares_versions_by_their_numbers` gains the pre-release lines above.
- [ ] **Step 2: Run** - Expected: compile errors (no `parseManifest`).
- [ ] **Step 3: Implement** with `readIni`; `offer` = withdrawn first, then `newer`, `skip` refusing either.
- [ ] **Step 4: Run** - Expected: PASS (the old GitHub tests still pass).
- [ ] **Step 5: Commit** `feat(core): the site's update.ini (format 1), the version to offer, pre-release versions`.

### Task 3: Older settings files, upgraded by the installer

**Files:** Create `src/core/ini_upgrade.hpp/.cpp`, `tests/test_ini_upgrade.cpp`.

**Interfaces:** Produces `std::string uagfx::upgradeIni(const std::string& text);` - install.bat's additions
(`git show 38e41b3:dist/install.bat`, lines 47-377) in their order, each checked on the text as the ones before left
it: a block of keys the file lacks goes at its end, preceded by an empty line, CRLF endings, its comment lines and
values verbatim (`%%` → `%`); the file's own lines are never changed except the 0.17 pair. The markers: `sharpen`,
`crisp_text`, `msaa` (case-sensitive substrings, as `findstr /c:`), then case-insensitive `[effects]`, `water_waves`,
`ao_strength`, `grade_saturation`, `dof_strength`, `smooth_light`, `torch_glow`, `torch_brightness`,
`smoke_density`, `lamps_light`, `fire_light_strength`, `skill_colours`, `aura_brightness`, `skill_brightness`,
`fireflies_brightness`, `map_glows_brightness`, `rays_brightness`, `lava_brightness`, `portals_strength`,
`actor_shadows_strength`, `clouds_strength`, `haze_strength`, `heat_strength`, `weather_amount`. The smoothing block:
`msaa/alpha_to_coverage/anisotropy` = 4/1/16, but 0/1/4 when the file has `sharpen = 0` and `crisp_text = 0`
(a line `^ *key *= *0` then end or a non-digit, any case), and 0/0/0 when also `high_resolution = 0`. The 0.17 pair:
when both exact lines `; the textures it takes, by their sides in pixels: from min_size to max_size, 16-1024` and
`min_size = 64` are there, they become `; the textures of the world it takes, by their sides in pixels (16 since
0.17)` and `min_size = 16`.

- [ ] **Step 1: Write the failing tests**
  - `ini_upgrade_gives_an_old_file_every_block_once`: `"[display]\r\nmode = window\r\n"` → every marker present
    exactly once (count substrings), the first two lines unchanged, `glow = 0`, `weather = 1`.
  - `ini_upgrade_twice_is_once`: `upgradeIni(upgradeIni(x)) == upgradeIni(x)`.
  - `ini_upgrade_leaves_the_package_template`: `dist/uaro-graphics.ini` (read via `UAGFX_SOURCE_DIR`) unchanged.
  - `ini_upgrade_keeps_a_light_preset_light`: sharpen 0 + crisp_text 0 → `msaa = 0`, `alpha_to_coverage = 1`,
    `anisotropy = 4`; plus `high_resolution = 0` → 0/0/0; otherwise 4/1/16 (`sharpen = 05` is not 0: 4/1/16).
  - `ini_upgrade_moves_0_17_textures_to_16`: the pair rewritten, other `min_size = 64` files (without the comment)
    untouched.
- [ ] **Step 2: Run** - Expected: compile error.
- [ ] **Step 3: Implement** as a table of `{marker, caseSensitive, lines}` plus the two special rules.
- [ ] **Step 4: Run** - Expected: PASS.
- [ ] **Step 5: Commit** `feat(core): install.bat's upgrades of older settings files, now the installer's`.

### Task 4: Zip from memory

**Files:** Modify `src/core/zip.hpp/.cpp`, `tests/test_zip.cpp`; Create `tests/zip_fixture.hpp` (the `makeZip`
helper moved out of test_zip.cpp, `inline`).

**Interfaces:** Produces `bool Index::openBytes(std::string bytes, const std::function<bool(const std::string&)>&
keep, std::string& error);` - `read` then reads from those bytes; `open(path)` unchanged.

- [ ] **Step 1: Write the failing test** `zip_reads_an_archive_in_memory`: `makeZip` of a stored and a deflated item
  → `openBytes` true, `names()` both, `read` gives each one's bytes; a cut archive → false.
- [ ] **Step 2: Run** - Expected: compile error.
- [ ] **Step 3: Implement** a read-at source (FILE* or the bytes) under the existing parsing.
- [ ] **Step 4: Run** - Expected: PASS, the other zip tests too.
- [ ] **Step 5: Commit** `feat(core): zip read from memory - the setup's appended package, the installer's tests`.

### Task 5: The installer puts a package in, all or nothing

**Files:** Modify `src/core/layout.hpp`; Create `src/app/installer.hpp/.cpp`, `tests/test_installer.cpp`; Modify
`CMakeLists.txt` (installer.cpp in the app and in the tests).

**Interfaces:**
- layout: `kPrevious = "uaro-graphics\\previous"`, `kUpdateWork = "uaro-graphics\\update"`, `kUpdateInfo =
  "uaro-graphics\\update.ini"`.
- Produces:
  ```cpp
  namespace uagfx::app::installer {
  struct Result { bool ok = true; std::wstring error; std::vector<std::string> log; };
  // The package's tree in `staged` (uaro-graphics\... as in the zip) into the game's folder `dir`; the root gets
  // ddraw.dll and uaro-graphics.exe from its uaro-graphics\. Each file that differs (every one with `force`): the
  // installed one into uaro-graphics\previous\ first - a root ddraw.dll that is not ours (dgVoodoo's) into
  // uaro-graphics\dgvoodoo\DDraw.dll instead. A step that fails undoes every step before it.
  Result putPackage(const std::wstring& dir, const std::wstring& staged, bool force);
  bool canRollBack(const std::wstring& dir);  // previous\ holds files
  std::string previousVersion(const std::wstring& dir);  // previous\uaro-graphics\version.txt's first word
  Result rollBack(const std::wstring& dir);   // previous\ back in place, all or nothing; previous\ goes
  }
  ```
- Moves only (renames: a running exe can be renamed), journaled; replaced files gather in
  `uaro-graphics\update\previous` and become `uaro-graphics\previous` (the old one removed) only when there are some.

- [ ] **Step 1: Write the failing tests** (fake::Windows; a folder with `uaRO.exe`)
  - `installer_puts_a_package_into_a_fresh_folder`: root `ddraw.dll`, `uaro-graphics.exe` and
    `uaro-graphics\version.txt` are the package's; no `previous`.
  - `installer_keeps_the_replaced_files_in_previous`: v1 installed, v2 differs in `ddraw.dll` only →
    `previous\ddraw.dll` and `previous\uaro-graphics\ddraw.dll` are v1's, `previous\uaro-graphics\version.txt` v1's
    when it differs, unchanged files absent from previous.
  - `installer_undoes_everything_when_a_file_cannot_be_put`: `uaro-graphics\uaro-render64.exe` locked → `ok ==
    false`, every file outside `uaro-graphics\update` exactly as before (maps equal), the old previous kept.
  - `installer_moves_a_foreign_ddraw_to_dgvoodoo`: root `ddraw.dll` without "uaRO Graphics" →
    `uaro-graphics\dgvoodoo\DDraw.dll`.
  - `installer_rolls_back_to_previous`: after v1 → v2, `rollBack` → v1's files, no `previous`;
    `previousVersion` was "1.0.0" before it.
  - `installer_keeps_previous_when_nothing_changed`.
- [ ] **Step 2: Run** - Expected: compile error.
- [ ] **Step 3: Implement** over `app/files` (`moveFile`, `readFile`, `writeFileAtomic` for the root copies staged
  under `update\root\`).
- [ ] **Step 4: Run** - Expected: PASS.
- [ ] **Step 5: Commit** `feat(app): the installer - a package in with the replaced files kept, all or nothing`.

### Task 6: What install.bat did, now the installer's

**Files:** Modify `src/app/installer.hpp/.cpp`, `src/app/model.cpp` (its `removeOldPad` moves here),
`src/app/registry.hpp/.cpp`, `tests/fake_windows.hpp/.cpp`, `tests/test_installer.cpp`.

**Interfaces:**
- Produces:
  ```cpp
  // After the files: a 2.x install's files moved (core/layout_migration), dgVoodoo's D3DImm.dll and a copy of its
  // dgVoodoo.conf into uaro-graphics\dgvoodoo\, the settings file (the template when there is none, an older one
  // upgraded by core/ini_upgrade), an old uaRO Pad dll away (what it moved back), uaro-graphics\pad\ away, our old
  // .bat files away when `scripts` (not while an old update.bat runs this), the folders of kMadeAtStart, the Apps
  // entry.
  Result finishInstall(const std::wstring& dir, bool scripts);
  void removeOldPad(const std::wstring& dir);      // from model.cpp, unchanged
  // install.bat, update.bat, uninstall.bat, get-dlss.bat, get-models.bat, use-dgvoodoo.bat, use-uaro.bat when their
  // bytes name "uaRO Graphics"; the leftover folders uaro-graphics-update, uaro-graphics-dlss-download,
  // uaro-graphics-models-download.
  void removeOldScripts(const std::wstring& dir);
  ```
- `app/registry`: `bool writeAppsEntry(const std::wstring& dir, const std::string& version); bool removeAppsEntry();
  std::wstring appsEntryFolder();` - HKCU `Software\Microsoft\Windows\CurrentVersion\Uninstall\uaRO Graphics`:
  DisplayName «uaRO Graphics», DisplayVersion, Publisher «KWEB Team», DisplayIcon `<dir>\uaro-graphics.exe,0`,
  InstallLocation, UninstallString `"<dir>\uaro-graphics.exe" --uninstall`, URLInfoAbout `https://uaro.kiev.ua`,
  NoModify 1, NoRepair 1. fake::Windows gets `std::map<std::wstring, std::wstring> appsEntry`.

- [ ] **Step 1: Write the failing tests**: `finish_install_puts_the_template_for_a_fresh_install`,
  `finish_install_upgrades_an_old_settings_file` (a root 0.3 ini → `uaro-graphics\uaro-graphics.ini` with
  `sharpen = 50`), `finish_install_moves_dgvoodoos_files_aside`, `finish_install_removes_our_old_scripts_only` (a
  player's `run.bat` and an `install.bat` without "uaRO Graphics" stay), `finish_install_leaves_the_scripts_while_one_runs`,
  `finish_install_writes_the_apps_entry` (UninstallString value exact).
- [ ] **Step 2: Run** - Expected: compile errors.
- [ ] **Step 3: Implement**; `Model()` calls `installer::removeOldPad` and `installer::removeOldScripts` when the
  game is closed.
- [ ] **Step 4: Run** - Expected: PASS, the model's old-pad tests too.
- [ ] **Step 5: Commit** `feat(app): install.bat's work in the installer - dgVoodoo, settings, old pad and scripts, Apps entry`.

### Task 7: Removal

**Files:** Modify `src/app/installer.hpp/.cpp`, `tests/test_installer.cpp`.

**Interfaces:** Produces `Result uninstall(const std::wstring& dir, bool keep);` - our root `ddraw.dll`, dgVoodoo's
files back from `uaro-graphics\dgvoodoo\` and the 2.x `dgvoodoo\` (its `dgVoodoo.conf` copy deleted), the root
`uaro-graphics.exe` (when it cannot go - the running one - `Result::log` says "self" and the caller deletes it),
`uaro-graphics\` whole, or with `keep` all but `uaro-graphics.ini`, `uaro-pad.ini`, `uaro-pad-binds.ini`, `backup\`,
`textures\`; our old scripts and leftovers; the Apps entry.

- [ ] **Step 1: Write the failing tests**: `uninstall_leaves_the_folder_as_before_the_install` (uaRO.exe, data.grf,
  dgVoodoo's DDraw.dll and D3DImm.dll, a root dgVoodoo.conf → putPackage + finishInstall + a game's logs and cache →
  `uninstall(dir, false)` → files and folders equal the first snapshot), `uninstall_keeps_the_settings_and_the_pack`,
  `uninstall_removes_the_apps_entry`.
- [ ] **Step 2: Run** - Expected: FAIL (no `uninstall`).
- [ ] **Step 3: Implement**.
- [ ] **Step 4: Run** - Expected: PASS.
- [ ] **Step 5: Commit** `feat(app): removal - the folder as before the install, settings and pack kept on request`.

### Task 8: Downloads and the update run

**Files:** Create `src/app/download.hpp/.cpp`; Modify `src/app/update.hpp/.cpp`, `tests/fake_windows.hpp/.cpp`;
Create `tests/test_update_run.cpp`.

**Interfaces:**
- `app/download`: `bool download(const std::string& file, const std::wstring& to, const std::function<bool(uint64_t
  done, uint64_t total)>& progress, std::string& error);` - `https://kSite kFiles file` into `to` through
  `<to>.part`, continued with `Range: bytes=N-` when a `.part` is there (206; a 200 starts over), `progress` false
  stops it; `bool fetch(const std::string& file, std::string& out, std::string& error);` (≤ 64 KB, no cache:
  `?<GetTickCount>` after the name). Faked in the tests from `fake::Windows::web` (name → bytes).
- `app/update`:
  ```cpp
  struct UpdateState { enum Kind { Idle, Checking, Done, Failed } kind = Idle; update::Manifest site;
                       std::string offer, error; };
  UpdateState checkNow(const std::wstring& dir);   // fetch, parse, kUpdateInfo = the site's text + [local] skip kept
  void startUpdateCheck(const std::wstring& dir);  // checkNow on a thread
  UpdateState updateState();
  std::string skipped(const std::wstring& dir);    // [local] skip of kUpdateInfo
  void setSkipped(const std::wstring& dir, const std::string& version);
  struct Step { enum What { Download, Wait, Install, Dlss, Done } what = Download; uint64_t done = 0, total = 0; };
  using Report = std::function<bool(const Step&)>;  // false: cancel (before any file of the game's folder changed)
  // The site's package downloaded into kUpdateWork, checked (size, SHA-256), unpacked into kUpdateWork\files; the
  // game (uaRO.exe of `dir`) waited for; installer::putPackage + finishInstall(scripts true); then DLSS on an NVIDIA
  // card when the site's dlss.version is not uaro-graphics\dlss\version.txt's (a failure there leaves DLSS off and
  // the update done). kUpdateWork removed at the end.
  bool runUpdate(const std::wstring& dir, const update::Manifest& m, bool force, const Report& report,
                 std::wstring& error);
  bool runDlss(const std::wstring& dir, const update::Manifest& m, const Report& report, std::wstring& error);
  bool runModels(const std::wstring& dir, const update::Manifest& m, const Report& report, std::wstring& error);
  ```

- [ ] **Step 1: Write the failing tests** (`tests/test_update_run.cpp`, the package a `makeZip` with
  `uaro-graphics\ddraw.dll`, `uaro-graphics\uaro-graphics.exe`, `uaro-graphics\version.txt`):
  `update_check_offers_the_sites_version_and_keeps_skip`, `update_downloads_checks_and_installs` (version.txt new,
  previous holds the old, `uaro-graphics\update` gone, the settings file byte-equal),
  `update_refuses_a_package_that_is_not_the_sites` (one byte changed → false, error has "SHA-256", every file as
  before), `update_cancelled_changes_nothing`, `update_gets_dlss_on_an_nvidia_card` (vendor 0x10de, the site's dlss
  zip → `uaro-graphics\dlss\version.txt`), `update_leaves_dlss_elsewhere`, `models_come_from_the_site`.
- [ ] **Step 2: Run** - Expected: compile errors.
- [ ] **Step 3: Implement** (WinHTTP in download.cpp after the pattern of report_send.cpp's POST; the rest over
  app/files and zip::Index::openBytes).
- [ ] **Step 4: Run** - Expected: PASS.
- [ ] **Step 5: Commit** `feat(app): updates from uaro.kiev.ua - download with progress, SHA-256, install, DLSS, models`.

### Task 9: The windows and the app's modes

**Files:** Create `src/app/args.hpp/.cpp`, `src/app/update_window.hpp/.cpp`, `tests/test_args.cpp`; Modify
`src/app/main.cpp`, `main_window.cpp` (the strip's button), `page_service.cpp`, `page_main.cpp`, `page_ai.cpp`,
`src/app/update.cpp` (the GitHub check and the .bat starters go), `src/core/update_check.*` (GitHub functions go),
`src/app/app.rc` (UPDATE_BAT, GET_MODELS_BAT go), `src/core/settings/strings.hpp/.cpp`, `tools/check_exe.py` (new
imports, each Windows 7's).

**Interfaces:**
- `app/args`:
  ```cpp
  struct Args {
      enum class Mode { Settings, Watch, Install, Update, AskUpdate, Rollback, Repair, Uninstall, UninstallRun } mode =
          Mode::Settings;
      unsigned long pid = 0;       // --watch N, --ask-update N, --after N
      bool quiet = false;          // --install --quiet
      enum class Then { Nothing, App, Game } then = Then::Nothing;  // --then-app, --then-game
      bool keep = true;            // --uninstall-run: --keep 0|1
      std::wstring dir;            // --uninstall-run --dir "<folder>"
      std::wstring gameArgs;       // everything after " -- "
  };
  Args parseArgs(const std::wstring& line);
  std::wstring argsLine(const Args& a);  // parseArgs(argsLine(a)) == a
  ```
- `core/settings/strings`: `Lang windowLang(std::optional<Lang> chosen, unsigned primaryLanguage);` (0x22 uk,
  0x19 ru, else en).
- `app/update_window`: `enum class Answer { Play, Update }; Answer askUpdate(const std::string& version, Lang l);`
  (title «uaRO Graphics», «Вийшло оновлення {1}», «Гра закриється, оновиться й запуститься знову — це близько 20
  секунд.», buttons «Грати без оновлення» / «Оновити», topmost); `bool runWithProgress(const std::wstring& title,
  Lang l, bool thenGame, const std::function<bool(const Report&, std::wstring&)>& work, std::wstring& error);`
  («Оновлюємо до {1}», «Завантажено {1} з {2} МБ. Потім гра запуститься сама.» / «…утиліта відкриється знову.»,
  «Чекаємо, поки гра закриється…», Cancel only while downloading); `bool askUninstall(const std::wstring& dir, Lang
  l, bool& keep);` (checkbox «Залишити налаштування й пакет текстур», on).
- Modes (main.cpp): `--install [--quiet]` (finishInstall after root copies via putPackage(staged = the package's own
  uaro-graphics\ copy, see Step 3), `scripts` false, message unless quiet); `--update [--after N] [--then-app |
  --then-game -- args]` (checkNow, runWithProgress(runUpdate), then start the app or `uaRO.exe args`);
  `--ask-update N -- args` (askUpdate; Update → start `--update --after N --then-game -- args`, exit 10; Play → exit
  0); `--rollback --then-app`; `--repair --then-app` (runUpdate force); `--uninstall` (askUninstall → copy itself to
  `%TEMP%\uaro-graphics-uninstall.exe`, start it `--uninstall-run --dir D --keep K --after N`, exit); `--uninstall-run`
  (wait for N, installer::uninstall, a message, `MoveFileExW(self, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT)` tried).
  Every mode that writes into the folder re-runs itself with "runas" when `folderWritable(dir)` is false
  (`elevationNeeded`).
- The settings app: `startUpdateCheck` at its start; the strip shows «Вийшло оновлення {1}» with a button «Оновити»;
  Service: «Перевірити оновлення», «Оновити» (when offered), «Повернути попередню версію ({1})» (canRollBack),
  «Відновити файли», «Видалити uaRO Graphics»; Main: «Завантажити DLSS» on an NVIDIA card without it; AI page: the
  models' download in a progress window. Each starts its mode as a process of its own and closes the app (the files
  it runs from are replaced).

- [ ] **Step 1: Write the failing tests**: `args_read_every_mode` (each mode's line → the struct),
  `args_round_trip` (argsLine → parseArgs equal, game args with spaces and quotes kept verbatim),
  `window_language_follows_windows_until_one_is_chosen`.
- [ ] **Step 2: Run** - Expected: compile errors.
- [ ] **Step 3: Implement**; the `--install` root copies: putPackage with `staged` = a copy of `uaro-graphics\`'s
  ddraw.dll and uaro-graphics.exe made under `uaro-graphics\update\files\uaro-graphics\` (the rest is already in
  place).
- [ ] **Step 4: Run** the tests and the gate (every text used; check_exe passes) - Expected: PASS, `GATE OK`.
- [ ] **Step 5: Commit** `feat(app): update, rollback, repair and removal windows; the app's modes; no .bat started`.

### Task 10: The game's start

**Files:** Modify `src/core/update_check.hpp/.cpp`, `tests/test_update_check.cpp`, `src/win/real_ddraw.cpp`,
`src/app/report_send.cpp` (`watchGame`).

**Interfaces:** Produces `bool update::askAtStart(const std::string& updateInfo, const std::string& installed,
std::string& version);` - kUpdateInfo's text: parseManifest + `[local] skip` + offer.

- [ ] **Step 1: Write the failing test** `the_game_asks_when_the_last_check_offered`: an offering text → true and
  "2.10.0"; skip 2.10.0 → false; garbage → false; installed equal → false.
- [ ] **Step 2: Run** - Expected: compile error.
- [ ] **Step 3: Implement** `askAtStart`; the driver: the watcher always starts (`--watch <pid>`); in `decide()`,
  after the settings are read and before the device: `askAtStart(kUpdateInfo text, kVersion)` → `CreateProcessA
  "<dir>\uaro-graphics.exe" --ask-update <pid> -- <GetCommandLineA's arguments>`, `AllowSetForegroundWindow`, wait
  ≤ 10 minutes; exit code 10 → log "update: the game closes for it" and `TerminateProcess(GetCurrentProcess(), 0)`.
  `watchGame`: `checkNow` first (the update.ini for the next start), then the report and statistics as before - and
  it ends at once when both are off.
- [ ] **Step 4: Run** the tests and the gate - Expected: PASS, `GATE OK`, ddraw.dll imports unchanged.
- [ ] **Step 5: Commit** `feat: the game's start asks before the login window when the last check found an update`.

### Task 11: The setup is the app with the package

**Files:** Modify `src/core/update_check.hpp/.cpp` (payload), `tests/test_update_check.cpp`, `src/app/main.cpp`,
`src/app/args.*` (Mode::Setup when a payload is there), `tools/package.sh`, `CMakeLists.txt`; Delete `src/setup/`.

**Interfaces:** Produces `bool update::payloadAt(const uint8_t* tail16, uint64_t fileSize, uint64_t& offset,
uint64_t& size);` - the last 16 bytes: the zip's size (8 bytes little-endian) and "UAROSETP".

- [ ] **Step 1: Write the failing test** `setup_finds_the_package_appended_to_it`: a good tail → offset/size; wrong
  magic, size larger than the file → false.
- [ ] **Step 2: Run** - Expected: compile error.
- [ ] **Step 3: Implement**; setup mode: the game's folder (its own when uaRO.exe is there, else SHBrowseForFolder,
  refused without uaRO.exe), the payload through `zip::Index::openBytes` into `uaro-graphics\update\files\`,
  `runWithProgress` (putPackage force, finishInstall, DLSS from the site on NVIDIA), then the installed app opens.
  package.sh: `uaro-graphics-setup.exe` = `build-win/uaro-graphics.exe` + the zip + size + magic.
- [ ] **Step 4: Run** the tests, `bash tools/package.sh` - Expected: PASS, "packed".
- [ ] **Step 5: Commit** `feat: the setup is uaro-graphics.exe with the package appended - no stub, no install.bat`.

### Task 12: The package, the site, the way over from 2.x

**Files:** Modify `dist/install.bat` (tiny), `tools/package.sh`, `tools/check_package.py`, `README.md`; Delete
`dist/update.bat`, `dist/uninstall.bat`, `dist/get-dlss.bat`, `dist/get-models.bat`; Create `tools/publish_site.py`,
`tools/test_publish_site.py`; `$S/ship.sh` and `$S/gate.sh` (not in the repository) call them.

**Interfaces:**
- `dist/install.bat`: `rem uaRO Graphics: ...` then `start "" /wait "uaro-graphics\uaro-graphics.exe" --install %1`
  and `exit /b %errorlevel%`.
- `tools/publish_site.py WEB_DIR VERSION ZIP SETUP [--dlss ZIP VERSION] [--models ZIP VERSION] [--withdraw V ...]`:
  copies `uaro-graphics-VERSION.zip`, `uaro-graphics-setup.exe`, `uaro-graphics-dlss-<safe version>.zip`,
  `uaro-graphics-models-<safe version>.zip` (safe = `[^A-Za-z0-9.]` → `-`), writes `update.ini` (format 1, sizes,
  sha256) through a temporary file and a rename, keeps the newest 5 package zips; `--self-test` runs it on a temp
  folder and checks the text against the format.

- [ ] **Step 1: Write the failing tests**: `tools/test_publish_site.py` (unittest: the manifest's lines, the kept
  zips, a withdraw) and check_package's new EXPECTED (root: `install.bat` only; the tiny install.bat's line).
- [ ] **Step 2: Run** `python3 -I tools/test_publish_site.py` - Expected: FAIL (no script).
- [ ] **Step 3: Implement**; ship.sh: after the package, `publish_site.py /var/www/uaro.kiev.ua/web/files/graphics
  ...`, `chown -R web71:client1` that folder, then `curl https://uaro.kiev.ua/files/graphics/update.ini` names the
  version and the zip's sha256 matches; GitHub `release/uaro-graphics.zip` still gets the zip (the old update.bat).
- [ ] **Step 4: Run** the tests, the package and its check - Expected: PASS.
- [ ] **Step 5: Commit** `feat(package): no .bat but the tiny install.bat; the site's update.ini and archives at a release`.

### Task 13: Notes and checks

- [ ] `$S/notes/2.10.0.{ru,uk,en}.md`: updates from uaro.kiev.ua with a question and a progress window, rollback,
  repair, removal from the app and from Windows' Apps, the setup; what to check: 2.7.58/2.8/2.9 → update by the old
  button (old update.bat, last time) → no .bat left after the app opens; the next release by «Оновити»; the game's
  start question; «Грати без оновлення»; rollback and back; removal (the folder as before, dgVoodoo back); a folder in
  Program Files (the administrator question); a dropped connection continues.
- [ ] Gate, self-review against the Review Focus, main fast-forwarded to `beta/phase-2`.
