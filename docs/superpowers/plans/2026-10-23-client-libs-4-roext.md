# Client Libraries, Step 4: ROExt's Work in the Driver - Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (inline, the user's way); self-review.

**Goal:** ROExt (the client's 8.7 KB `dinput8.dll`, loaded by Windows' DirectInput from the game's folder) no longer
needed: the driver does its work, the installer moves `dinput8.dll` aside and brings `dinput8.ini`'s settings into
uaro-graphics.ini - 3.0.1-beta.8 on «Бета».

**Spec:** `docs/superpowers/specs/2026-10-23-own-client-libraries-design.md` (decision 1, step 4).

## What ROExt does (uaRO__dinput8.dll disassembled 2026-10-23)

- DllMain reads `.\dinput8.ini` [ROExt] by GetPrivateProfileInt; its own defaults: MinimizeKey 0,
  RemapMouseButtons 1, RemapAltF4 0, WindowOnTop 0, WindowLock 1, WindowPosX/Y 0, WindowWidth/Height 0 (width rounded
  up to 4), AutoFreeCPU 1 (0 off, 1 or less = 250 ms, more = ms), CodePage -1. The client's file: AutoFreeCPU 4,
  WindowLock 1, RemapMouseButtons 0, the rest off, 1600x863 (unused under the lock).
- CreateWindowExA (the game's main window: no ex style, parent, menu or param; style WS_CLIPCHILDREN|WS_CAPTION|
  WS_MINIMIZEBOX[|WS_SYSMENU] or WS_POPUP|WS_MINIMIZEBOX): style 0x860A0000 under the lock (borderless), else
  0x06CA0000; WS_EX_TOPMOST when on top; the lock: the work area (SPI_GETWORKAREA, width rounded up to 4); else
  WindowPos/Size when both sizes are set (the client size, AdjustWindowRect), else the game's. Records the thread.
- PeekMessageA on that thread: middle button -> F24, X1 -> F23, X2 -> F22 key down/up (RemapMouseButtons; ROExt took
  the X button from MK_XBUTTON1, wrong on the release - ours from HIWORD(wParam)); Ctrl+W -> SC_MINIMIZE
  (MinimizeKey); Scroll Lock's WM_KEYDOWN/WM_KEYUP dropped (always); Alt+F4 -> F4, Alt+Print Screen -> Alt+F4
  (RemapAltF4); WM_CHAR made UTF-8 under CodePage 65001; with the window behind and no message: up to AutoFreeCPU
  ms waiting for input (MsgWaitForMultipleObjectsEx, QS_INPUT 0x1C07).
- DirectDrawCreateEx -> IDirectDraw7::WaitForVerticalBlank skipped behind: the driver's own [frames] save_power
  covers the background - not carried.
- CodePage != -1: MultiByteToWideChar/WideCharToMultiByte from the exe take it (UTF-8: flags 0); CharNextExA/
  CharPrevExA (UTF-8 aware); GetClipboardData/SetClipboardData CF_TEXT through CF_UNICODETEXT.

## Tasks

1. core/roext (pure): `Settings`, `fromDinput8Ini`, `iniEdits` ([roext]), `isGameWindow`, `windowPlan`,
   `transform` (the messages). Tests `tests/test_roext.cpp`.
2. Config `[roext]` (window_lock, window_on_top, window_x/y/width/height, mouse_buttons, alt_f4, minimize_key,
   free_cpu, code_page) and `[client] roext`; schema rows (Page::Game; client.roext on Service), texts, template.
3. Installer: `dinput8.dll` a library with no file of ours (`aside`): on - moved to uaro-graphics\original\ and
   dinput8.ini's settings written into [roext]; off - back. Tests.
4. Driver: win/iat (an import slot by name, shared with sound_hook); win/roext (hooks at DllMain after the pad's
   mount; only with [client] roext = 1 and no dinput8.dll in the game's folder); the pad's hooks chain by name when
   the slot is another's; «Гра й чат» rows; logs.
5. Notes, gate, ship-beta; the build's handoff (dinput8.dll out of the manifest, dinput8.ini the player's).
