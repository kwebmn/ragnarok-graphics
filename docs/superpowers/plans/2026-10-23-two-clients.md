# Two Clients Side by Side Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> The user works inline («делай все фазы без остановки», «в инлайн режиме»): executing-plans, no subagents; the final
> review is a self-review.

**Goal:** uaRO Graphics in the old uaRO and in the new «uaRO HD» side by side: each copy has its own shortcut and Apps
entry, gets only its client's patch, keeps the player's own Palettes.grf/dinput8.ini, takes away the files the admin
withdrew - shipped as 3.0.1-beta.1 on «Бета».

**Architecture:** core/client_patch learns the client (`uaro-client.json`, the build's manifest), the player's own
files, a plan (put / kept / remove) and a record that remembers what the patch made. app/client_patch asks the site
for its client's list, checks three programs of its folder by full path, puts and removes. app/installer names the
shortcut and the Apps entry by the client and touches only its own; the app's start makes them for an unpacked client.
The site keeps a client per patch file.

**Tech Stack:** C++17 (Win32), PHP 8.4 Laravel + PHPUnit, React/TypeScript (vite).

**Spec:** `docs/superpowers/specs/2026-10-23-two-clients-design.md` (approved 2026-10-23: «норм»).

## Global Constraints

- Client file: `uaro-client.json` in the game's root, format 1 (`client`, `version`, `files`); none: «uaRO».
- Names: shortcut `<client>.lnk`; Apps key/DisplayName «uaRO Graphics» for «uaRO», «uaRO Graphics (<client>)» else.
- Site: `GET /api/public/client-patch?client=<name>` (none: «uaRO»); clients uaRO, uaRO HD; the answer's `client` is
  the one asked.
- The player's own: `Palettes.grf`, `dinput8.ini` (root, any case). Never patched: DATA.INI, `uaro-client.json`, uaRO
  Graphics' files, ScreenShot/Emblem/_tmpEmblem, programs.
- Site changes: additive migration, edited files backed up first, PHPUnit as web71 under DatabaseTransactions, the
  site's git left uncommitted (as before); the admin bundle built into $S and copied (old index kept).
- Branch `beta`; the gate after every task; 3.0.1-beta.1 via $S/ship-beta.sh; main only on «выпускай».

## Review Focus

1. The old uaRO and uaRO HD both on 3.0.1: shortcuts «uaRO» and «uaRO HD», two Apps entries; removing one leaves the
   other's - Task 3 `two_clients_keep_their_own_names`.
2. An HD install whose 3.0.0 wrote the Apps entry «uaRO Graphics»: the app's start renames it - Task 3
   `the_start_renames_an_entry_of_the_old_name`.
3. A patch file that REPLACED a file the client had (data/sclientinfo.xml), then withdrawn: never deleted - Task 1
   `a_withdrawn_file_goes_only_when_the_patch_made_it`.
4. The player's own 4 MB Palettes.grf while the list has the 101 MB one: kept, nothing downloaded - Task 1
   `the_players_own_files_stay`.
5. The other client's game open while this one patches: the patch goes on (full paths) - Task 2
   `another_folders_game_does_not_stop_the_patch`.

---

### Task 1: core/client_patch - the client, the player's own files, the plan, the record

**Files:** Modify `src/core/client_patch.hpp/.cpp`, `tests/test_client_patch.cpp`.

**Interfaces (Produces):**
```cpp
constexpr const char* kClientFile = "uaro-client.json";
constexpr const char* kOldClient = "uaRO";
struct File { std::string path, sha256; uint64_t size = 0; bool made = false; };
struct Manifest { std::string client, version; std::vector<File> files; };  // parse() fills client
std::string clientName(const std::string& name);  // fit for a file and a key, else "uaRO"
bool sameClient(const std::string& a, const std::string& b);  // any case; "" is "uaRO"
bool playersOwn(const std::string& path);
using Look = std::function<Have(const std::string& path, const std::vector<uint64_t>& sizes)>;
struct Plan { std::vector<File> put, kept, remove; };
Plan plan(const Manifest& next, const Manifest& last, const Manifest& build, const Look& look);
void carryMade(Manifest& next, const Manifest& last);
std::string record(const Manifest& m);  // "version V", "client C", "<size> <sha256> made|over <path>"
bool readRecord(const std::string& text, Manifest& out);  // also 3.0.0's lines "<size> <sha256> <path>"
bool upToDate(const Manifest& m, const std::string& recordText, const std::function<long long(const std::string&)>&);
```
(`needed` goes: `plan` replaces it.)

- [ ] **Step 1: failing tests** `client_patch_reads_the_client` (client from the manifest; clientName of "uaRO HD",
  "", "a/b", 41 letters, "x." → "uaRO HD", "uaRO", "uaRO", "uaRO", "uaRO"; sameClient("uaro hd","uaRO HD")),
  `client_patch_refuses_the_client_file` (safePath("uaro-client.json") and "UARO-CLIENT.JSON" false),
  `the_players_own_files_stay` (Palettes.grf: missing → put; as the build → put; as the last patch → put; other →
  kept, no other sizes looked at), `client_patch_puts_what_differs` (the old needed table through plan),
  `a_withdrawn_file_goes_only_when_the_patch_made_it` (made + unchanged → remove; over → stays; changed → stays; in
  the build → stays; still listed in another case → stays), `client_patch_record_round_trip` (made flags and client
  back; 3.0.0's line read; upToDate: another client → false; the player's own of another size → still up to date).
- [ ] **Step 2: run** `cmake --build build-host -j8 && ./build-host/uagfx_tests` - FAIL (compile).
- [ ] **Step 3: implement.**
- [ ] **Step 4: run** - PASS; gate.
- [ ] **Step 5: commit** `feat(patch): the client, the player's own files and withdrawn files in the patch's plan`.

### Task 2: app/client_patch - its client's list, three programs, removals

**Files:** Modify `src/app/client_patch.hpp/.cpp`, `src/app/system.hpp/.cpp` (`running`), `src/app/main_window.cpp`
(doApply: `work()`), `tests/fake_windows.hpp/.cpp`, `tests/test_client_patch_app.cpp`.

**Interfaces:**
- Consumes Task 1's plan/record/readRecord/carryMade/clientName/sameClient.
- Produces: `bool running(const std::wstring& exe);` (any program by its full path; `gameRunning` stays for uaRO.exe
  callers as `running`); `client_patch::Manifest clientOf(const std::wstring& dir);` (uaro-client.json, else «uaRO»
  with no files); `PatchCheck { asked, error, client, manifest, last, needed, kept, remove, bytes; bool work() const; }`.
- Fake: `std::set<std::wstring> running` (full paths, any case) beside `gameRunning`.

- [ ] **Step 1: failing tests** `the_patch_asks_for_its_clients_list` (uaro-client.json «uaRO HD» → the web key
  `/api/public/client-patch?client=uaRO%20HD`; a list answering «uaRO» refused, nothing needed),
  `the_patch_takes_away_what_it_made_and_was_withdrawn`, `the_players_palettes_stay` (app level),
  `another_folders_game_does_not_stop_the_patch` (running C:\other\uaRO.exe → applies; this folder's uaro-studio.exe
  → refused, nothing written), `a_list_with_nothing_to_do_is_recorded` (record written by the check);
  the old tests moved to `?client=uaRO`.
- [ ] **Step 2-4:** FAIL → implement → PASS; gate.
- [ ] **Step 5: commit** `feat(patch): each client its own list; Studio and the renderer closed; withdrawn files go`.

### Task 3: the names - shortcut, Apps entry, the app's start

**Files:** Modify `src/app/registry.hpp/.cpp`, `src/app/installer.hpp/.cpp`, `src/app/main_window.cpp`,
`tests/fake_windows.hpp/.cpp`, `tests/test_installer.cpp`, `tests/test_update_run.cpp`.

**Interfaces:**
```cpp
struct AppsEntry { std::wstring name, dir, version; };
std::vector<AppsEntry> appsEntries();  // the keys under ...\Uninstall starting "uaRO Graphics"
bool writeAppsEntry(const std::wstring& name, const std::wstring& dir, const std::string& version);
bool removeAppsEntry(const std::wstring& name);
namespace installer {
struct Names { std::wstring shortcut, apps; };  // "uaRO HD", "uaRO Graphics (uaRO HD)"
Names namesOf(const std::wstring& dir);
void atStart(const std::wstring& dir);  // the shortcut once (no marker), the Apps entry right
}
```
Fake: `std::map<std::wstring, std::map<std::wstring, std::wstring>> apps` (key name → values) replaces `appsEntry`.

- [ ] **Step 1: failing tests** `two_clients_keep_their_own_names` (old client + HD folder: «uaRO.lnk», «uaRO HD.lnk»,
  both entries; uninstall of HD leaves «uaRO Graphics»), `a_second_copy_does_not_take_the_first_ones_names` («uaRO
  (2).lnk»; the entry stays the first's; the first's folder gone → taken over),
  `the_start_renames_an_entry_of_the_old_name`, `an_unpacked_client_gets_its_shortcut_at_the_start` (no marker →
  shortcut + entry; marker there → no shortcut); the old Apps tests through `apps`.
- [ ] **Step 2-4:** FAIL → implement → PASS; gate.
- [ ] **Step 5: commit** `feat(install): the shortcut and the Apps entry named by the client, each copy its own`.

### Task 4: the site - a client per patch file

**Files (site):** Create `database/migrations/2026_10_23_120001_add_client_to_client_patch.php`; Modify
`app/Models/ClientPatchFile.php`, `app/Http/Controllers/Api/Public/ClientPatchController.php`,
`app/Http/Controllers/Admin/ClientPatchController.php`, `frontend/admin/src/pages/ClientPatchPage.tsx`,
`tests/Feature/Admin/ClientPatchTest.php`.

- client_patch_files + `client` (string 40, default 'uaRO'), unique (client, path) instead of (path);
  client_patch_uploads + `client`. `ClientPatchFile::CLIENTS = ['uaRO', 'uaRO HD']`; pathError refuses
  `uaro-client.json`; manifestFiles($client). Public: `?client=` (regex `^[A-Za-z0-9 ._()-]{1,40}$`, else 422),
  version hash over client + list. Admin: start takes `client` (in CLIENTS, default uaRO), finish keys (client, path).
  Page: «Клієнт» select, a column, the note about withdrawn files and the player's own.
- [ ] **Step 1: backups** (mysqldump of the two tables, the edited files into $S/site-backup-clients); failing tests
  `test_lists_are_per_client`, `test_the_client_file_is_refused`.
- [ ] **Step 2-4:** FAIL → migrate (web71) → implement → PASS (the file's tests; then the full site suite).
- [ ] **Step 5:** build the admin bundle into $S/admin-build-clients, deploy (old index kept), curl the bundle.

### Task 5: notes, review, beta

- [ ] Notes `$S/notes/3.0.1-beta.1.{uk,ru,en}.md`; self-review against the Review Focus; `bash $S/ship-beta.sh
  3.0.1-beta.1 "..."`; the build's handoff `/root/uaro-client-build/UPDATE-3.0.1.md` (uaro-client.json, the log
  check, publish only with 3.0.1); memory.
