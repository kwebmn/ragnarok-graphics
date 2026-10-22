# uaRO Graphics beta - design

The approved design is the page https://claude.ai/artifact/QjTrLqpnyyw8qKiUZQ3i24 (Russian, with mockups). This file
is its summary for whoever works from the repository; where they differ, the page and the user's later words win.

## Intent

A beta for the players of uaro.kiev.ua: one download, the game runs on any graphics card (NVIDIA, AMD, integrated
Intel/AMD) with settings picked for it, our files do not litter the game's folder, nothing in the app is broken or
misleading, a glitch is reported in one press and the tester sees the answer, updates come by themselves - no
console windows, no .bat.

## Decisions (2026-10-22, the user)

| # | Question | Decided |
|---|---|---|
| 1 | Our files in the game's root | `ddraw.dll`, `uaro-graphics.exe`; with the pad also `dinput.dll` |
| 2 | 64-bit renderer | on by itself where Direct3D 12 and 64-bit Windows are |
| 3 | HD texture pack | the big pack ships with the full client |
| 4 | Update channels | 2026-10-23: two - «Реліз» (branch `main`, every player) and «Бета» (branch `beta`, the testers); see `2026-10-23-update-channels-design.md` |
| 5 | Report when the game crashes | asked at the first start, «Так» preselected |
| 6 | Beta number | 3.0.0-beta.1 |
| 7 | Screenshot watermark | `data\texture\scr_logo.bmp` («uaro.kiev.ua») in the new full client and as the first patch |
| 8 | `uaro-flags.grf` | the client's file; out of our package |
| 9 | Testers' computers | open - needed for the closed round |
| 10 | Windows 7 / 8.1 | not supported; the game must still open with its own look |
| 11 | Code signing | none; instructions for SmartScreen and antivirus |
| 12 | Answers to reports | the user in the site's admin; Claude drafts answers and sets «Виправлено в X» at a release |
| 13 | `uaRO.exe` started directly | the launcher page «Гра» first; «Запускати гру одразу» turns that off |
| 14 | Where updates come from | 2026-10-23: GitHub (raw files of the channel's branch); the game's own files through uaro.kiev.ua |
| 15 | Contact field in a report | none: the conversation in the app only |
| 16 | Desktop shortcut | the installer replaces the game's with «uaRO» (the launcher) |
| 17 | Patcher of the game's files | yes, its own phase before the closed round |
| 18 | Name for the players | «uaRO» (launcher, graphics settings, reports in one program; the file stays uaro-graphics.exe) |

## Phases (one release each, shipped on the user's word)

1. Clean game folder (this file's section below).
2. Install and update without .bat: a C++ installer (install, update, repair), update check at the app's and the
   game's start with a question, a progress window, rollback, removal from the app and from Windows' Apps; the site
   serves the newest version and the archives; a transitional install.bat for 2.x installs.
3. Launcher «Гра»: the site's news in the player's language, the server's online count, updates, «Грати» /
   «Оновити й грати»; the «uaRO» shortcut; a direct `uaRO.exe` start opens «Гра».
4. Sets itself up: auto setup at install and first start, integrated GPU tiers, `api = auto`, safe mode after two
   failed starts, «Безпечний режим» and «Вимкнути uaRO Graphics», the start splash in the player's language.
5. «Звернення»: the reports window (list with statuses, conversation, new report with screenshots, offline queue),
   Ctrl+Shift+F10 in the game, the site's statuses/answers/API and the admin page.
6. Game files: the client patcher (admin «Оновлення клієнта», loose files into `data\`, never DATA.INI).
7. Nothing extra: rows hidden by hardware, developer tools only in the Developer mode, the console on Ctrl + the key
   left of 1, texts checked, the package without the AI models.
8. Guide (uk/ru/en) for the site.
9. Closed round on the testers' computers.
10. Beta 3.0.0-beta.1: the full client with uaRO Graphics and the pack (built in a separate WSL session, see
    /root/uaro-client-build/CLAUDE.md on loki).

## Phase 1: the game's folder

After phase 1 the root holds, of ours: `ddraw.dll`, `uaro-graphics.exe`, the pad's `dinput.dll`. Until phase 2 the
.bat files stay (the update goes through update.bat); until the pad's own release its `uaro-pad.ini`, `uaro-pad.log`,
`uaro-pad.moved` stay. `uaro-flags.grf` is the client's file now and is left where it is.

```
uaro-graphics\
  uaro-graphics.ini         settings (the package never ships a file at this path)
  logs\                     uaro-graphics.log (+ .prev.log, .1.log), uaro-render64.log, uaro-pack.log,
                            uaro-studio-engine.log, uaro-graphics-report.txt; each at most 32 MiB
  cache\                    pipelines.txt, shaders-32.bin, shaders-64.bin, shaders-d3d11.bin, studio-shaders-64.bin
  textures\                 uaro-textures.zip
  backup\                   the settings app's copies
  dgvoodoo\                 the client's dgVoodoo, back in its place on removal
  notes\                    TEST.en.md, TEST.uk.md, TEST.ru.md («Що нового»)
  defaults\                 uaro-graphics.ini (the package's template)
  ddraw.dll, uaro-render64.exe, uaro-studio.exe, version.txt, ai\, dlss\, pad\, ...
```

A 2.x install's files move there at the first start of the driver or the settings app; what cannot move (a game
under Program Files, a pack held open) keeps working where it is.
