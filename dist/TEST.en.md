# uaRO Graphics 0.69.0 — testing

What is new in 0.69.0 (Direct3D 12):
- **Sky past the map's edge** (Effects → "Sky past the map's edge", clouds and strength; on by default). On the fields
  and in the towns the void past the edge of the map (black or the haze's colour) now shows a sky: the map's haze
  colour at the horizon, blue higher up, clouds drifting over it; looking down past the edge, a sea of clouds. At night
  (the Night effect) the sky is dark with stars. Dungeons and houses are unchanged. Log: `sky: over <map>`.

How to test: the edge of a field (prt_fild08, pay_fild01 by the edge), zoom the camera out and turn it so the void
past the map shows; at night - the stars. Pause - compare.

# uaRO Graphics 0.68.0 — testing

What is new in 0.68.0 (Direct3D 12 and Direct3D 11, with the torches or the lamps on):
- **Rim light on characters** (Effects → "Rim light on characters" and its strength, on by default). The light of
  torches, lamps and burning skills fell on the characters before, but a flat sprite was lit evenly. Now the edge of a
  character (and of the models) that faces the fire is lit more - near a fire the sprites look rounder.

How to test: stand by a torch or a lamp (Payon, the caves, Geffen at night), walk around the fire; Fire Wall next to
monsters. Are there bright outlines where there is no fire.

# uaRO Graphics 0.67.0 — testing

What is new in 0.67.0 (Direct3D 12 and Direct3D 11):
- **Soft effects at the ground** (Effects → "Soft effects at the ground", on by default). Skills, sparks and portals
  facing the screen were cut along a straight line where they went into the ground or a wall. Now they fade softly
  over the last few steps before the surface. The characters are not touched - their feet stand on the ground as
  before.

How to test: skills by a wall and on the ground (Fire Wall, Soul Strike next to a wall, Warp Portal) - is there a
hard line; Pause - compare with the old look.

# uaRO Graphics 0.66.0 — testing

What is new in 0.66.0 (Direct3D 12):
- **The characters' pictures first.** The texture pack's pictures were read in the order asked for: on a new map the
  hundreds of its textures known ahead kept the characters' frames and the icons waiting behind them (blocky for a
  moment). Now one at a time, the most urgent first - the sprite frames and the icons, then the textures in view, then
  those known ahead; a texture asked for ahead and drawn meanwhile moves up. As Direct3D 11 does since 0.49.1.
- The log says how long the frames wait: `sprite frames - ... read in N ms on average, M at most`.

How to test: Direct3D 12, a town with many players, then another map: are the characters improved at once; send the
log.

# uaRO Graphics 0.65.0 — testing

What is new in 0.65.0:
- **The waterfalls of Brasilis** (Direct3D 12, with the modern water on): bra_fild01's map file has some forty
  waterfalls along its cliffs that uaRO.exe never shows (effect 821) - now each is a sheet of falling water with
  streaks running down and foam at its foot, hidden behind what stands nearer. The log:
  `waterfalls: the map file's drawn (N in view)`. Their width and which way they face are guessed from the map file:
  send a screenshot.
- **HDR** (Settings → Quality → HDR, off by default; Direct3D 12): on a monitor with Windows' HDR on, the picture in
  HDR10 - white at 200 nits, fires and sparks a little brighter than white. The log: `HDR: on ...`, or why not.
  Not with DLSS frame generation; Ctrl+Shift+F12 takes no screenshot while it is on.

How to test: bra_fild01 (Brasilis' field) with water on - the cliffs; HDR on with Windows' HDR on - the log and the
look (torches, skills).

# uaRO Graphics 0.64.0 — testing

What is new in 0.64.0:
- **The rain's splashes** (Direct3D 12, with the weather on, where it rains - Niflheim, Einbroch): rings spreading
  where the drops land, fixed on the ground (not on the screen), on what faces up only - not on walls.
- **Leaves in the wind** (Effects → "Leaves in the wind", off by default; Direct3D 12 and 11): the leaves of trees and
  bushes - textures that are green with holes - sway a little, their tops more than their trunks; fences and walls
  stay still.

How to test: Niflheim with the weather on - the splashes on the paths; Payon or Prontera's fields with "Leaves in the
wind" - the trees; tell if anything that is no leaf moves.

# uaRO Graphics 0.63.0 — testing

What is new in 0.63.0:
- **/aura at once (off).** The game reads /aura only when it makes an aura (a map entered): now the level 99 aura's
  ring and bubbles go the moment /aura is off; its circles stay, as the game shows them. On again: the game brings
  the ring back as it makes the aura anew (another map, the player out of view and back) - that it does itself.
- **Night** (Effects → Night, strength, "By the clock"): the world darker and colder, the torches, lamps, skills and
  fireflies shining out of it; the interface as it is. By the clock: only 20:00-6:00 by the computer's time, with an
  hour of dusk and of dawn. Direct3D 12 and 11.

How to test: a level 99 character, /aura - the ring at once off; Night on in Prontera, then in a dungeon with
torches.

# uaRO Graphics 0.62.0 — testing

What is new in 0.62.0 (a test, off by default):
- **Settings → Quality → "Interface at the screen's pixels"** (`[display] ui_native`): with a render scale over 100 %
  on Direct3D 12 without DLSS, only the world is drawn larger and made smaller; the windows and the text are drawn at
  the screen's own pixels - so they are not softened by the shrinking (150 % shrinks by a step that is no whole
  number). The way DLSS already draws them, with the world shrunk instead of upscaled. MSAA is not needed then
  (the world is supersampled). The log: `supersampling (the interface at the screen's pixels): the world at ...`.

How to test: Direct3D 12, DLSS off, render scale 150 %; the box on and off (restart the game): the windows' text,
item icons, the chat; the frame counter (Ctrl+Shift+F11).

# uaRO Graphics 0.61.0 — testing

What is new in 0.61.0 (Direct3D 11, part 2):
- **The characters' shadows on Direct3D 11** (Effects → characters' shadows): each character and monster casts its
  sprite's soft shadow on the ground, as with Direct3D 12 - lit by the sun alone (Direct3D 12 also turns them away
  from the map's lamps). The log: `characters' shadows: N characters ... (Direct3D 11, the sun's light)`.
- **The torches' light on Direct3D 11** (Effects → torches, their light): each torch lights the ground about it, warm
  and flickering a little; the flame and its glow stay the game's (Direct3D 12 draws them anew). The log:
  `torches: N lit (Direct3D 11 ...)`.
- With either on, Direct3D 11 now knows the sprites' kinds without AI textures too.

How to test: Direct3D 11; a field with monsters - their shadows; a dungeon with torches (Prontera's culverts, Glast
Heim) - the light round them; Pause to compare.

# uaRO Graphics 0.60.0 — testing

What is new in 0.60.0 (one file for a player):
- **uaro-graphics-setup.exe** (release/, next to the zip): the whole release in one program. Put it in the uaRO folder
  and run it - or run it anywhere and choose the uaRO folder; it unpacks uaRO Graphics there, runs install.bat
  without its questions, says what is next and opens the settings app: "Set up automatically", then "Download the
  pack", then the game. In English, Ukrainian or Russian by Windows' language.
- uaro-graphics.zip and update.bat stay as they were.

How to test: on another computer (or a copy of the client) run uaro-graphics-setup.exe; then the two presses.

# uaRO Graphics 0.59.0 — testing

What is new in 0.59.0:
- **The Effects page's colours on Direct3D 11** (brightness, contrast, saturation, warmth, vignette): drawn over the
  world before the interface, as with Direct3D 12 - the first of the Effects page on Direct3D 11. The others (lights,
  torches, water, shadows, weather, glow) still need Direct3D 12; the log says so.

How to test: Direct3D 11, Effects → colours on, saturation 150 %, vignette 60 %: the world changes, the windows not;
Pause shows the game's own.

# uaRO Graphics 0.58.0 — testing

What is new in 0.58.0 (an upscale for AMD and Intel):
- **Render scale under 100 %** (Settings → Quality, 50-200 %): on a large screen a weaker card draws the world smaller
  than the picture, and it is made larger sharply when shown - a Catmull-Rom filter and CAS sharpening, the way FSR 1
  does it (our own filter, not AMD's code; any card, Direct3D 12 and 11). DLSS stays NVIDIA's.
- "Set up automatically": a middle card on a screen past 2560x1440 gets 75 %, a small or old one past 1920x1200 67 %,
  with sharpening.

How to test: on a 4K (or 1440p) screen set 67 % and 100 %: the frame counter (Ctrl+Shift+F11) and the look.

# uaRO Graphics 0.57.0 — testing

What is new in 0.57.0 (weak graphics cards):
- **AI textures compressed in video memory** (Direct3D 11): BC1 for the opaque pictures (an eighth of the memory),
  BC3 for the ones with alpha (a quarter), made on the reading thread as they load. Settings → AI textures →
  "Compress in video memory": Automatic (a card under 3 GB - the old Radeons, integrated graphics), On, Off
  (`[ai] compress`). The log says "AI textures: kept compressed in video memory".
- "Set up automatically" turns the world's AI textures on from 1 GB of video memory (was 2 GB).

How to test: Direct3D 11, "Compress in video memory" On: the textures look the same at a glance, the log's "copies"
line shows a fraction of the MB of before; Off for comparison.

# uaRO Graphics 0.56.0 — testing

What is new in 0.56.0:
- **Texture pack → "Clothes colours"** (off by default): the builder makes the characters' bodies in every clothes
  dye too (`data\palette\몸\`), so players who dyed their clothes are improved as well - 4 to 9 times the bodies'
  pictures, a bigger pack and a longer make. "Make the pack" adds only what is missing.

How to test: tick "Clothes colours", "Make the pack"; in a town, players with dyed clothes smooth.

# uaRO Graphics 0.55.0 — testing

What is new in 0.55.0 (profiles):
- **Settings app → Service → "Profiles".** "Save as a profile" keeps every setting of uaro-graphics.ini (the changes
  not applied yet too) under a name - "Maximum for 4K", "Weak laptop", "Recording"; "Load" makes it
  uaro-graphics.ini again (a backup of the present one first, the app's language kept); "Delete". Profiles live in
  `uaro-graphics\profiles\`, so a profile can be handed to another player as a file.

How to test: save two profiles with different settings, load each in turn, start the game.

# uaRO Graphics 0.54.0 — testing

What is new in 0.54.0 (hotkeys):
- **Ctrl+Shift+F11 - the frame counter**: "143 fps · 7.0 ms (16 max) · Direct3D 11" in the game window's top left
  corner, twice a second; again to hide it. Settings → Frames → "Frame counter" shows it from the start
  (`[frames] fps_counter`). It is a small window of its own over the game: the same with Direct3D 12, 11 and DLSS.
- **Ctrl+Shift+F12 - a screenshot at the picture's full size** (3840x2160 on a 4K screen, not the game's 1920x1080)
  as PNG into the game's ScreenShot folder (`uaro-YYYYMMDD-HHMMSS.png`); the log says where.
- Pause or Scroll Lock stays the before/after key.

How to test: in the game Ctrl+Shift+F11, Ctrl+Shift+F12 (Direct3D 11 and 12); look at ScreenShot.

# uaRO Graphics 0.53.0 — testing

What is new in 0.53.0 ("download the pack from the site"):
- **Settings app → Texture pack → "Download from uaro.kiev.ua".** The pack the developers made, nothing to make on
  the player's computer: the settings app reads the pack on the site (its list of pictures, by HTTP ranges) and fetches
  only the pictures this computer lacks, each checked (CRC-32) as it comes and written into uaro-textures.zip at once.
  "Stop", a closed app or a lost connection keep what came - "Download the pack" again goes on from there; a newer pack
  on the site brings only its new pictures. Pictures the player's own game made stay, and both indexes are merged.
- The progress: pictures, MB, speed and the minutes left.
- For the developers: the pack goes to `/var/www/uaro.kiev.ua/web/files/graphics/uaro-textures.zip` on the server
  (https://uaro.kiev.ua/files/graphics/uaro-textures.zip).

How to test: put the pack on the site; on another computer (or after renaming uaro-textures.zip) press "Download the
pack", stop in the middle, press again - it goes on; then start the game.

# uaRO Graphics 0.52.0 — testing

What is new in 0.52.0 (reports, part 2):
- **A crash says where.** When the game crashes, the log has `crash: exception 0x... at <module>+0x...` and
  `uaro-graphics\crash.dmp` keeps a small minidump; the report (by hand or automatic) takes it along, and the site's
  admin offers it for download (WinDbg / Visual Studio).
- **The pictures the pack has none of** (`uaro-graphics\missing\`, up to 64) go with the report; the admin shows
  them in the report's card.
- The report's "Problem" lines are in English whatever the settings app's language.
- The site's admin has a "Зведення" tab: reports by reason, API, version and graphics card, those with errors.

How to test: send a report from Diagnostics; in the admin, the missing pictures show in its card.

# uaRO Graphics 0.51.1 — testing

What is new in 0.51.1 (from the first report, R-4ZK5KR):
- **The real video memory.** Windows tells a 32-bit program (the settings app) no more than 3072 MB: the report said
  "at least 3072 MB" of a 16 GB card, and "Set up automatically" would have taken it for a small card (under 4 GB).
  Now the number comes from the display driver's own key in the registry (16 GB is 16 GB); where there is none, the
  automatic setup counts such a card as a middle one, not a small one.

How to test: Diagnostics shows the card's full memory; "Set up automatically" says "a strong graphics card".

# uaRO Graphics 0.51.0 — testing

What is new in 0.51.0 ("logs and settings to the site, to debug for each device"):
- **Settings app → Diagnostics → "Report to the developers".** "What happened" (a line of your own) and "Send the
  report": the graphics card(s), Windows, monitors, uaro-graphics.ini and the logs' ends (uaro-graphics.log,
  uaro-render64.log, uaro-pack.log) go to uaro.kiev.ua - no login, no character, the Windows user name hidden. The
  window answers with the report's code (R-XXXXXX, copied): name it to the developers. The codes sent are listed.
- **"Send a report by itself when something goes wrong"** (off by default; `[report] auto` in uaro-graphics.ini,
  from the next start of the game): the driver starts the settings app hidden as a watcher of the game; after the game
  it reads the log and sends a report only when the renderer would not start, the graphics card was lost, or the game
  closed without closing the renderer - at most one in 6 hours.
- The site keeps the reports 90 days; the developers see them in its admin ("Звіти графіки").

How to test: Diagnostics → write a line → "Send the report" → a code comes; tick the automatic one, Apply, play.

# uaRO Graphics 0.50.2 — testing

What is new in 0.50.2 ("can the card be chosen somewhere?"):
- **Settings app → Renderer → "Graphics card"**, shown when the computer has several: "Automatic: <the fast one>"
  or any card by name (with its video memory). The game, the 64-bit renderer and the texture pack's builder draw on
  it; "Set up automatically" looks at the card chosen there. A card that is gone shows "(not found)" and the fast
  one is taken. In uaro-graphics.ini: `[renderer] gpu = auto` or the card's name; the log says
  `graphics card chosen: ...`.

How to test: on a computer with two cards - choose each in turn, start the game, check the log's
`Direct3D 1x renderer on ...` line names it.

# uaRO Graphics 0.50.1 — testing

What is new in 0.50.1 ("there can be strong cards of other makers, and several cards"):
- **Several graphics cards: the game is drawn on the fast one** - a laptop's NVIDIA or AMD, not its integrated
  Intel; Windows' own choice for high performance (Settings → System → Display → Graphics, where a player can pick
  the card for uaRO.exe) comes first, else the card with the most video memory. Direct3D 12 and 11 alike, and the
  automatic setup looks at that same card. The log names them: `graphics cards: ... - Direct3D 11 on ...`.
- Strong cards of AMD and Intel get the same settings as NVIDIA's of their memory (DLSS is NVIDIA's alone).

How to test: on a laptop with two graphics cards - press "Set up automatically" (it should name the fast card), play,
send uaro-graphics.log.

# uaRO Graphics 0.50.0 — testing

What is new in 0.50.0 ("a button to hand the client to other people"):
- **Settings app → Renderer → "Set up automatically".** It looks at the graphics card, its video memory and the
  screen and picks the settings, says what it found and offers to write them and start the game:
  - no graphics driver (Windows' basic display adapter): the game's own look, and a word to install the driver;
  - a card without Direct3D 12 (Radeon HD 6000 and the like): Direct3D 11, the game's size, no smoothing;
  - Direct3D 12 under 4 GB: the light settings, the heavy effects off; 4 to 8 GB: balanced; 8 GB and more: drawn at
    200 % (150 % past 2560x1440);
  - DLSS and frame generation off on cards that are not NVIDIA's;
  - AI textures on when uaro-textures.zip lies next to uaRO.exe (under 2 GB: characters, monsters, items and icons
    only); the game never makes pictures itself.

How to test: press the button on your computer (expected: "a strong graphics card", 150 %, AI textures on) and on
the other computers; send what the window said and each one's uaro-graphics.log after a game.

# uaRO Graphics 0.49.4 — testing

What is new in 0.49.4 ("rebuilt the pack - other players' hair still blocky"):
- **Fixed: the hair colours were never made.** The builder opened the game's archives for pictures only, so it saw
  none of the palettes (`data\palette\...`) - 0.49's hair colours came out empty, and the pack stayed at the same
  number of pictures.

How to test: update; Texture pack → "Make the pack" (it only adds what is missing: the hairstyles in every colour) -
the number of pictures should grow. Then Prontera: other players' hair smooth, `uaro-graphics\missing` without new
`player-...` files; send the log.

# uaRO Graphics 0.49.3 — testing

What is new in 0.49.3 ("the character facing us stays blocky; walking, the improved frames show"):
- The game caches a sprite frame once and draws it from its cache until the cell is needed for another frame. A frame
  that got there past the place the driver watches (say, before the cache was known to be one) stayed blocky all that
  time. Now the driver keeps a copy of every sprite cache and finds **every cell that changed**: such a frame is looked
  up in the pack like the rest. A frame the game writes again unchanged is no longer drawn over.
- Both Direct3D 11 and Direct3D 12. The log says how many such frames came:
  `sprite frames - N came past the game's call (found changed in their caches), M of them in the pack`.

How to test: update, start the game - the character standing facing us, at the start and after a while; send the log.

# uaRO Graphics 0.49.2 — testing

What is new in 0.49.2 (Direct3D 11: "at the start the character is blocky, after a while it is fine"):
- A sprite frame waiting for its picture from the pack is shown **smoothly enlarged** meanwhile, not texel by texel
  (that was the blocky look) - also the first frames of a new sprite cache, which went in as one blocky piece.
- The log says how long the frames wait: `sprite frames - ... from the pack (waited N ms on average, M at most)`.

How to test: update, start the game on Direct3D 11 - the character at the start; send the log.

# uaRO Graphics 0.49.1 — testing

What is new in 0.49.1 (Direct3D 11: "characters are blocky at first and get rendered after a while"):
- **The characters first.** The pictures of the sprite frames and the windows' icons are read before the textures,
  and a texture on screen before the ones known ahead (a map's hundreds as it loads) - the frames waited behind them.
- **Frames read lately stay in memory** (48 MB): an animation shows the same frames again and again - at once now.
- **What the pack has no picture of is saved** in `uaro-graphics\missing\` (the first 64 of a run, as BMP): send that
  folder - from it the developers see why (the hair, say).

How to test:
1. update.bat (or Settings → Service → Update now). If not yet done with 0.49: Texture pack → "Make the pack" (the hair
   colours, the .jpg pictures).
2. Direct3D 11, a town with many players: do the characters turn improved at once, or after how long?
3. If something stays blocky: send uaro-graphics.log and the uaro-graphics\missing folder (zipped).

# uaRO Graphics 0.49.0 — testing

What is new in 0.49.0 (after "the hair and the loading screens are not upscaled"):
- **Hair in every colour.** The game draws a head with its hair colour's palette (`palette\머리\...`), so its frames
  were not the pack's. The builder now makes every head with every hair palette of the game's files.
- **The .jpg pictures: the loading screens and the water.** The builder reads them now (it read only BMP, TGA and SPR).
  The game decodes .jpg with its own ijl15.dll, a level or two apart from ours, so these pictures are found by their
  likeness (a small copy kept in the pack's index), not by their exact key.
- **Updates from the settings app:** Service → Updates → "Check for updates" asks GitHub for the newest version;
  "Update now" starts update.bat and closes the app.

How to test:
1. update.bat (or Update now in the settings, once 0.49 is installed).
2. Texture pack page → "Make the pack": it goes on with the pictures already made and adds the hair colours and the
   .jpg ones. Send the speed and the time it shows.
3. In the game: hair of different colours, the loading screens, water - improved (Pause to compare). In the log
   `not in the pack - ...` should show fewer.
4. Service → Updates → Check for updates: it shows 0.49.0 as the newest.

# uaRO Graphics 0.48.0 — testing

What is new in 0.48.0:
- **upscaled.grf is gone: the AI textures come from the texture pack uaro-textures.zip alone** (Direct3D 12 and
  Direct3D 11). The old upscaled.grf is not used any more; the Ready textures page offers to delete it.
- **Making new ones while playing** (Direct3D 12, a debugging aid) writes the pictures **into uaro-textures.zip**,
  with their kind and the maps they were seen on in the pack's index. Only one writer at a time: while the pack's
  builder runs, the game does not write (and the other way round).
- **Check the keys** is gone from the Texture pack page. Instead the game's log says what the pack lacks:
  `AI textures: not in the pack - ground N, objects N, ...; sprite frames N, icons N`.
- The **Ready textures** page shows the pack: its pictures by map and kind, deleting the ticked ones (the pack is
  written again without them), deleting the whole pack, and deleting the old upscaled.grf.
- A pack cut short (its builder or the game stopped before its directory) is still read, by its pictures' headers.
- The settings app starts in **English** until a language is chosen (it was the language of Windows). A language
  chosen before stays.
- These notes are in three files now: TEST.en.md, TEST.uk.md, TEST.ru.md. Earlier versions' notes: TEST.ru.md.
- Fixed in passing: in a sprite cache a picture of a frame could go over a newer frame of another kind.

How to test:
1. update.bat. In the game folder uaro-textures.zip is there (made on the Texture pack page).
2. Direct3D 12, AI textures on: in uaro-graphics.log `render64: AI textures: on - ...`,
   `the texture pack uaro-textures.zip has N pictures`, every 30 seconds `... from the pack ...` and, if something is
   missing, `not in the pack - ...`. Textures improved as before. No `the 64-bit renderer exited`.
3. The same with Direct3D 11 (the Render page): `AI textures: on with Direct3D 11 - ...`.
4. Settings → Ready textures: the pack's pictures in the table; "Delete the old upscaled.grf" if it is still there.
5. Send the log and, if something looks wrong, a screenshot.

Notes of earlier versions (in Russian): TEST.ru.md.
