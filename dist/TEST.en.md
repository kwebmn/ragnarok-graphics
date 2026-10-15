# uaRO Graphics 0.50.0 — testing

What is new in 0.50.0 ("a button to hand the client to other people"):
- **Settings app → Renderer → "Set up automatically".** It looks at the graphics card, its video memory and the screen and
  picks the settings, says what it found and offers to write them and start the game:
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
