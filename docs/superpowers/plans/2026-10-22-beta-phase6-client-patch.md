# Beta Phase 6: Game Files (Client Patcher) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> The user works inline («делай все фазы без остановки», «в инлайн режиме»): executing-plans, no subagents; the final
> review is a self-review.

**Goal:** a change of the game's files reaches every player by itself: the admin uploads a file with its place in the
game's folder («Оновлення клієнта»), the site keeps the list with sizes and checksums, and the launcher «Гра» brings
what is missing before «Грати» - checked, written while the game is closed, never DATA.INI.

**Architecture:** The site: a table of patch files, the files themselves as static `/files/client/<sha256>` (nginx,
Range), a public manifest in the client build's shared format 1 (`GET /api/public/client-patch`), an admin page with a
chunked upload (PHP takes 2 MB a file: 1 MB pieces). The app: `core/client_patch` (the manifest, the safe paths, what
is needed, the record of what was put), `app/client_patch` (the check on «Гра»'s thread, the download with progress,
the files put in place), «Гра» shows it and «Грати» applies it first.

**Tech Stack:** PHP 8.4 Laravel + PHPUnit; React/TypeScript admin (vite); C++17 app (WinHTTP via app/download).

**Spec:** `docs/superpowers/specs/2026-10-22-beta-release-design.md` (phase 6), the page QjTrLqpnyyw8qKiUZQ3i24
(«Файлы игры: патчер клиента»), decisions 7 (the watermark `data\texture\scr_logo.bmp` as the first patch) and 17;
the manifest format of `/root/uaro-client-build/CLAUDE.md` §6 (the client build's contract).

## Global Constraints

- Never DATA.INI; never what uaRO Graphics owns (`ddraw.dll`, `uaro-graphics.exe`, `uaro-graphics.ini`, `dinput.dll`,
  `*.bat` in the root, `TEST.*.md`, `dgvoodoo/`, `uaro-graphics/`, `uaro-*.log`, `uaro-graphics-*.txt`); never the
  player's (`ScreenShot/`, `Emblem/`, `_tmpEmblem/`); never programs (`.exe .dll .bat .cmd .com .scr .ps1 .vbs .js
  .msi .lnk .asi`) - refused by the site and again by the app.
- Paths: relative, `/`, ASCII letters, digits, `. _ - ( ) space` (the game's loose names in another code page wait for a
  later phase), no `..`, at most 200 characters.
- Nothing is deleted on the player's computer: a file taken off the list stays where it is.
- The game running: nothing is written (the game holds its GRFs FILE_SHARE_READ).
- Site additive (new table, routes, static folder `web/files/client/` owned web71:client1); tested under
  DatabaseTransactions; the admin bundle deployed with the old one kept; permission `serverops.maintenance`.
- Releases only on the user's word.

## Review Focus

1. A manifest entry `../../Windows/x.dll` or `DATA.INI` or `ddraw.dll`: refused by the app even when the site sent it
   - Task 3 `client_patch_refuses_unsafe_paths`.
2. A download that arrives corrupted (another sha256): not put in place, the old file stays - Task 4
   `a_corrupted_download_is_not_put`.
3. A player who never opens «Гра» (direct starts with `direct_start = 1`): nothing breaks, the patch waits - manual.
4. The site down while «Грати»: the game starts as it is, no error box - Task 4
   `no_site_no_patch_the_game_starts`.
5. Many small files: the check does not hash the whole data folder - only the listed files (Task 3 `needed`).

---

### Task 1: The site - the list, the files, the manifest

**Files (site):** Create `database/migrations/2026_10_22_210001_create_client_patch_files_table.php`,
`app/Models/ClientPatchFile.php`, `app/Http/Controllers/Api/Public/ClientPatchController.php`,
`app/Http/Controllers/Admin/ClientPatchController.php`, `tests/Feature/Admin/ClientPatchTest.php`; Modify
`routes/api.php`.

**Interfaces:**
- `client_patch_files`: id, path (string 200, unique), size (bigint), sha256 (char 64), note (string 500 null),
  active (bool, default 1), admin_id (null), created_at, updated_at.
- `GET /api/public/client-patch` → `{"format":1,"client":"uaRO","version":"p<12 hex of the list's sha256>","built":
  ISO,"files":[{"path","size","sha256"}]}` (active only, sorted case-insensitively by path); `Cache-Control: public,
  max-age=60`.
- Admin (`auth:admin`, `permission:serverops.maintenance`), prefix `/api/admin/client-patch`: `GET /` (all, newest
  first), `POST uploads` {path, size, sha256, note} → {id} (the path checked here), `POST uploads/{id}` {offset, data
  base64 ≤ 1 MB} → {received}, `POST uploads/{id}/finish` → the file checked (size, sha256), moved to
  `web/files/client/<sha256>`, the path's row made or replaced (active), `PATCH {id}` {note, active}.
- `ClientPatchFile::pathError(string $path): ?string` (the rules above, Ukrainian messages).
- [ ] Steps: tests `the_manifest_lists_the_active_files_in_the_shared_format`, `unsafe_paths_are_refused`
  (DATA.INI, data.ini, ../x, ddraw.dll, uaro-graphics/x, x.exe, ScreenShot/a.jpg, non-ASCII), `an_upload_in_pieces_
  becomes_a_listed_file` (2 pieces, sha256 checked, a wrong sha256 refused), `uploads_need_the_permission` → RED →
  migrate, implement → GREEN.

### Task 2: The admin page «Оновлення клієнта»

**Files (site):** Create `frontend/admin/src/pages/ClientPatchPage.tsx`; Modify `frontend/admin/src/App.tsx`,
`frontend/admin/src/shell/AppShell.tsx`; build into `$S`, deploy keeping the old bundle.
- The list: number, path, size, sha256 (first 12), note, date, «Вимкнути» / «Увімкнути»; the form: the file, its path
  (offered `data/texture/<name>`), the note, «Завантажити» with a progress bar (sha256 by crypto.subtle, 1 MB pieces).
- [ ] Steps: tsc (no new errors), vite build, deployed, the bundle answers 200.

### Task 3: Core - the manifest, the safe paths, what is needed

**Files:** Create `src/core/client_patch.hpp/.cpp`, `tests/test_client_patch.cpp`.

**Interfaces:**
```cpp
namespace uagfx::client_patch {
constexpr const char* kPath = "/api/public/client-patch";
constexpr const char* kFiles = "/files/client/";
struct File { std::string path, sha256; uint64_t size = 0; };  // path with '/'
struct Manifest { std::string version; std::vector<File> files; };
bool parse(const std::string& json, Manifest& out, std::string& error);  // format 1; unsafe entries dropped
bool safePath(const std::string& path);
std::string localPath(const std::string& path);  // '/' -> '\'
struct Have { bool exists = false; uint64_t size = 0; std::string sha256; };  // sha256 "" when not computed
// What to fetch: a file missing, of another size, or (`hash` asked) of another sha256
std::vector<File> needed(const Manifest& m, const std::function<Have(const File&)>& have);
// The record uaro-graphics\client-patch.txt: the version put and its files ("path size sha256" lines)
std::string record(const Manifest& m);
bool upToDate(const Manifest& m, const std::string& recordText);  // same version, every file there with its size
}
```
- [ ] Steps: tests `client_patch_reads_the_shared_manifest`, `client_patch_refuses_unsafe_paths`,
  `client_patch_fetches_only_what_differs`, `client_patch_record_round_trip` → RED → implement → GREEN.

### Task 4: The app - check, fetch, put

**Files:** Create `src/app/client_patch.hpp/.cpp`, `tests/test_client_patch_app.cpp`; Modify `src/core/layout.hpp`
(`kClientPatchRecord = "uaro-graphics\\client-patch.txt"`, staging `uaro-graphics\update\client`), `CMakeLists.txt`,
`tests/fake_windows.*` (web for `/files/client/<sha>`).

**Interfaces:**
```cpp
struct PatchCheck { bool asked = false; std::string error; std::vector<client_patch::File> needed; uint64_t bytes = 0;
                    std::string version; };
PatchCheck checkClientPatch(const std::wstring& dir);           // the site's list against the disk
// The needed files fetched (progress over all of them), checked, put; false and why - nothing half-put
bool applyClientPatch(const std::wstring& dir, const PatchCheck& c, const std::function<bool(uint64_t, uint64_t)>&
                      progress, std::string& error);
```
- [ ] Steps: tests `the_patch_puts_the_missing_files_and_records_them`, `a_corrupted_download_is_not_put`,
  `no_site_no_patch_the_game_starts` (check fails quietly: asked false), `a_patch_put_is_not_fetched_again` → RED →
  implement → GREEN.

### Task 5: «Гра» and «Грати»

**Files:** Modify `src/app/launcher.hpp/.cpp` (the state carries the patch check), `src/app/page_play.cpp` (a line
«Оновлення гри: N файлів, X МБ», «Оновити й грати»), `src/app/modes.cpp` (`playNow`: the patch with the progress
window first; also after an update's `--then-game`), strings.
- [ ] Steps: texts; the gate; manual checks into the notes.

### Task 6: Notes and checks

- [ ] `$S/notes/2.14.0.{ru,uk,en}.md`; gate; self-review; main fast-forwarded; the first patch (scr_logo.bmp from
  /root/uaro-client-build/files) is the user's to upload - offered, not done.
