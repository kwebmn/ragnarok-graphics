# Beta Phase 7: Nothing Extra Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> The user works inline («делай все фазы без остановки», «в инлайн режиме»): executing-plans, no subagents; the final
> review is a self-review.

**Goal:** a player sees only what works on their computer and is meant for them: no DLSS / frame generation / Reflex
rows without an NVIDIA card, no HDR rows without an HDR screen, the developer's tools only in the Developer mode
(whose button players do not see), «ё» and the apostrophe typed in the game's chat (the console on Ctrl + the key
left of 1), uaRO Studio's keys for the developer only, a line on the screen while Pause keeps the improvements off,
the names in «» of every text checked at the build, a package of about 8 MB (the AI models downloaded when used).

**Architecture:** core decides what is testable: `settings::fitsMachine(key, Machine)` (rows by hardware),
`Config::developer` (`[utility] mode = developer`), `console::toggles(ctrl, vk)`, the texts' names check
(`tools/check_text_names.py`, in the gate). The app hides rows and tools by them; the driver gates its keys and draws
the Pause line on the frame counter's overlay; the package leaves the models to the models' archive, which the
update brings when the AI textures are on.

**Tech Stack:** C++17, Python 3 (the tools), the existing gate.

**Spec:** `docs/superpowers/specs/2026-10-22-beta-release-design.md` (phase 7), the page QjTrLqpnyyw8qKiUZQ3i24 («Без
лишнего»; the audit: «Строки не зависят от железа», «Наши инструменты видны игрокам», «Консоль забирает «ё» и
апостроф»).

## Global Constraints

- Rows hidden by hardware keep their values and their keys in the ini (as the modes' hidden rows do).
- The Developer mode: its button shows only while it is the mode, or after Ctrl+Shift+D in the app.
- Texts uk/ru/en; «» names are the Ukrainian interface's (feedback: quote switch names from strings.cpp's uk column).
- The package: no `uaro-graphics/ai/*.bin`; an installed computer keeps the models it has (nothing deletes them).
- Releases only on the user's word; gate after every task.

## Review Focus

1. An NVIDIA laptop that draws on Intel (Optimus) - DLSS rows follow the card the game draws on (Model::machine's
   card, the Render page's choice) - Task 1 `rows_follow_the_card_the_game_draws_on`.
2. A player with `[quality] dlss = quality` who moves to an AMD card: the row hides, the value stays, the driver
   ignores DLSS there as before - Task 1 (the value untouched).
3. Ctrl + the key left of 1 typed in the chat by a player who wants «ё» with Ctrl: the console opens - accepted.
4. An update to 2.15 on a computer with AI textures on: the models are not lost (no removal), a fresh install with AI on
   downloads them - Task 5 `the_update_brings_the_models_when_ai_is_on`.
5. A text that names a button which was renamed later: the build fails - Task 4.

---

### Task 1: Rows by hardware; the developer's tools

**Files:** Modify `src/core/settings/navigation.hpp/.cpp` (`bool fitsMachine(const std::string& sectionKey, const
Machine& m)`), `tests/test_navigation.cpp` (or the existing navigation test file), `src/app/page_settings.cpp`
(listed + buildRow), `src/core/settings/search.*` (a predicate for the hits), `src/app/page_main.cpp` («Зробити
пакет» developer-only), `src/app/page_pack.cpp` (the making section developer-only), `src/app/page_ai.cpp`
(«Створювати нові під час гри» developer-only), `src/app/main_window.cpp` (the Developer mode button; Ctrl+Shift+D),
strings (RenderBridge's label without «перевірка»).
- NVIDIA only (vendor 0x10de): `quality.dlss`, `quality.dlss_scale`, `quality.dlss_width`, `quality.dlss_height`,
  `quality.dlss_model`, `quality.dlss_jitter`, `quality.dlss_motion`, `frames.frame_generation`, `frames.reflex`.
- An HDR screen only (`Machine::hdr`): `display.hdr`.
- [ ] Tests `rows_hide_what_the_card_cannot_do`, `rows_follow_the_card_the_game_draws_on` → RED → implement → GREEN.

### Task 2: The driver's keys and the Pause line

**Files:** Modify `src/core/config.hpp/.cpp` (`developer`), `src/win/console.cpp` (Ctrl), `src/win/context.cpp`
(Studio keys need `developer`; the Pause line), `src/win/fps_overlay.*` (`setNote`), `src/win/real_ddraw.*` (the
start's language kept in Startup), strings (`CompareOffNote`), tests (`test_config.cpp`, a core console predicate).
**Interfaces:** `namespace uagfx::console { bool toggles(bool ctrl, unsigned vk); }` (core/console_key.hpp).
- [ ] Tests `developer_mode_comes_from_the_apps_mode`, `the_console_wants_ctrl_with_the_key_left_of_1` → RED →
  implement → GREEN.

### Task 3: (folded into Task 1 and Task 2)

### Task 4: The names in «» checked at the build

**Files:** Create `tools/check_text_names.py`, `tools/test_check_text_names.py`; Modify `$S/gate.sh` (the check),
`tools/package.sh` (the check), the texts it finds wrong.
- Every «…» in a uk text names another uk text (its value without "&" and a trailing "…"/":"), or a name on the
  allow list (game and Windows names: «Знімки екрана», «Параметри» …, the site's tabs «Звіти графіки», «Оновлення
  клієнта» …); ru and en texts quote the uk names too (the interface the user runs).
- [ ] Tests (python) `a_known_name_passes`, `an_unknown_name_fails` → RED → implement → GREEN; the gate.

### Task 5: The package without the AI models

**Files:** Modify `tools/package.sh`, `tools/check_package.py`, `tools/models_package.py` (all models in the
models' archive; version `models-2 1`), `dist/models-version.txt`, `release/uaro-graphics-models.zip` (rebuilt from
the current one and dist/ai), `src/app/update_run.cpp` (the models when `[ai] enabled = 1` and their version differs
- after DLSS), `src/win/gpu/ai_textures.cpp` (the message names the settings app), tests (`test_update_run.cpp`).
- [ ] Test `the_update_brings_the_models_when_ai_is_on` (and not when off) → RED → implement → GREEN; package.sh
  builds a zip under 10 MB.

### Task 6: Notes and checks

- [ ] `$S/notes/2.15.0.{ru,uk,en}.md`; gate; self-review; main fast-forwarded.
