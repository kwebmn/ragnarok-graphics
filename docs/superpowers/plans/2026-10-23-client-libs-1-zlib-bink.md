# Client Libraries, Step 1: cps.dll and binkw32.dll Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> The user works inline («приступай без остановки в инлайн режиме»): executing-plans, no subagents; the final
> review is a self-review.

**Goal:** our cps.dll (zlib's compress/uncompress over miniz) and a binkw32.dll stub in the game's root, put in by
uaRO Graphics with the game's originals kept aside, each switchable - 3.0.1-beta.5 on «Бета».

**Architecture:** core/client_libs knows the libraries (file, ini key, the marker) and decides each one's step for
the game's folder; src/clientlibs builds the two 32-bit DLLs; app/installer syncs the root (install, update, app
start, after Apply) and puts the originals back on removal; the Service page shows the switches.

**Tech Stack:** C++17, MinGW i686, miniz.

**Spec:** `docs/superpowers/specs/2026-10-23-own-client-libraries-design.md`.

## Global Constraints

- Exports exactly as uaRO.exe takes them: cps.dll `compress`, `uncompress` (cdecl, ordinals 1, 2); binkw32.dll the 11
  `_Bink*@N` names (stdcall). Imports: Windows' own DLLs and msvcrt only. Each holds the marker "uaRO Graphics".
- Package: `uaro-graphics\clientlibs\<name>`; originals: `uaro-graphics\original\<name>`; keys `[client] cps`,
  `bink` (1 by default, Detailed mode, not live: applied with the game closed).
- The root never lacks a library: off without a kept original leaves ours; the game running: nothing moves.

## Review Focus

1. A 3.0.1-beta.4 install updated: the game's cps.dll goes to original\, ours in the root - Task 2
   `the_update_puts_our_libraries_and_keeps_the_games`.
2. The player turns «cps.dll» off: the original back; on again: ours - Task 2 `a_library_switched_off_is_the_games`.
3. Removal: the originals back, ours gone - Task 2 `uninstall_puts_the_games_libraries_back`.
4. A root library already ours from an older package: replaced, the original kept as it was - Task 2.
5. The game running (the file locked): nothing moves, the next sync does it - Task 2 `a_locked_library_waits`.

### Task 1: the two DLLs and their check
Files: `src/clientlibs/cps.cpp`, `cps.def`, `binkw32.cpp`, `binkw32.def`, `CMakeLists.txt`, `tools/check_client_libs.py`
(exports, imports, marker, 32-bit), `tools/package.sh`, `tools/check_package.py`, `$S/gate.sh`.
- [ ] Build; the checker passes on both; the original cps.dll fails its marker check (the checker's own test).

### Task 2: core/client_libs + the installer's sync + the switches
Files: `src/core/client_libs.hpp/.cpp`, `src/core/layout.*` (kClientLibs, kOriginals), `src/app/installer.*`
(`syncClientLibs`), `src/app/main_window.cpp` (at the start and after Apply), schema/strings/template, the Service
page; tests `tests/test_client_libs.cpp`, `tests/test_installer.cpp`.
- [ ] Failing tests (Review Focus 1-5) → implement → pass; gate.

### Task 3: notes, review, beta; the build's handoff
- [ ] `$S/notes/3.0.1-beta.5.*`; self-review; ship-beta; the build's task (manifest exclusions, original\).
