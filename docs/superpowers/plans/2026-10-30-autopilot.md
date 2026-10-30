# Autopilot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** a fresh install (and an untouched one) runs smooth 60 frames on any card, survives early crashes, asks
nothing before the first game, and tells the site how its first game went.

**Architecture:** the driver's card judge (`core/card_speed`) walks the whole effects ladder 0..3 for the `auto` set and
remembers its verdict per card; the automatic setup writes `auto` and keep_fps 60; a one-time migration moves untouched
installs over; the safe mode also counts bad ends in the first 5 minutes; the app sets up a copied install silently and
asks its questions after a game; the first game's outcome rides the anonymous install ping.

**Tech Stack:** C++20 (mingw i686 driver, x86-64 app/renderer, host tests on Linux), Laravel 13 site (sandbox, owner
deploys).

**Spec:** `docs/superpowers/specs/2026-10-30-autopilot-design.md`

## Global Constraints

- Branch `beta`; each phase shipped by `$S/ship-beta.sh 3.0.11-beta.N "msg"` (gate inside); never `main`.
- New texts in `core/settings/strings.cpp` in en, uk, ru; the uk words are the ones the owner reads.
- The target: 60 frames a second (the fps limit's when lower), the card's own work - `effect_costs::slowForCard`.
- Set names: level 0 `TestSetOff` «Без ефектів», 1 `FxPresetWeak` «Низькі», 2 `FxPresetMedium` «Середні», 3
  `FxPresetStrong` «Високі».
- Lines in src/tests at most 120 characters; comments in the code's own style (what, why, the version).
- The site's sandbox `$S/site_db/app`; nothing on the live site without the owner's command.

## Review Focus

1. A card judged in a town slow for the processor's part (fps under 54, the card's work small): no step (test in T1).
2. A record of another card (an upgrade): ignored, the memory level again (test in T1).
3. A log without time stamps (old builds, a cut log): the safe mode as before (test in T5).
4. A copied install with a driver log left by the packer: asked as before, not silently changed (test in T7).
5. A first game cut before any map (crash at the login): map 0, its end known, sent (test in T8).

---

## Phase 1 - 3.0.11-beta.58: the ladder

### Task 1: the card judge's ladder and record (`core/card_speed`)

**Files:** Modify `src/core/card_speed.hpp`, `src/core/card_speed.cpp`; Test `tests/test_card_speed.cpp` (rewrite).

**Interfaces - Produces:**
- `constexpr int kRaiseWindows = 12, kTrialWindows = 36, kBackWindows = 2, kLowerWindows = 6, kTopLevel = 3;`
- `bool hasRoom(int target, double gpuMs);` (unchanged)
- `enum class Step { None, Raise, Keep, Back, Lower };`
- `class Judge { explicit Judge(int level, bool settled = false); Step window(double fps, int target, double gpuMs,
  int slowTarget = 0); int level() const; bool trying() const; bool settled() const; };`
- `struct Record { int level = 0; bool settled = false; std::string adapter; };`
- `std::string format(const Record&)` -> `"v2 <level> <0|1> <adapter>\n"`; `bool parse(const std::string&, Record&)`
  (v2 levels 0..3; the old `"<1|2> <adapter>"` -> that level, settled false).
- `struct Start { int level = 0; bool settled = false; };`
  `Start startOf(int memoryLevel, const std::string& adapter, const std::string& text);` (the record's when its adapter
  is this one, else `{memoryLevel, false}`). `levelAt`/`decided` removed.

Rules: on trial - `kBackWindows` slow windows in a row (slowForCard at slowTarget or target) -> level-1, settled, Back;
`kTrialWindows` -> Keep, settled when at kTopLevel. Not on trial, not settled: `kLowerWindows` slow in a row and level
> 0 -> level-1, settled, Lower; `kRaiseWindows` with room in a row and level < 3 -> level+1 on trial, Raise. Settled:
None always.

- [ ] Tests: `card_speed_room_is_half_a_frame` (old cases); `card_speed_raises_any_level_below_the_top` (0->1, 2->3,
  never above 3); `card_speed_keeps_a_trial_and_settles_at_the_top` (36 windows: Keep; level 2 kept -> not settled, can
  raise; level 3 -> settled); `card_speed_backs_off_a_failed_trial` (2 slow -> Back, previous level, settled, then
  None); `card_speed_lowers_a_steady_slow_card_once_unsettled` (6 slow -> Lower, never below 0, settled -> no more);
  `card_speed_a_town_slow_for_the_processor_is_no_step` (fps 40, gpu 5 ms: None for 100 windows);
  `card_speed_trial_by_the_players_keep_fps` (slowTarget used); `card_speed_record_round_trip_and_old_format` (v2
  round trip, "2 AMD Radeon HD 6570" -> 2 not settled, "5 X", "v2 9 0 X", "" -> false);
  `card_speed_start_of_this_card_only` (matching adapter -> record; other adapter -> memory, not settled).
- [ ] Run `cmake --build build-host --target uagfx_tests -j8 && ./build-host/uagfx_tests card_speed` -> FAIL (missing).
- [ ] Implement; run -> PASS. Commit with Task 2-4 at the phase's ship.

### Task 2: the automatic setup's auto set and the migration (`core/settings/auto_setup`)

**Files:** Modify `src/core/settings/auto_setup.hpp/.cpp`; Test `tests/test_auto_setup.cpp`.

**Interfaces - Produces:** `std::vector<IniEdit> autopilotOnce(const std::map<std::string, std::string>& ini, int
memoryLevel);` - nothing once `utility.autopilot` is there; else `effects.preset` (absent = auto) equal to the memory
level's set (1 weak, 2 medium, 3 strong) -> `auto`; when the set is or becomes `auto` and `quality.super_light` is not
"1", `frames.keep_fps` absent, "0" or "45" -> "60"; always `utility.autopilot = 1`.

`autoSetup`: `effects.preset` = `auto` for Old, Low, Middle, High, IntegratedStrong (IntegratedWeak, NoDriver: `off`);
`frames.keep_fps` = 60 but IntegratedWeak and NoDriver 0.

- [ ] Tests: update `auto_setup_keeps_the_frame_rate_on_smaller_cards` (rtx5060 and old: "60"), the classes' preset
  checks ("auto"), `auto_setup_strong_integrated_is_low_with_msaa_2` (auto, 60); new
  `autopilot_once_moves_the_untouched_sets` (weak/45 at level 1 -> auto/60/mark; strong/0 at 3 -> auto/60; medium at 3
  -> mark only; custom -> mark only; off + super_light 1 -> mark only; absent preset + absent keep -> keep 60 + mark;
  marked -> nothing).
- [ ] FAIL, implement, PASS.

### Task 3: the driver (`win/real_ddraw.cpp`, `win/context.*`, strings)

**Consumes:** Task 1 (`startOf`, `Judge`, `Step`, `format`), Task 2 (`autopilotOnce`).

- `Startup::cardDecided` -> `cardSettled` (`startOf`); after the memory level: `autopilotOnce(readIni(ini),
  memoryLevel)` written (`editIniFile`) and applied to `s.config` (preset Auto, keepFps 60), one log line.
- `Context::judge()`: `Judge(startup().cardLevel, startup().cardSettled)`.
- `Context::judgeCard`: Raise - log + note `CardRaisedNote` {1} the new set; Keep - record `{level, settled}`, log;
  Back - record, log, note `CardLoweredNote` {1} the failed set, {2} the set back; Lower - record, log, note
  `CardSlowNote` {1} the set before, {2} the set now. Any step: `keepSlow_ = 0`. Record path
  `uaro-graphics\card-level.txt` (as now).
- Strings: `CardRaisedNote` «Відеокарта встигає: набір ефектів «{1}» — на пробу», `CardLoweredNote` «Набір «{1}»
  заважкий для цієї відеокарти — повернуто «{2}»», new `CardSlowNote` «Відеокарта не встигає з набором «{1}» —
  тепер «{2}»» (en, ru alike).
- [ ] Build `cmake --build build-win -j8` without warnings.

### Task 4: the app (`app/model`, `page_effects`, `page_play`)

**Consumes:** Tasks 1-2.

- `Model::cardLevel()` via `startOf`; `Model` at open: `autopilotOnce` with the memory level (as worldScaleReset).
- `startSetup`: Apply or Ask -> delete `uaro-graphics\card-level.txt` (a new card or screen judged again).
- Effects page «Авто: {1}» names level 0 `TestSetOff`.
- «Гра» card while the set is `auto`: `AutopilotNote` «Ефекти під цю відеокарту: «{1}» · ціль — 60 кадрів» (Tone::Good)
  and button `AutopilotChange` «Змінити» -> `host_.showPage(Page::Effects)`.
- [ ] Build; gate; ship `3.0.11-beta.58` with notes.

## Phase 2 - 3.0.11-beta.59: the safe mode

### Task 5: early bad ends and the note's version (`core/safe_mode`)

**Files:** `src/core/safe_mode.hpp/.cpp`; Test `tests/test_safe_mode.cpp`.

**Produces:** `enum class Start { Unknown, Fine, Failed, FailedEarly };` `constexpr int kEarlySeconds = 300;`
`failedInRow` counts FailedEarly; `std::string noteText(int inRow, const std::string& version);` (first line `version
<V>`); `std::string noteVersion(const std::string& note);` ("" for an old note).

Rule: the first picture's line stamped and a bad end (`report::reasonOfLog`) stamped within kEarlySeconds after it
(past midnight: +86400) -> FailedEarly; the end's stamp - the crash's or the card-lost line, for a "not closed" end the
log's last stamped line; no stamps -> Fine as before.

- [ ] Tests: `safe_mode_a_crash_in_the_first_minutes_counts` (10:00:00 / 10:01:30 FailedEarly; 10:06:00 Fine; not
  closed with the last line 10:02:00 FailedEarly; 23:59:00 / 00:01:00 FailedEarly; no stamps Fine);
  `safe_mode_note_carries_its_version` (round trip; old note -> ""); keep the old tests.

### Task 6: the driver and the app's words

- `safeModeFirst`: a note of another version -> both files deleted, log «safe mode: version X now - the normal mode
  tried again»; the note written with `noteText(inRow, kVersion)`.
- `SafeModeAuto` words: «…останні запуски не дійшли до картинки або закрилися з помилкою в перші хвилини…».
- [ ] Gate; ship `3.0.11-beta.59`.

## Phase 3 - 3.0.11-beta.60: the app's first start

### Task 7: copied installs and the questions

**Files:** `src/core/settings/auto_setup.hpp/.cpp`, `src/app/model.*`, `src/app/main_window.cpp`; Test
`tests/test_auto_setup.cpp`.

**Produces:** `FirstStart firstStart(bool usedBefore, const std::string& kept, const std::string& now, bool
playedHere);` - kept non-empty, other than now, not played here -> Apply. `bool Model::playedHere() const` - the
driver's log there.

- [ ] Test `first_start_sets_up_a_copied_install_silently` (foreign key + not played -> Apply; + played -> Ask; same ->
  Nothing; empty -> Apply / Record as before).
- `kAskStats`: the two questions only when `playedHere()`.
- [ ] Gate; ship `3.0.11-beta.60`.

## Phase 4 - 3.0.11-beta.61: the first game's outcome

### Task 8: `core/first_game`

**Files:** Create `src/core/first_game.hpp/.cpp`; Test `tests/test_first_game.cpp`; layout `kFirstGame =
"uaro-graphics\\cache\\first-game.txt"`, `kFirstGameSent = "uaro-graphics\\cache\\first-game.sent"`.

**Produces:** `namespace uagfx::firstgame { struct Outcome { std::string version, cls; bool safe = false, map =
false; int fps = -1, level = -1; std::string end; int minutes = -1; }; std::string format(const Outcome&); bool
parse(const std::string&, Outcome&); bool complete(const Outcome&); std::string endOfLog(const std::string& log);
int minutesOfLog(const std::string& log); std::string json(const Outcome&); }` - `cls` "L<memory level> <api>[ safe]";
`endOfLog` reasonOfLog or "fine" ("" for an empty log); `json` `{"class":"L1 d3d11","safe":0,"map":1,"fps":58,
"level":1,"end":"fine","minutes":12}` (fps 0..999, minutes 0..99999, strings [a-z0-9 -]).

- [ ] Tests: `first_game_round_trip`, `first_game_end_and_minutes_of_a_log` (crash, fine, midnight, no stamps -1),
  `first_game_json_clamped`, `first_game_cut_before_a_map` (map 0, end crash, complete).

### Task 9: the driver writes it

- At the start (no file): `{version, cls, safe}`. In `keepFrameRate`, the first 12 windows on maps: their fps kept;
  at the 12th - median, `map = 1`, `level = cardLevel()` written (once).

### Task 10: the app sends it

- `ping::body(id, version, channel, first = "")` -> `,"first":{...}` when given; `pingInstall(dir, channel, bool
  firstNow = false)` - a complete unsent outcome rides along (and is sent even inside the 12 hours when `firstNow`);
  2xx -> `first-game.sent`.
- `watchGame`: an incomplete outcome makes it wait for the game; then `endOfLog`, `minutesOfLog`, written,
  `pingInstall(..., true)`.
- The ping's tip: «…і як пройшла перша гра: чи дійшла до карти, скільки кадрів, чи закрилася з помилкою».
- [ ] Test `install_ping` extended (body with and without `first`).

### Task 11: the site (sandbox; the owner deploys)

- Migration `2026_10_30_100002_add_first_game_to_graphics_installs`: `first_class` varchar(24), `first_safe`,
  `first_map` tinyint, `first_fps` smallint, `first_level` tinyint, `first_end` varchar(16), `first_minutes` int - all
  nullable.
- `GraphicsPingController@store`: `first` optional (validated as above), stored when `first_end` is null.
- Admin summary `installs.first`: total, reached a map, ended badly, by class (class, n, map, bad).
- `$S/site_db/deploy-first.sh` / `rollback-first.sh` (as deploy-ping.sh); dry run on a worktree copy.
- [ ] Gate; ship `3.0.11-beta.61`; give the owner the deploy command.
