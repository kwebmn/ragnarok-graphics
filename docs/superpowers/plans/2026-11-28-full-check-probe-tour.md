# Полная проверка: `probe` и `tour` в MCP игры — план беты 3.0.24-beta.57

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> The owner's rule: inline execution, no agents; betas by gship.

**Goal:** the game checks each of its 611 settings and each of its windows by itself, in the background, and writes
one line of evidence per check into a file the watchdog hands over.

**Architecture:** two jobs that run frame by frame in the game (core: pure step logic, tested on the host; win:
the frame's glue) started and polled by the game MCP's new tools `probe` and `tour` (each call answers at once, the
10 s rule). A setting's check goes the Alt+O way (settingchange::edits + editIniFile, the watcher forced), its trace
is a raw frame from the renderer (shotspec `.bgra`, the portrait's way), the card's passes (GpuTiming), the windows'
words (uaui::check) and the logs' new ERROR lines. loki's `fullcheck.py` drives scenes, restarts, two PCs.

**Tech Stack:** C++17 (driver x86, renderer x64, MinGW), RmlUi (uaUI), the repo's TEST() host tests, Python 3 on loki.

**Spec:** Claude Doc «План полной проверки uaRO Graphics» https://claude.ai/artifact/LNr5kKbJQaPhk9cFxue2Lq —
its «Решения 28.11»: both PCs at once (desktop tunnel 8780/8781), unattended and in the background, the GM character
plus a merchant account without GM, Direct3D 11 on both, the whole run at once (no trial section).

## Global Constraints

- Lines ≤ 120; dist/*.ini stay CRLF; builds `nice -n 19 ionice -c3`, -j4 at most.
- The driver imports no new DLL; no new player-facing words (the tools' descriptions are English, as the others).
- A game MCP call answers within 10 s: `probe`/`tour` only start, stop or report; the work runs in frames.
- The window may be behind others, never minimised; while a job runs the game counts as driven (no idle fps cap).
- Never touched by a check: `mcp.*` (the line itself), `connection.*` (the server), `renderer.api` (its own stage),
  `launcher.*`, `report.*`, `stats.send`, `debug.log` (the probe reads the log); `client.*` and the restart-only
  `renderer.*` keys go one by one with a restart each, at the end of the run.
- No password is ever written to a file by these tools or by me; accounts switch on the client's login shelf.
- The game server (uAthena) is never changed; GM commands are used as a player uses them.
- Results: `uaro-graphics\logs\check-YYYYMMDD-HHMMSS.json`, one JSON object a line (the watchdog's `files get`).

## Review Focus

1. The window minimised mid-run: no frame comes - each wait ends (shot 3 s), the row says «no frame», the job goes on.
2. A setting that changes the picture's size (display.high_resolution, quality.render_scale) - the diff reports
   `sizeChanged`, never compares mismatched rows.
3. A shader compiled on first use for up to 70 s (the laptop) - the settle waits for `loading` false up to 90 s and
   writes how long; it never records «no trace» while loading.
4. The tunnel drops mid-run - the job keeps going and writing; `status` after the reconnect tells where it is.
5. A tour stop whose keys open nothing - no Esc is pressed (Esc would open the game's menu), the row says so.

---

### Task 1: core/check_probe - the setting check's logic

**Files:** Create `src/core/check_probe.{hpp,cpp}`; Test `tests/test_check_probe.cpp`

**Interfaces (namespace `uagfx::checkprobe`):**
- `std::string skipReason(const std::string& sectionKey)` - "" when checked; else why (the list above).
- `std::string testValue(const settings::IniSetting& s, const std::string& now)` - Check: the other token; Choice:
  the next token, round; Slider/Number: max unless now is max, then min; Line: "" (not tried).
- `struct FrameDiff { double changed = 0, mean = 0; bool sizeChanged = false; };`
  `FrameDiff diff(const std::vector<uint32_t>& a, int aw, int ah, const std::vector<uint32_t>& b, int bw, int bh);`
  changed = % of pixels with a channel moved > 12; mean = the channels' mean absolute change (0-255).
- `json::Value passesDelta(const std::map<std::string, double>& before, const std::map<std::string, double>& after)`
  → `{"added":{name:ms}, "gone":[names], "more":{name:+ms}}`, «more» from +0.15 ms.
- `int errorLines(const std::string& logText)` - lines whose level field (after `hh:mm:ss.mmm `) is `ERROR`.
- `struct Item { std::string key, from, to, skip; };` `struct Seen { uint64_t ms; std::string value; bool loading,
  minimised, shotReady; uint32_t gpuSerial; };` `struct Timing { int reachMs = 3000, settleMs = 1200,
  loadingMs = 90000, shotMs = 3000; };`
- `enum class Do : uint8_t { Nothing, ShotBefore, Write, ShotAfter, WriteBack, Record, Finish };`
  `struct Job { std::vector<Item> items; size_t at = 0; /* phase, clocks */ Row row; };`
  `Do step(Job& j, const Seen& s, const Timing& t);` - one frame: what the frame must do now.
- `struct Row { std::string key, from, to, skip, error; bool reached = false, restored = false; int reachMs = 0,
  loadMs = 0; FrameDiff frame; json::Value passes; int uiChanged = 0, logErrors = 0; };`
  `json::Value rowJson(const Row& r);`
- Restart batch: `json::Value restartPlan(const std::vector<Item>& items)`;
  `std::vector<Row> verifyPlan(const json::Value& plan, bool back, const std::function<std::string(const
  std::string&)>& valueOf)` - `back` false: each running value must be `to`; true: `from`.

- [ ] Step 1: tests - `check_probe_skips_the_line_and_the_server` (mcp.enabled, connection.patient_entry,
  renderer.api, debug.log skipped; effects.grass, interface.theme not), `check_probe_test_value_flips_each_kind`
  (a check 0→1 and 1→0, a choice's next and last→first, a slider at max → min, a line → ""), `check_probe_frame_diff`
  (same → 0/0; one pixel +40 of 100 → changed 1.0; sizes 4x4 vs 8x8 → sizeChanged), `check_probe_passes_delta`,
  `check_probe_log_errors` (2 ERROR of 5 lines, «error» inside an INFO line not counted),
  `check_probe_steps_a_setting` (ShotBefore → Write → (value reached) → ShotAfter → WriteBack → (back) → Record →
  Finish; row reached and restored), `check_probe_waits_out_loading` (loading 20 s: no ShotAfter before it ends;
  loadMs ≈ 20000), `check_probe_minimised_gives_no_frame` (minimised: the row's error «no frame», next item),
  `check_probe_unreached_value` (reached false after 3 s, still writes back), `check_probe_restart_plan`.
- [ ] Step 2: `cd build-host && nice -n 19 ionice -c3 make -j4 uagfx_tests && ./uagfx_tests check_probe` - FAIL.
- [ ] Step 3: implement. - [ ] Step 4: the same run - PASS, and the whole `./uagfx_tests`.
- [ ] Step 5: commit `3.0.24-beta.57: core/check_probe - a setting's check, step by step`.

### Task 2: core/check_tour - the windows' tour logic

**Files:** Create `src/core/check_tour.{hpp,cpp}`; Test `tests/test_check_tour.cpp`

**Interfaces (namespace `uagfx::checktour`):**
- `struct Stop { const char* name; const char* keys; };` `const std::vector<Stop>& stops();` - the game's own keys
  (win/command_window windowName): items alt+e, equip alt+q, stats alt+a, skills alt+s, basic alt+v, detail alt+d,
  settings alt+o, cart alt+w, guild alt+g, party alt+p, friends alt+z, whispers alt+h, friend setup alt+i,
  macros alt+m, emotes alt+l, pet alt+j, homunculus alt+r, menu esc.
- `std::vector<std::string> appeared(const std::vector<std::string>& before, const std::vector<std::string>& after)`.
- `bool inside(int x, int y, int w, int h, int screenW, int screenH)`.
- `struct Seen { uint64_t ms; std::vector<std::string> docs; int padFocus; };` (`padFocus` -2 no pad ring).
- `Job.here` true: no keys and no Esc - the windows shown at the start are checked (a scene's: NPC, shop, storage,
  trade, vending, chat room, opened by the helper).
- `enum class Do : uint8_t { Nothing, Keys, Check, PadStep, Esc, Record, Finish };`
  `struct Job { std::vector<std::string> only; size_t at = 0; /* phase, clocks */ Row row; };`
  `Do step(Job& j, const Seen& s, int openMs = 2000);`
- `struct Row { std::string stop, error; std::vector<std::string> opened; json::Value problems; bool inside = true,
  padMoves = false, padRing = false, closed = false; };` `json::Value rowJson(const Row& r);`

- [ ] Step 1: tests - `check_tour_stops_take_keys` (every stop's keys pass mcp::keyCombo), `check_tour_appeared`,
  `check_tour_inside`, `check_tour_steps_a_window` (Keys → (a doc appears) → Check → PadStep → Esc → (gone) →
  Record), `check_tour_nothing_opened_presses_no_esc` (Review Focus 5), `check_tour_here_checks_what_is_open`.
- [ ] Steps 2-5 as Task 1; commit `3.0.24-beta.57: core/check_tour - the windows' tour, step by step`.

### Task 3: the MCP's tools and the settings' «live»

**Files:** Modify `src/core/mcp.cpp` (table, argument checks), `src/core/mcp_game.cpp` (`settings`: `"live":
appliesNow(key)`, `"skip": skipReason(key)`); Test `tests/test_mcp*.cpp`.

- `probe`: `{"action": start|status|stop|restart|verify, "prefix": "effects.", "keys": [..] (≤ 700), "photo": bool}`.
  start: the live keys under `prefix`/`keys`; restart: the restart-only ones written at once + the plan file; verify:
  after a restart, checks the plan (then writes the values back, `back` next time).
- `tour`: `{"action": start|here|status|stop, "only": [stop names]}`.
- [ ] Tests `mcp_probe_args` (no action → refused; keys over 700 → refused), `mcp_tour_args`, `mcp_settings_list`
  (live and skip present) - FAIL, then implement, PASS; commit.

### Task 4: win - the jobs in the frame

**Files:** Create `src/win/context_check.cpp`; Modify `src/win/context.hpp` (members), `src/win/context.cpp` (the
frame: `checkFrame()` after `mcpFrame(true)`; the shot pick: `if (screenshot.empty()) screenshot =
checkShotWanted();`), `src/win/context_mcp.cpp` (probe/tour → `checkCall`), `src/win/ui_sheet.{hpp,cpp}`
(`int navFocus(int kind)` - -2 none), `src/win/uaui.{hpp,cpp}` (`std::vector<std::string> shownDocs()`,
`uint64_t wordsPrint(const std::string& doc)` - a hash of check(doc, all)).

- Write = settingchange::edits + editIniFile, then `settingsKnown_ = false; settingsChecked_ = 0;` (read next frame).
- Seen.value = settings::tokenOf(*findSetting(key), config); loading/serial from the renderer's last GpuTiming.
- ShotBefore/After: `uaro-graphics\check-frame.bgra` through the screenshot pick, read by shotspec::readRaw; the
  failed rows' frames saved halved as `logs\check-<key>-before|after.png` (shotspec::shrunk + writePng).
- uiChanged = docs whose wordsPrint moved; logErrors = new ERROR lines of graphics + render64 logs (tail from the
  size noted at ShotBefore).
- While a job runs: `mcpCalledMs_ = GetTickCount64()` each frame (drivenOverMcp); photo mode on when asked.
- The tour: Keys via mcpinput::key(Game, keyCombo(stop.keys)), Check = uaui::check(doc, false) + the box inside the
  client area, PadStep = uisheet::padNav(kind, Down) when navFocus(kind) ≥ -1, Esc via mcpinput::key.
- [ ] Build win32 + win64 (warnings fatal), host tests; commit `3.0.24-beta.57: probe and tour in the frame`.

### Task 5: the watchdog's ini copy and pictures

**Files:** Modify `src/core/mcp_watch.cpp` (`ini` action backup|restore; `files` kinds + png), `src/app/mcp_watch.cpp`
(backup = copy to `uaro-graphics.ini.check`, restore = copy back); Test `tests/test_mcp_watch*.cpp`
(`mcp_watch_catalog`: the new actions; a png name passes, `..\x.png` refused).

### Task 6: loki - fullcheck.py

**Files:** Create `/root/uaro-video/tools/fullcheck.py`, `/root/uaro-video/tools/fullcheck_scenes.json`.

- `--pc laptop|desktop` → game 8770/8780, watchdog 8771/8781. Stages `prep a restart b c d3d11 risky report`, each
  resumable: prep (ini backup, Prontera, @hide, clock and weather held), a (`probe start` all live keys, photo off),
  restart (restart → watchdog game restart → verify → restart → verify back), b (each scene: warp, conditions,
  effects.preset off + groups on, photo on, `probe start` its keys with parents set first), c (`tour start`, then
  the light look and 150 %), d3d11 (renderer.api d3d11 via watchdog ini + restart, restart and b again), risky
  (client.*, renderer.* one by one, a watchdog restore if the game does not come back in 180 s), swap (Esc menu
  «Змінити акаунт», the other account's card on the login shelf by its box from `windows`; the merchant's password
  typed once at run time, kept by the shelf, never by the helper), pair (both PCs' characters met in Prontera: the
  merchant's cart and shop, a trade, a party, a chat room - each window checked by `tour here`), report (fetch the
  check files and the failed rows' pictures, one table per PC).
- Scenes = the effects checklist's spots (artifact KePViKFLiFfgLcLGD4GCcH) for the keys of fx_pass2.json; the rest
  of `effects.*` in Prontera by day.
- [ ] Dry run against the laptop when its tunnel is up (`--stage prep`), else `python3 -m py_compile`.

### Task 7: ship 3.0.24-beta.57

- [ ] core/checks_now entry (probe/tour); notes `/root/uagfx-tools/work/notes/3.0.24-beta.57.{uk,en,ru}.md`;
  `bash /root/uagfx-tools/work/gship.sh 3.0.24-beta.57 "..."`; memory project-full-check-plan updated.
