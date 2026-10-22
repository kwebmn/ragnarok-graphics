# Beta Phase 4: Sets Itself Up Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> The user works inline («делай все фазы без остановки», «в инлайн режиме»): executing-plans, no subagents; the final
> review is a self-review.

**Goal:** a clean install needs no press before «Грати»: the settings are picked for the card at the app's first start
(integrated graphics told apart), asked again when the card or the screen changed; a game that failed to start twice
starts in a safe mode; «Безпечний режим» and «Вимкнути uaRO Graphics» are one press; the start splash speaks the
player's language.

**Architecture:** Core decides everything testable: the tiers (`core/settings/auto_setup`: two integrated tiers), the
first-start decision and the machine's key, the safe mode (a failed start = a bad end before the first picture, two in
a row → Direct3D 11 without the risky parts), `Api::System` (the game's own look), the splash's texts. The driver logs
the first picture, counts failed starts in `uaro-graphics\cache\failed-starts.txt` and leaves the reason in
`uaro-graphics\cache\safe-mode.txt` for the app; the app applies the auto setup at its first start and shows cards
on «Гра».

**Tech Stack:** C++17, DXGI (SharedSystemMemory), the existing auto setup, strings.cpp.

**Spec:** `docs/superpowers/specs/2026-10-22-beta-release-design.md` (phase 4), the page QjTrLqpnyyw8qKiUZQ3i24
(«Автонастройка под видеокарту»: its tiers table, «Страховка в самой игре»).

## Global Constraints

- Branch `beta/phase-4` from main, fast-forwarded at the end; gate after every task; releases only on the user's word.
- A player's own settings are never overwritten unasked: the silent auto setup only for an install the app never
  ran in (`usedBefore` false); a changed card or screen asks.
- The safe mode changes the settings for that start only (in memory), never the ini; the app explains it.
- `api = system` keeps the pad, the sound and the launcher: only the drawing is the game's own.
- The tiers' numbers are the page's starting ones (to be tuned by the closed round).
- Texts uk/ru/en in strings.cpp.

## Review Focus

1. A 2.x player with tuned settings opens the new version: nothing of theirs changes (the machine is recorded, no
   auto setup) - Task 4 `first_start_leaves_a_used_install_alone`.
2. A crash two hours into a game is no failed start: no safe mode next time - Task 3
   `a_crash_after_the_first_picture_is_no_failed_start`.
3. A laptop with Intel UHD and an NVIDIA card (Optimus): the auto setup looks at the card the game draws on (the probe's
   main adapter), not the integrated one - Task 1 `auto_setup_takes_the_fast_card_of_a_laptop` (the integrated flag
   comes from the main adapter's memory).
4. Safe mode on a computer without Direct3D 11: the game's own look, not a crash - Task 3
   `safe_mode_without_direct3d_11_is_the_games_own_look`.
5. The splash on a Windows in another language (German): English - Task 5 `the_splash_speaks_the_players_language`.

---

### Task 1: Integrated graphics and their tiers

**Files:** Modify `src/core/settings/auto_setup.hpp/.cpp`, `tests/test_auto_setup.cpp`, `src/win/system_probe.hpp/
.cpp` (sharedMb), `src/app/model.cpp` (machine()), strings (AutoTierIntegratedWeak, AutoTierIntegratedStrong).

**Interfaces:** `Machine::integrated`, `Machine::battery`; `Tier::IntegratedWeak`, `Tier::IntegratedStrong`;
`bool integratedByMemory(unsigned long long dedicatedMb, unsigned long long sharedMb);` (dedicated ≤ 1024 MB and less
than shared); `bool strongIntegrated(const std::string& adapter);` ("Iris Xe", "Arc(TM) Graphics" / "Arc Graphics"
with no "A" model number, "680M", "760M", "780M", "880M", "890M", "8050S", "8060S").
Weak: `quality.super_light = 1`, effects preset off, msaa 0, `frames.fps_limit = 60`, `frames.battery_saver = 1` with
a battery, AI sprites only at ×2, no DLSS. Strong: Speed, effects weak, msaa 2, keep_fps 45, AI ×2.
- [ ] **Step 1: Write the failing tests** `auto_setup_weak_integrated_is_super_light` (Intel UHD 620, dedicated 128
  MB, shared 8 GB), `auto_setup_strong_integrated_is_low_with_msaa_2` (Iris Xe), `integrated_by_memory_and_name`,
  `auto_setup_takes_the_fast_card_of_a_laptop` (RTX 3060 Laptop 6 GB: Middle, not integrated).
- [ ] **Step 2-4**: Run (FAIL), implement, run (PASS).
- [ ] **Step 5: Commit** `feat(settings): integrated graphics told apart - super light for the weak, low with MSAA 2x for the strong`.

### Task 2: `api = auto` shipped, `api = system` to switch it off

**Files:** Modify `src/core/config.hpp/.cpp`, `src/core/renderer_choice.cpp`, `src/core/settings/schema.cpp`,
strings, `dist/uaro-graphics.ini`, tests (`test_renderer_choice.cpp`, `test_config.cpp`, `test_schema.cpp`).

**Interfaces:** `Api::System` (token "system"): `chooseRenderer` → `Renderer::System`, reason "api = system: uaRO
Graphics switched off - the game's own look"; the template's `[renderer] api = auto`.
- [ ] **Step 1: Write the failing tests** `api_system_is_the_games_own_look`, `the_shipped_settings_ask_for_auto`.
- [ ] **Step 2-4**: Run, implement, run.
- [ ] **Step 5: Commit** `feat: api = auto shipped; api = system switches uaRO Graphics off`.

### Task 3: The safe mode

**Files:** Create `src/core/safe_mode.hpp/.cpp`, `tests/test_safe_mode.cpp`; Modify `src/core/config.*`
(`renderer.safe_mode`), schema, strings, `src/win/real_ddraw.cpp`, the present path (the first picture's log line).

**Interfaces:**
```cpp
namespace uagfx::safe {
constexpr const char* kFirstPicture = "renderer: the first picture is on the screen";
bool startFailed(const std::string& previousLog);  // a bad end (report::reasonOfLog) before kFirstPicture
int failedInRow(int before, bool lastFailed);
bool needed(int failedInRow, bool asked);           // two in a row, or [renderer] safe_mode = 1
void apply(Config& c);  // Direct3D 11 (api d3d11; System when it was), no bridge, AI, effects, DLSS, frame
                        // generation, HDR, Remix
}
```
- [ ] **Step 1: Write the failing tests** `safe_mode_after_two_failed_starts`,
  `a_crash_after_the_first_picture_is_no_failed_start`, `safe_config_turns_the_risky_parts_off`,
  `safe_mode_without_direct3d_11_is_the_games_own_look` (chooseRenderer after apply on a computer without D3D11 →
  System).
- [ ] **Step 2-4**: Run, implement (the driver: after the log opens, the .prev log judged, the counter file kept, the
  config made safe, `safe-mode.txt` written with the reason or removed), run.
- [ ] **Step 5: Commit** `feat: a safe mode after two failed starts, and [renderer] safe_mode`.

### Task 4: The first start sets itself up; the app's cards and buttons

**Files:** Modify `src/core/settings/auto_setup.*` (machine key, first-start decision), `src/app/model.*`
(applyAutoSetup), `src/app/main_window.cpp`, `src/app/page_play.cpp`, `src/app/page_service.cpp`,
`src/app/page_settings.cpp` (runAutoSetup through the model), strings, `tests/test_auto_setup.cpp`,
`tests/test_model.cpp`.

**Interfaces:** `std::string machineKey(const Machine& m);` ("adapter|vramMb|WxH");
`enum class FirstStart { Nothing, Apply, Ask, Record }; FirstStart firstStart(bool usedBefore, const std::string&
kept, const std::string& now);` (`[utility] machine`); `std::wstring Model::applyAutoSetup(Lang l, AutoSetup& done);`
(the edits, the preset, the whole-number interface; applied; `utility.machine` written).
«Гра»: the card «Підібрано для {adapter}» after a silent setup (with «Докладніше» - the auto setup's text), the
safe-mode card (its reason, «Звичайний режим» - the counter and the file gone, `safe_mode = 0`); Service: «Якщо гра не
запускається» - «Безпечний режим» (`safe_mode = 1`), «Вимкнути uaRO Graphics» / «Увімкнути uaRO Graphics» (api
system / auto).
- [ ] **Step 1: Write the failing tests** `first_start_decides_by_the_install_and_the_machine` (table),
  `first_start_leaves_a_used_install_alone` (model: a 2.x ini with the language → the machine recorded, nothing else
  changed), `model_sets_a_fresh_install_up` (no ini → the tier's edits written, the machine recorded).
- [ ] **Step 2-4**: Run, implement, run; the gate (texts used).
- [ ] **Step 5: Commit** `feat(app): the first start sets itself up; safe mode and switching off in one press`.

### Task 5: The splash in the player's language

**Files:** Modify `src/core/start_screen.hpp/.cpp`, `src/win/start_screen.cpp` (UTF-8 drawn), `src/win/real_ddraw.cpp`
(the language), strings, `tests/test_start_screen.cpp`.

**Interfaces:** `std::string line(const Progress& p, settings::Lang l); std::string hint(const Progress& p,
settings::Lang l);` - texts StartRenderer, StartDlss, StartShadersLoad, StartShadersBuild ("{1} of {2}"),
StartAiTextures, StartFirstHint; the driver: `utility.language` of the ini, else Windows' (settings::windowLang).
- [ ] **Step 1: Write the failing test** `the_splash_speaks_the_players_language` (uk, ru, en lines; German → en).
- [ ] **Step 2-4**: Run, implement, run.
- [ ] **Step 5: Commit** `feat: the start splash in the player's language`.

### Task 6: Notes and checks

- [ ] `$S/notes/2.12.0.{ru,uk,en}.md`: integrated graphics' tiers, the first start's setup and its card, a changed card
  asks, the safe mode and its card, «Безпечний режим», «Вимкнути uaRO Graphics», api = auto, the splash's language;
  checks: a clean install → «Грати» with no other press; Intel UHD → «Надлегка»; two forced failed starts → safe mode
  and its card; «Звичайний режим»; «Вимкнути uaRO Graphics» → the game's own look, the pad still works.
- [ ] Gate, self-review against the Review Focus, main fast-forwarded.
