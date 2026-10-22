# Update Channels on GitHub Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> The user works inline («делай, и делай все остальное без остановки»): executing-plans, no subagents; the final
> review is a self-review.

**Goal:** uaRO Graphics updates itself from GitHub - «Реліз» = branch `main` for every player, «Бета» = branch `beta`
for the testers - and 3.0.0-beta.2 (HDR off by default, the crash-report question, the channels) goes out on `beta`.

**Architecture:** core/update_check knows the channels, the raw.githubusercontent.com URLs and update.ini format 2
(each archive named by the commit that holds it, so a cached update.ini always matches its archives). app/download
reaches any https host; app/update_run asks the update.ini of the computer's channel; the installer puts a beta
build's install on «Бета»; the Model records the channel at the app's start and keeps it across a reset; the Service
page switches it. tools/release_ini.py writes release/update.ini; the release scripts push `beta` (and `main` at a
release).

**Tech Stack:** C++17 (WinHTTP), Python 3 (tools), bash (release scripts).

**Spec:** `docs/superpowers/specs/2026-10-23-update-channels-design.md` (approved 2026-10-23: «делай»).

## Global Constraints

- Channels: release = branch `main`, beta = branch `beta`; tokens `release`, `beta` in `[update] channel`.
- update.ini URL: `https://raw.githubusercontent.com/kwebmn/ragnarok-graphics/<branch>/release/update.ini`; an
  archive: `https://raw.githubusercontent.com/kwebmn/ragnarok-graphics/<ref>/<file>` (ref = 40 lower-case hex).
- No cache-busting query on GitHub; uaro.kiev.ua keeps the news, the reports and the game's files only.
- `main` never gets a `-beta` version (the 2.8 updater reads it); pushes: `beta` by me, `main` only on «выпускай».
- ddraw.dll imports only kernel32/user32/gdi32/msvcrt; the app's imports pass tools/check_exe.py; lines <= 120;
  texts uk/ru/en; the gate after every task.

## Review Focus

1. A tester whose ini has no channel line, on a beta build, updated to a release build: stays on «Бета» (the app
   recorded the channel at its start) - Task 3 `model_records_the_channel_at_its_start`.
2. A download of 3.0.0-beta.2 cut off, then 3.0.0-beta.3 offered (the same file name): the old .part is never
   continued into the new archive - Task 2 `update_never_continues_another_versions_part`.
3. GitHub's cache serving the old update.ini for 5 minutes: it names its own commit's archives, which still match -
   Task 1 `update_urls_name_the_branch_and_the_commit`.
4. A player on «Реліз» who runs a beta setup by hand: joins «Бета» - Task 3
   `installing_a_beta_build_joins_the_beta_channel`.
5. GitHub not answering: the check fails with the host's name and nothing else changes - Task 2
   `update_check_reads_its_channels_branch` (no web answer).

---

### Task 1: Channels, GitHub's URLs, update.ini format 2

**Files:** Modify `src/core/update_check.hpp/.cpp`, `tests/test_update_check.cpp`.

**Interfaces (produces):**
```cpp
namespace uagfx::update {
enum class Channel { Release, Beta };
constexpr const char* kRawHost = "raw.githubusercontent.com";
const char* channelToken(Channel c);                  // "release", "beta"
bool preRelease(const std::string& version);          // "3.0.0-beta.2" (a '-' after the numbers)
Channel channelOf(const std::string& token, const std::string& installed);  // the token's; else beta for a
                                                                             // pre-release build, release
std::string manifestUrl(Channel c);  // .../main/release/update.ini, .../beta/release/update.ini
std::string archiveUrl(const Archive& a);  // .../<ref>/<file>
std::string localName(const Archive& a);   // "<sha256's first 12>-<the file's last part>"
struct Archive { std::string version, ref, file, sha256; uint64_t size = 0; };
}
```
parseManifest reads format 2 only: `[release] format = 2, version, withdrawn`; each archive `ref`, `file` (a
repo path: letters, digits, `. _ -` and `/` between parts, no `..`, at most 120), `size`, `sha256` (+ `version` for
dlss/models). `kFiles`, `kManifest` go; `kSite` stays (uaro.kiev.ua's other services).

- [ ] Step 1: tests `update_urls_name_the_branch_and_the_commit`, `manifest_2_reads_the_commits_and_paths` (the
  converted `manifest_reads_the_newest_release`), `manifest_refuses_what_is_not_format_2` (format 1, a ref of 39 hex,
  upper-case or non-hex, `file = release/../x`, `file = /release/x`, `file = release\\x`),
  `update_channel_by_the_line_else_by_the_build`
  (`channelOf("beta","3.0.0")` Beta, `channelOf("release","3.0.0-beta.2")` Release, `channelOf("","3.0.0-beta.2")`
  Beta, `channelOf("","3.0.0")` Release, `channelOf("nightly","3.0.0")` Release), `localName` of two shas differ.
- [ ] Step 2: run - FAIL (compile: no Channel).
- [ ] Step 3: implement.
- [ ] Step 4: run - PASS; commit `feat(update): channels and GitHub's update.ini (format 2: archives by their commit)`.

### Task 2: The app downloads from GitHub, the check per channel

**Files:** Modify `src/app/download.hpp/.cpp`, `src/app/update_run.hpp/.cpp`, `src/app/update.hpp`,
`tests/fake_windows.cpp`, `tests/test_update_run.cpp`.

**Interfaces:** `bool downloadUrl(const std::string& url, const std::wstring& to, progress, std::string& error);`
`bool fetchUrl(const std::string& url, std::string& out, std::string& error, size_t most = 64u << 10);` (https only;
the host named in errors) replace `download(file...)`/`fetch(file...)`; `update::Channel channelOf(const std::wstring&
dir);` (the ini's `[update] channel`, else by `installedVersion`); `CheckResult::channel`; fetchArchive downloads
`archiveUrl(a)` into `uaro-graphics\update\<localName(a)>`.

- [ ] Step 1: tests (fake web keyed by URL): `update_check_reads_its_channels_branch` (a beta line → the beta URL's
  version offered; no line + an installed 3.0.0-beta.1 → beta; an installed 3.0.0 → main; no answer → !ok with
  "raw.githubusercontent.com"), `update_never_continues_another_versions_part` (a `.part` of another sha in the work
  folder is not the new archive's), the existing update tests moved to GitHub's URLs.
- [ ] Step 2: run - FAIL.
- [ ] Step 3: implement.
- [ ] Step 4: run - PASS; gate; commit `feat(update): updates from GitHub's branch of the computer's channel`.

### Task 3: The channel kept - the installer and the Model

**Files:** Modify `src/app/installer.cpp`, `src/app/model.hpp/.cpp`, `src/app/main_window.cpp`, the installer's
test file, `tests/test_model.cpp`, `dist/uaro-graphics.ini` (an `[update]` section of comments).

**Interfaces:** `update::Channel Model::channel() const;` `std::wstring Model::setChannel(update::Channel c,
settings::Lang l);` (written at once, like the statistics' answer) `void Model::recordChannel(settings::Lang l);` (no
line → the derived one written; called before the start's update check); keptAcross keeps `update.channel`;
finishInstall writes `[update] channel = beta` for a pre-release version.

- [ ] Step 1: tests `installing_a_beta_build_joins_the_beta_channel` (a release line → beta after a beta package; a
  release package leaves a beta line), `model_records_the_channel_at_its_start` (no line + beta build → `beta`
  written; a later release build keeps it), `model_keeps_the_channel_through_a_reset`.
- [ ] Step 2: run - FAIL. Step 3: implement. Step 4: run - PASS; gate; commit.

### Task 4: The Service page and the texts

**Files:** Modify `src/app/page_service.cpp`, `src/core/settings/strings.hpp/.cpp`.

- «Канал оновлень» (a choice: «Реліз», «Бета (тестування)», with a hint) in the Detailed and Developer modes; a change
  writes the channel, checks again, rebuilds. Texts changed: UpdateChecking («Запитую на GitHub найновішу версію...»),
  UpdateNote (updates from GitHub, the channel named). New: UpdateChannel, UpdateChannelRelease, UpdateChannelBeta,
  UpdateChannelHint.
- [ ] Steps: texts; the page; gate (every text used, the names check) - GATE OK; commit.

### Task 5: Release tooling

**Files:** Create `tools/release_ini.py` (with `--self-test`); Delete `tools/publish_site.py`,
`tools/test_publish_site.py`; Modify `docs/superpowers/specs/2026-10-22-beta-release-design.md` (decisions 4, 14
point to the new design). Scratchpad: `ship-beta.sh`, `ship-release.sh`, gate.sh runs `release_ini.py --self-test`.

`release_ini.py RELEASE_DIR VERSION REF [--dlss VERSION] [--models VERSION] [--withdraw V ...]` writes
RELEASE_DIR/update.ini (format 2): the package `release/uaro-graphics.zip`, the dlss and models zips when present,
each with REF, size and sha256; `withdrawn` carried over from the old file.
- [ ] Steps: self-test written first (FAIL: no script), the script, PASS; the scripts; commit.

### Task 6: 3.0.0-beta.2 on the beta channel

- [ ] Local git: `beta` fast-forwarded to this work, checked out; local `main` back to GitHub's main (0dfe661).
- [ ] `$S/notes/3.0.0-beta.2.{uk,ru,en}.md`; `ship-beta.sh 3.0.0-beta.2` (version, gate, notes, package, the archives'
  commit, update.ini's commit, push `beta`, check the raw files' sha256 against update.ini).
- [ ] The WSL task `UPDATE-3.0.0-beta.2.md` and its page; memory.
