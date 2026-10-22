# uaRO — the beta of the new graphics and launcher

uaRO Graphics draws the game anew: HD textures, effects, anti-aliasing, DLSS on NVIDIA RTX cards, a gamepad. With it
come the launcher «Play» with the server's news, updates without a single .bat, and «Reports» - to report a glitch and
see the answer. This is the beta 3.0.0-beta.1: every new release reaches everyone at once, so if something breaks,
write to us - and the app goes back to the previous version in one press.

## Installing

- **A new player:** the full client from [uaro.kiev.ua/download](https://uaro.kiev.ua/download) - the game, uaRO
  Graphics and the HD textures are inside.
- **Already playing:** download `uaro-graphics-setup.exe` (about 12 MB) from the same page and start it in the game's
  folder (or show the folder when it asks). No .bat, no black windows; the game's desktop shortcut becomes «uaRO».
- Windows may say «Windows protected your PC»: press «More info» → «Run anyway» - our files carry no digital signature.
  If an antivirus complains about `ddraw.dll`, add the game's folder to its exceptions: it is uaRO's graphics file.
- Windows 7 and 8.1 are not supported: the game starts there with its own usual look.

## Starting

- The «uaRO» shortcut opens the page «Play»: the server's news, the online count, updates and the big «Play» button.
- Starting `uaRO.exe` directly opens «Play» too. To go straight into the game, tick «Start the game at once».
- The first start of a new version prepares the shaders: from a few seconds to a minute on a weak laptop. Then it is
  quick.

## Settings

- The settings are picked for your graphics card and screen by themselves - «Play» says «Set up for …». The main ones
  are on «Main», the rest in the «Detailed» mode.
- A new graphics card or monitor - the app asks whether to pick them again.
- Integrated graphics (Intel UHD and the like) get the super-light mode: no effects, 60 frames, the battery spared on
  a laptop.
- DLSS, frame generation and Reflex exist on NVIDIA RTX cards only; elsewhere there are no such rows.

## Updates

- A new version or new game files - «Play»'s button turns «Update and play»: it downloads, installs and starts the game.
- Something wrong after an update - «Service» → «Back to the previous version (…)».

## Reporting a glitch

- The «Reports» button at the top right of the app; the number in brackets is the developers' new answers.
- In the game **Ctrl+Shift+F10**: a full-size screenshot and at once «New report» with it and the map's name.
- Write what happened, choose «What it is», tick the screenshots (up to 6; «Add a file…» and «Paste (Ctrl+V)» are
  there too) and press «Send».
- The graphics card, Windows, the settings and uaRO Graphics' logs go with it - no login, no character, no Windows user
  name. Windows' own «Screenshots» folder is never taken by itself: only what you ticked.
- The answer comes into «Reports». Statuses: «New», «Read», «Needs information», «Being fixed», «Fixed in …», «Closed».
  Once the fix is installed: «Confirm» or «Still there».
- No internet - the report is kept and goes by itself once there is a connection.

## If the game does not start

- Two failed starts in a row - and the next one starts in the safe mode by itself (Direct3D 11 without the risky
  parts); «Play» shows a card with «Normal mode».
- «Service» → «If the game does not start»: «Safe mode» or «Switch uaRO Graphics off» - the game's own look, the
  gamepad and the launcher still work. «Switch uaRO Graphics on» brings it all back.
- Removing: «Service» → «Remove uaRO Graphics», or Windows «Settings» → «Apps» → uaRO Graphics.

## Questions

- **Everything looks old again?** You pressed Pause or Scroll Lock - that compares with the old graphics; a line in
  the game's corner says so. Press it again.
- **The effects' console** - Ctrl + the key left of 1. The key alone types «ё» and the apostrophe in the chat.
- **Gamepad:** plug it in - it works at once (Xbox, PlayStation over USB and Bluetooth); its buttons are on the
  «Gamepad» page.
- **The game in Program Files:** Windows lets the app save settings there only as administrator - better move the
  game to another folder (for example `C:\Games\uaRO`).
- **Screenshots:** Print Screen in the game (with uaro.kiev.ua on it), Ctrl+Shift+F12 at the screen's full size. Both
  go to the game's `ScreenShot` folder.

## Known problems

- On some maps with tiles the ground's seams may blink - press Ctrl+Shift+F10 right while it blinks.
- The fire pillar looks blurred in places - we are looking for the cause.
- The HDR row shows only while HDR is on in Windows «Settings».
