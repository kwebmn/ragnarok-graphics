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
