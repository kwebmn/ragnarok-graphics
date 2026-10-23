# Several game windows - implementation plan

**Goal:** a player who plays in several uaRO windows at once (the main character and helpers: a priest, a Soul
Linker, a Sage for endows) gets each window's logs apart, the character's name in the window's title, each
character's window where it was put, and the game windows not played in drawn light and without their music.

**Spec:** the conversation of 2026-10-24 - the ideas offered, the user chose «1 и 2»: (1) a log per window - a second
game truncates the first's logs (win/log.cpp: MoveFileEx fails on the held file, fopen "w" empties it); (2) the
character's name in the title, the window's place per character, a light mode for the background windows (no HD
pictures, no DLSS, no effects, half the resolution), the music only from the window in front. The rest (a panel of
the helpers, alerts, buff timers, starting the set, control) is to be discussed - not here.

**Decisions (mine):**
- The window's size is not kept per character: the game's resolution is one registry value for every window, and a
  window of another size would put the game's mouse off. Its place is.
- The light mode is automatic, not a mark per character: a game window is light while another game window is the
  main one (in front, or the last one in front while another program is). Switching is instant - the HD pictures stay
  in video memory, only not drawn; the effects are set per frame.
- The name comes from uaRO.exe's memory: CSession 0x771148 +0x618, 64 bytes XOR the 64 at *(0x6e2f78)
  (GetCharName 0x642500, SetCharName 0x642550, called at the character select 0x45aeab); read only when
  GetCharName's bytes are this build's.
- The places are kept by the character's name (the server's, unique) in uaro-graphics\uaro-windows.txt.

## Global constraints

- uaRO.exe is read, never changed. ddraw.dll imports kernel32, user32, gdi32 and msvcrt only.
- Each task is a beta through ship-beta.sh (3.0.3-beta.1 ... beta.3); releases wait for the user's word.
- Code comments in English, lines up to 120, the gate before every beta.
- UI names in Ukrainian as strings.cpp has them; new rows on the Detailed level; their hints are the Help's text.

## Task 1 (beta.1): a log per window

- `core/log_files`: `kSlots = 8`, `slotted(path, slot)`: "a.log" -> "a-2.log" (slot 1 or less: as it is).
- `win/log`: `openFree(path, keepPrevious, firstSlot, opened, slot)` - "a+" with _SH_DENYWR (a log a running game
  writes refuses), the previous game's text copied to .prev.log when there is any, then emptied (_chsize);
  `open(path, level, firstSlot = 1)` returns the slot; `path()`, `slot()`; the 32 MiB part copied and emptied, no
  close.
- `pad/win/log`: `openFree`, the driver's slot first, no .prev.
- `real_ddraw`: a line naming the window and its log from the second on; the safe mode judges previous(path());
  `--log-slot N` to the watcher, to the in-game report and to uaro-render64.exe (`host/main` opens its log there).
- `app/args`: `logSlot` (`--log-slot N`, written when above 1); `watchGame` reads that slot's log; `makeUpload` sends
  that slot's logs; a report from the app also the other windows' game logs, 8 files at most (the site's limit).

## Task 2 (beta.2): the name in the title, the place per character

- `core/char_name`: `decodeCharName(stored, key)` - 64 bytes each, the name to its NUL, 24 bytes at most.
- `core/window_places`: uaro-windows.txt - "x y name" a line, the name to the line's end; parse, format, find, set.
- `win`: the name in the game mode only; per frame (every 30th) the title "<name> — <the game's own title>" set by
  DefWindowProcW(WM_SETTEXT) when it changes, the game's own back when the character leaves; a framed window (a
  caption, neither minimised nor maximised) moved to its character's place when the character comes (when that place
  is on a monitor), and its new place written a second after the player moved it (no mouse button down).
- `[windows] title_name = 1`, `remember_place = 1`: rows on the Game page, strings, dist ini.

## Task 3 (beta.3): the other windows light, the music in the main one only

- `core/light_mode`: `overridesOf(c, onBattery, otherWindow)` - another window and `[windows] others_light`: effects
  none, AI pictures hidden, render scale 50 % at most, no sharpening, no supersampling, no DLSS (`noUpscaler`), 10
  frames a second at most.
- `win`: the main window - a shared DWORD (Local\uaro-graphics-front) holds the PID of the game last in front; this
  window is the main one when in front, or that PID is its own, none, or a game that ended.
- `sound`: no music in a window that is not the main one unless `[windows] others_music = 1`; the effects as before
  (`[sound] background`).
- `[windows] others_light = 1` (Frames page), `others_music = 0` (Sound page): rows, strings, dist ini.
