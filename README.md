# uaRO Graphics

**A modern renderer for the classic Ragnarok Online client of [uaRO](https://uaro.kiev.ua).** A drop-in
`ddraw.dll` takes the client's DirectX 7 drawing and makes it with Direct3D 12 (or Direct3D 11 on older cards):
the game at your monitor's real resolution with crisp text, smooth edges and sharpening, and on top of it optional
effects, textures redrawn by a neural network and NVIDIA DLSS — all switchable while you play.

Players: **[Українською](README.uk.md)** · **[По-русски](README.ru.md)** · what changed in each build:
[English](dist/TEST.en.md) · [Українська](dist/TEST.uk.md) · [Русский](dist/TEST.ru.md)

## Download

- **[uaro-graphics-setup.exe](https://uaro.kiev.ua/files/graphics/uaro-graphics-setup.exe)** — the installer: put
  it into the uaRO client folder (or choose that folder when it asks) and run it; it does the rest and opens the
  settings. A client under Program Files asks Windows for administrator rights by itself.

Updates come from uaro.kiev.ua: the settings app checks at its start, and the game's next start asks before the login
window — **Update** closes the game, updates it and starts it again, with no console window. The previous version is
kept: **Back to the previous version** on the Service page. **Remove uaRO Graphics** there (or in Windows' Apps) puts
the folder back as it was before the install — dgVoodoo too, if it was there; the settings and the texture pack can
stay.

## What it does

- **Resolution and text.** The game is drawn at the window's real pixels, not stretched: the world sharper, the
  interface at the size you choose, the text and the windows' frames crisp (Screen and Graphics pages). MSAA,
  anisotropic filtering, TAA and contrast-adaptive sharpening are on the Graphics page.
- **Effects, chosen by quality.** *Auto* picks a set by your graphics card — *Low* (Direct3D 11 or under 4 GB of
  video memory), *Medium* (4–8 GB) or *High* (8 GB and more); *Off* and *Custom* are there too. Fourteen groups by
  what the eye is on — day, night and seasons; weather; characters and monsters; skills; fire and smoke; light; ground
  and walls; grass and trees; water; creatures; caves and dungeons; sky and air; the whole screen; game fixes — each
  with its own switch and strength, and each opens to its effects. New effects come switched off.
- **AI textures.** The Neural network page's Pack tab builds `uaro-textures.zip` from the game's own pictures with Real-ESRGAN
  models or Game AI 2.0 (downloaded apart, with the page's button), a model for each group of pictures; the game then shows
  them by kind — ground, buildings, characters and items, skills, icons.
- **uaRO Studio** (`uaro-graphics\uaro-studio.exe`, for the pack's makers and testers): every texture of the game
  beside its pack picture — its archive and the ones it shadows, the model and the date it was made, the maps it is
  on, quality flags (smoothed, tone shifted, bands, halos, a pink fringe, spoiled alpha, seams); any texture made
  again by any model and a chain of steps (tone, grain, sharpening, clean alpha, an atlas's tiles one by one), looked
  at and kept or discarded; taken out, the game's own always, its material lighter or stronger. The running game
  shows the changes at once; in the game Ctrl+Shift+F8 opens the texture under the cursor in the studio.
- **NVIDIA DLSS and frame generation** on GeForce RTX cards (the Graphics page), drawn by the 64-bit renderer
  `uaro-render64.exe`; the DLSS package is downloaded at installation on computers with an NVIDIA card.
- **The settings app in three modes.** *Simple* — the Main page with the choices a game needs, the effects' groups
  and the gamepad; *Detailed* — every setting on eight pages; *Developer* — the Debug page and the test rows too.
  The window grows and remembers its place, **Ctrl+F** finds any setting and changes it in place, a dot marks a
  value that is not the one recommended for your computer.
- **The gamepad.** [uaRO Pad](https://github.com/kwebmn/ragnarok-gamepad) comes along: the Gamepad page turns it on.
- **Reports and statistics.** **Report a problem** at the top of the settings app sends a report to the
  developers with one button (a code
  `R-XXXXXX` comes back) and can send one by itself when a game went badly. With your leave — asked once — the
  settings app sends anonymous statistics of which effects are kept and how the cards cope; no name, no login.

## In the game

| Keys | What |
|---|---|
| `~` | The console: quality, groups and effects (letters search), the AI textures, the frame counter — changed live |
| `Pause` or `Scroll Lock` | Every improvement off and on again, to compare |
| `Ctrl+Shift+F11` | The frame counter: frames a second, milliseconds, the graphics card's milliseconds |
| `Ctrl+Shift+F12` | A screenshot of the picture as shown, into the game's `ScreenShot` folder |
| `Ctrl+Shift+F9` | uaRO Studio's X-ray: textures from the pack green, made in the game blue, without a picture red |
| `Ctrl+Shift+F8` | The texture under the cursor opened in uaRO Studio |
| `Ctrl+Shift+F7` | That texture's picture off and on again (A/B) |

In the console, hold `Tab` on an effect or a group to see the picture — and the card's milliseconds — without it.

## Requirements

Windows 10 or 11 and a graphics card with Direct3D 12 for everything. Cards with Direct3D 11 only draw the game,
its resolution, crisp text, the AI textures and a part of the effects. **Recommended settings** on the settings
app's Main page (**Set up for this computer** on the Graphics page) picks the renderer, the quality and the
effects for your card.

## Files in the client folder

Since 2.8 only `ddraw.dll` and `uaro-graphics.exe` (with the gamepad also its `dinput.dll`) sit next to `uaRO.exe`;
the rest is in `uaro-graphics\`, and a 2.x install moves there by itself at the first start.

| File | What |
|---|---|
| `uaro-graphics\uaro-graphics.ini` | Every setting; the settings app edits it, the game reads changes within a second |
| `uaro-graphics\logs\` | The logs of the driver (and of the game before), the 64-bit renderer, the pack's builder |
| `uaro-graphics\cache\` | The shader caches and the prepared pipelines |
| `uaro-graphics\textures\uaro-textures.zip` | The AI texture pack |
| `uaro-graphics\backup\` | The settings app's copies of the settings |
| `uaro-graphics\dgvoodoo\` | The client's dgVoodoo, back in its place when uaRO Graphics is removed |
| `uaro-graphics\` | The driver, the settings app, the 64-bit renderer, the models, the notes of «Що нового» |
| `uaro-graphics\seen\` | What the game drew on the last 20 maps and how (uaRO Studio reads it) |

## Building from source

Everything is cross-built on Linux with MinGW-w64; the portable core is tested on Linux.

```sh
# ddraw.dll, the settings app uaro-graphics.exe and the installer (32-bit, as the game)
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-win -j
# the 64-bit renderer uaro-render64.exe and uaRO Studio uaro-studio.exe
cmake -S . -B build-win64 -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-x86_64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-win64 -j
# the core's tests
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug && cmake --build build-host -j && ./build-host/uagfx_tests
# the release archive and installer (build-win/uaro-graphics.zip)
bash tools/package.sh
```

`tools/check_dll.py` keeps `ddraw.dll` importing only `kernel32`, `user32`, `gdi32` and `msvcrt`;
`tools/check_exe.py` keeps the settings app to functions Windows 7 has. The shaders (`src/win/gpu/shaders.cpp`) are
compiled on the player's computer by `d3dcompiler_47.dll` (FXC) and kept in `uaro-graphics\shaders-*.bin`; check
changes with DXC `-HV 2018`, which keeps to FXC's rules (a loop's variable lives on after the loop).

## Project layout

| Folder | What |
|---|---|
| `src/core` | Portable code without Windows headers: settings, effects and their groups, formats (GRF, RSW, RSM, GND), the AI texture pack, statistics — tested in `tests/` |
| `src/win` | `ddraw.dll`: DirectDraw 7 and Direct3D 7 over the renderers, the game's hooks, the console, the frame counter |
| `src/win/gpu` | The renderers: Direct3D 12 (and its effects), Direct3D 11, RTX Remix, the bridge to the 64-bit process |
| `src/host` | `uaro-render64.exe`: the 64-bit renderer, DLSS through NVIDIA Streamline, the texture pack builder |
| `src/app` | `uaro-graphics.exe`, the settings app — and the installer, the updates and the removal (with the package appended it is `uaro-graphics-setup.exe`) |
| `src/studio` | `uaro-studio.exe`, uaRO Studio; its engine is `uaro-render64.exe --studio` |
| `dist` | What the archive carries besides the programs: the default `uaro-graphics.ini`, the notes, `install.bat` for older updaters |
| `docs` | Research of the client and the design notes |

## License

[MIT](LICENSE). Real-ESRGAN models: BSD-3-Clause (`dist/ai/LICENSE-Real-ESRGAN.txt`); Game AI 2.0 by Tal: WTFPL
(`dist/ai/LICENSE-GameAI.txt`); NVIDIA Streamline headers: MIT; miniz, libwebp and stb_image under their own licenses
in `third_party/`.
