# Beta Phase 1b: The Gamepad Inside uaRO Graphics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> The user works inline («все в инлайн режиме»): executing-plans, no parallel agents; the final review is a
> self-review.

**Goal:** uaRO Pad stops being a separate `dinput.dll`: its code lives in this repository, our `ddraw.dll` mounts it
(on by default), its files live in `uaro-graphics\`, and an installed old pad is removed.

**Architecture:** The pad's portable core (`/root/uaro-gamepad/src/core`, namespace `uapad`, 294 host tests) moves to
`src/pad/core` as the static library `uagfx_pad`; its Windows part moves to `src/pad/win` and is compiled into
`ddraw.dll`. Instead of being the game's `dinput.dll`, the pad is mounted by our `DllMain`: it patches uaRO.exe's
import slot `DINPUT.dll!DirectInputCreateA` and wraps what the system's DirectInput returns, as its proxy export did.
Its files join `core/layout` (moved by `core/layout_migration`).

**Tech Stack:** C++17, MinGW-w64 i686, Win32 (DirectInput 7 wrappers, XInput and HID loaded at run time), the test
runner `tests/test.hpp` (the pad's is the same).

**Spec:** `docs/superpowers/specs/2026-10-22-beta-release-design.md`; the user, 2026-10-22: «геймпад надо прямо в
uaro-graphics монтировать, функционал сам», «уаропад должен быть включен по умолчанию».

## Global Constraints

- As phase 1: work on main, gate after every task (`bash $S/gate.sh` → `GATE OK`), release only on the user's word
  (`bash $S/ship.sh 2.9.0 "Выпуск 2.9.0"`), comments in English, lines ≤ 120, commits by KWEB Team with the trailer.
- `ddraw.dll` imports only kernel32, user32, gdi32, msvcrt: the pad's `hid.dll` and `setupapi.dll` calls are loaded at
  run time from the system folder; `ws2_32` and XInput already are.
- The pad is on by default: `[pad] enabled = 1` in uaro-graphics.ini (the driver mounts it); `0` mounts nothing. Its
  own `[general] enabled` (the in-game switch, Back) stays as it is.
- An old pad `dinput.dll` in the game's folder (its bytes hold `uaro-pad.log`) loaded by the game: the driver does not
  mount a second pad; the settings app and install.bat remove that file while the game is closed and put back
  `dinput.dll.before-pad`.
- `/root/uaro-gamepad` is only read; its repository, its site page and its forum thread are not touched here.

## Review Focus

1. A player without a gamepad (most players, the pad now on by default): nothing changes for keyboard and mouse - the
   pad's start must not fail the game; a pad failure leaves pass-through - Task 3 keeps the pad's `guarded`/`healthy`
   pass-through; the tester checks a game with no controller.
2. A player who had switched the pad on in 2.x (root `dinput.dll` is the pad): no double pad, the old file removed
   when the game is closed - Task 4 `model_removes_an_old_pad_dll_and_puts_back_what_it_moved`.
3. The pad's settings and layouts of 2.x (`uaro-pad.ini`, `uaro-pad-binds.ini` in the root) keep working - Task 2
   `migration_moves_the_pads_files`.
4. A game started with `[pad] enabled = 0`: the import slot untouched - Task 3 (`mountPad` returns early; manual).
5. Sony pads (HID loaded at run time): a missing `hid.dll` export leaves Sony pads unseen, XInput pads still work -
   Task 3 (`HidApi::load` false → the Sony probe skipped).

---

### Task 1: The pad's core and its tests in this repository

**Files:**
- Create: `src/pad/core/**` (copied from `/root/uaro-gamepad/src/core/**` at e59889d), `tests/test_pad_*.cpp` (each
  of its 22 test files renamed `test_pad_<name>.cpp`), `tests/pad_fixtures.hpp`, `tests/pad_test_fixtures.hpp` (its
  `fixtures.hpp`)
- Modify: `CMakeLists.txt` (library `uagfx_pad` from `src/pad/core/*.cpp`, linked into `uagfx_tests`)

**Interfaces:**
- Produces: the library `uagfx_pad` (namespace `uapad`), includes `pad/core/...`; `uapad::version()` returns
  `uagfx::kVersion`.

- [ ] **Step 1:** copy; rewrite `#include "core/` → `#include "pad/core/` in the copied sources and tests, and
  `fixtures.hpp` → `pad_test_fixtures.hpp`; no `test_main.cpp` (ours runs every registered test).
- [ ] **Step 2: Run the tests**

Run: `cmake -S . -B build-host >/dev/null && cmake --build build-host -j8 2>&1 | grep -E "error|warning" | head`
then `./build-host/uagfx_tests | tail -1`
Expected: no error or warning; `1258 tests, 0 failed checks` (964 + the pad's 294).
- [ ] **Step 3: Gate and commit** `feat(pad): uaRO Pad's core and its 294 tests in this repository (from
  uaro-gamepad e59889d)`.

### Task 2: The pad's files in core/layout

**Files:**
- Modify: `src/core/layout.hpp/.cpp`, `tests/test_layout.cpp`, `tests/test_layout_migration.cpp`

**Interfaces:**
- Produces: `layout::File::PadIni` (`uaro-pad.ini` → `uaro-graphics\uaro-pad.ini`), `File::PadBinds`
  (`uaro-pad-binds.ini` → `uaro-graphics\uaro-pad-binds.ini`), `File::PadLog` (`uaro-pad.log` →
  `uaro-graphics\logs\uaro-pad.log`); `kFileCount = 17`.

- [ ] **Step 1: Failing tests**

```cpp
TEST(layout_the_pads_files_live_in_uaro_graphics) {
    CHECK_EQ(std::string(layout::now(layout::File::PadIni)), std::string("uaro-graphics\\uaro-pad.ini"));
    CHECK_EQ(std::string(layout::before(layout::File::PadBinds)), std::string("uaro-pad-binds.ini"));
    CHECK_EQ(std::string(layout::now(layout::File::PadLog)), std::string("uaro-graphics\\logs\\uaro-pad.log"));
}

TEST(migration_moves_the_pads_files) {
    // root uaro-pad.ini "[pad]\r\ntype = sony\r\n", uaro-pad-binds.ini "b", uaro-pad.log "l" -> under uaro-graphics\
    // with their bytes, none left in the root; dinput.dll in the root untouched
}
```
- [ ] **Step 2:** run → compile error (no `File::PadIni`). **Step 3:** the three rows. **Step 4:** tests pass, gate.
- [ ] **Step 5: Commit** `feat(layout): the pad's settings, layouts and log in uaro-graphics\`.

### Task 3: The driver mounts the pad

**Files:**
- Create: `src/pad/win/**` (from `/root/uaro-gamepad/src/win/**` without `dllmain.cpp` and `dinput.def`),
  `src/pad/win/mount.hpp`, `src/pad/win/mount.cpp`
- Modify: `src/pad/win/runtime.cpp` (paths), `src/pad/win/sony_pad.cpp` (HID at run time), `src/win/exports.cpp`
  (`DllMain`), `src/core/config.hpp/.cpp` (`[pad] enabled`), `CMakeLists.txt` (the pad's Windows sources into
  `ddraw`, `uagfx_pad` linked)
- Test: `tests/test_config.cpp`

**Interfaces:**
- Produces: `bool uapad::mount(const std::string& gameDir);` - patches uaRO.exe's `DINPUT.dll!DirectInputCreateA`
  slot with `uapad::proxyDirectInputCreateA` (the old export's body: the original from the slot, `runtime::init`,
  `wrapDirectInput`); false when the slot is missing or the old pad dll is the game's `dinput.dll`.
  `Config::padEnabled` (`[pad] enabled`, default true).

- [ ] **Step 1: Failing test**

```cpp
TEST(config_the_pad_is_on_by_default) {
    CHECK(parseConfig("").padEnabled);
    CHECK(!parseConfig("[pad]\nenabled = 0\n").padEnabled);
}
```
- [ ] **Step 2:** run → compile error (no `padEnabled`). **Step 3:** the key, then the pad's Windows sources (includes
  `win/` → `pad/win/`), `mount`, `DllMain` calling it after `installSoundHooks` when the ini's `[pad] enabled` is 1;
  `runtime::init` reads `win::ourFile(dir, File::PadIni)`, `File::PadBinds`, logs into `now(File::PadLog)`;
  `sony_pad.cpp` takes `HidD_*`, `HidP_GetCaps`, `SetupDi*` from `LoadLibraryA(<system>\hid.dll / setupapi.dll)`.
- [ ] **Step 4:** tests pass; `bash $S/gate.sh` → `GATE OK` with `check_dll.py` still printing only
  `gdi32.dll kernel32.dll msvcrt.dll user32.dll`.
- [ ] **Step 5: Commit** `feat(pad): the driver mounts uaRO Pad itself - no dinput.dll in the game's folder, on by
  default`.

### Task 4: The settings app: the pad's switch and the old pad's removal

**Files:**
- Modify: `src/core/settings/schema.cpp` (row `pad.enabled` on the Pad page), `src/app/page_main.cpp`,
  `src/app/page_pad.cpp`, `src/app/model.cpp`, `src/app/report_send.cpp` (attach `uaro-pad.log`)
- Test: `tests/test_model.cpp`

**Interfaces:**
- Consumes: `settings::classifyPad`, Task 2's layout, Task 3's `pad.enabled`.

- [ ] **Step 1: Failing test**

```cpp
TEST(model_removes_an_old_pad_dll_and_puts_back_what_it_moved) {
    // root dinput.dll "MZ ... uaro-pad.log ..." and dinput.dll.before-pad "ROExt"; the game not running ->
    // after app::Model: dinput.dll is "ROExt", no dinput.dll.before-pad, no uaro-pad.moved
}
```
- [ ] **Step 2:** run → fails. **Step 3:** `Model::Model()` removes it (the game not running); the Main page's
  «Геймпад» and the Pad page switch `pad.enabled`; the Pad page edits `ourFile(File::PadIni)` (from
  `uaro-graphics\defaults\uaro-pad.ini` when missing); its version is the driver's; no dll install or removal there.
- [ ] **Step 4:** tests pass, gate. **Step 5: Commit** `feat(app): the pad switched in uaro-graphics.ini, an old pad
  dll removed`.

### Task 5: Package and scripts

**Files:**
- Modify: `tools/package.sh`, `tools/check_package.py`, `dist/install.bat`

- [ ] **Step 1:** `check_package.py`: no `uaro-graphics/pad/*`, `uaro-graphics/defaults/uaro-pad.ini` present;
  install.bat holds `del /f "dinput.dll"` under the `uaro-pad.log` check. Run on the 2.8.0 archive → FAIL.
- [ ] **Step 2:** `package.sh` (template to `defaults`, no pad dll), `install.bat` (an old pad dll deleted,
  `dinput.dll.before-pad` moved back, `uaro-pad.moved` deleted); `bash tools/package.sh` → OK.
- [ ] **Step 3:** gate; commit `feat(package): no separate pad - its template in uaro-graphics\defaults`.

### Task 6: Notes and checks

- [ ] `$S/notes/2.9.0.{ru,uk,en}.md`: the pad inside uaRO Graphics, on by default; what to check: a game with a
  gamepad (walk, skills, NPC, the menu, Sony light bar), a game without one (mouse and keys as before), an old pad
  dll removed after the update, the pad's settings and layouts kept, «Геймпад» switch off → no pad.
- [ ] Gate, self-review against the Review Focus, then tell the user it is ready to ship as 2.9.0.
