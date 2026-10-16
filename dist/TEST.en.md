# uaRO Graphics 1.5.5 — testing

- **The pack builder (Texture pack page → Make the pack) did not work with anime 6B:** it has its own way through
  the network, which knew only the old kind - anime 6B went through as a compact model, the pictures came out as
  garbage and the card hardly worked. The builder now takes the same tiled way as the game, the work handed to the
  card in parts (a large texture is over a trillion multiply-adds).
- anime 6B pictures the 1.5.4 builder or older wrote into a pack are wrong: delete that pack.
- How to check: Make the pack with anime 6B - the card busy, proper pictures in the pack.

# uaRO Graphics 1.5.4 — testing

- **Making textures nearly stopped in the background.** Behind other windows the game draws 15 frames a second
  (power saving), and the network got its share of each frame - four times less work in the background while the
  card stood idle. With "Make new ones while playing" the network now takes up to 40 ms of each frame there. In
  front - as before, by "The card's time a frame".
- How to check: turn on making, put the game behind another window for a minute - the log's "made by the network"
  grows faster than it did in the background.

# uaRO Graphics 1.5.3 — testing

- **anime 6B works** (1.5.2: the check passed, the textures are made). The start with this model took 10 seconds
  longer: the step-by-step check always ran. It now runs only when the usual check fails (for the log) - the start
  takes about a second.
- How to check: start with "anime 6B" - the log says `self-test passed in ... ms` around 1000 ms, not 10000.

# uaRO Graphics 1.5.2 — testing

- **anime 6B: the graphics card hung at the check's very first step** (0x887a0006 - DXGI_ERROR_DEVICE_HUNG), and the
  game did not start after it. The likely cause: the shader read "no such" stand-in addresses (0xFFFFFFFF) under a
  condition, and the game's compiler may read them before testing it - far past the buffer's end. Every read of the
  shader now stays inside its buffer; what is not wanted is multiplied by zero.
- How to check: as for 1.5.0 - the "anime 6B" model, "Make new ones while playing", Direct3D 12. Should it fail
  again - send the log (the line with `anime-6b`).

# uaRO Graphics 1.5.1 — testing

- **anime 6B: the graphics card's check against the CPU failed** (a difference of 255 of 255), and the game made
  no picture after it. The likely cause: a shader variable named `half`, a type in HLSL - the game's compiler could
  read it otherwise. Renamed. The steps' logic is checked by tests against an exact copy of the shaders in C++, whole
  and in tiles.
- The check now goes step by step first: should the card go its own way somewhere, the log names the step and the
  values (`step N of 96 ... differs: GPU ..., CPU ...`) or the card's stop code (`the graphics card stopped at step`).
- A render target that cannot be made logs the card's state.
- How to check: as for 1.5.0 - the "anime 6B" model, "Make new ones while playing", Direct3D 12. Should the check
  fail again - send the log (the line with `anime-6b`).

# uaRO Graphics 1.5.0 — testing

- **A third model for the AI textures: anime 6B** (RealESRGAN_x4plus_anime_6B, Real-ESRGAN, BSD-3). Made for drawn
  pictures: finer lines, details kept, less of the "plasticine" look. 5-7 times slower than anime. Chosen on the AI
  page in both model lists (the ground and buildings; characters and effects). Direct3D 12 only.
- A pack made before stays as it is: the model shapes new pictures (in the game with "Make new ones while playing",
  or when the pack is built).
- Large textures go in pieces of 96x96 with margins of 10 pixels; small ones (up to 116x116) whole.
- At the start with this model the graphics card is checked against the CPU; should they differ, the log says
  `the GPU's network does not match the CPU's` and the AI textures stay off.
- How to check: move `uaro-textures.zip` away (rename it), choose "anime 6B" on the AI page, turn on "Make new ones
  while playing", the card's time 8-10 ms, start on Direct3D 12 — the textures sharpen step by step. The log says
  `AI textures: on - ... realesrgan-x4plus-anime-6b`. Put the pack back after the test.

# uaRO Graphics 1.4.2 — testing

- **Depth of field: the ground went soft when the character stood under a tree or a roof.** The focus came from
  the depths seen in the middle of the screen, and there a tree's crown, much nearer the camera than the ground,
  took it — all the ground around blurred; a step away and it was sharp again. The focus now comes from the game's
  camera — exactly the point it looks at, the character, whatever covers it. Without the camera (another exe) — as
  before.
- How to check: turn on the depth of field, step under a tree's crown or an awning — the ground around the
  character stays sharp, only the distance is soft.

# uaRO Graphics 1.4.1 — testing

- **uaRO Pad 0.7.0 comes along: talking to NPCs from the gamepad.** D-pad ↓ talks to the nearest NPC (with the right
  stick pushed — to the one it points at). While the NPC's window is open the buttons answer it: A — Next / Close /
  OK, D-pad ↑ ↓ — choose in a menu, B — cancel; a number (how many to buy, store): ↑ ↓ by 1, RB / LB by 10, → ← by
  100. A hint line at the bottom of the screen. The pad presses the client windows' own buttons.
- The Gamepad page has a "Hint in NPC windows" box.
- To update the pad: Gamepad → Update (or update.bat updates a switched-on pad itself).
- How to check: go to a Kafra, D-pad ↓ — her window opens; A pages on, ↑ ↓ choose a line, A takes it; at a shop or
  the storage — the number field. Should something not press — send uaro-pad.log (the `NPC window` lines).

# uaRO Graphics 1.4.0 — testing

- **The gamepad comes along: uaRO Pad 0.6.0 and a new Gamepad page.** The pad travels in the package
  (uaro-graphics\pad\). Switch on puts its dinput.dll into the game's folder; the client's dinput.dll (ROExt) becomes
  dinput8.dll and goes on working, Switch off puts everything back. The game has to be closed.
- **PlayStation controllers (DualShock 4, DualSense) over USB and Bluetooth** — read directly, no DS4Windows or Steam;
  rumble and the light bar. On the page: which controller (any / Xbox / PlayStation), the light bar's colour, what the
  touchpad's click does, vibration, whether the pad is on at the start, its menu's language. Saved to uaro-pad.ini at
  once.
- update.bat updates a switched-on pad together with the graphics.
- How to check: Gamepad → Switch on → connect the controller → start the game: the stick walks, Start opens the pad's
  menu (with a PlayStation controller it names Cross/Circle/Square/Triangle). Change the light bar's colour — after a
  restart of the game it shines in the new one. Switch off — the old dinput.dll is back, the game as before.

# uaRO Graphics 1.3.5 — testing

- **Depth of field: the distance "swam" while walking.** The blurred distance came from the picture made four times
  smaller by averaging 4×4 squares; as the camera followed a walk the picture moved across that grid and the blur
  shook. The reduction is now soft (8×8 with smooth weights) — a pixel's move hardly changes it. Direct3D 12.
- How to check: turn on the depth of field, walk through a town — the far houses evenly blurred, not shaking. Should
  it still swim — tell us (and check whether it goes with the depth of field off).

# uaRO Graphics 1.3.4 — testing

- **Text jumped while typed** (the login, the chat). Crisp text spread its large letters evenly over the whole line
  since 0.41: every new letter changed the line's width and moved the ones already typed a little. Each letter now
  sits in the middle of its own place in the game's line — the typed ones stay, the room after "m" and "w" split
  either side.
- How to check: type the login and a chat message — the letters stay put, the windows' text reads evenly. Should it
  still jump — turn off crisp text on the Quality page and tell us.

# uaRO Graphics 1.3.3 — testing

The auto setup ("Set up for this computer") now sets these too:
- **The interface a whole number of times.** Where it is a fraction now (167 %: the game at 1150×647 on 1920×1080 —
  soft letters), 100 % (up to 1440p) or 200 % (4K) — the crispest letters.
- **NVIDIA RTX:** DLAA on screens up to 2560×1440 (the picture at its own size, smoothed by the network; DLSS Quality
  at 1080p draws the world at 720p and it shows), DLSS Quality on larger ones. GTX and other cards: DLSS untouched.
- **HDR**, when Windows' HDR is on for the main monitor and the card draws with D3D12.
- **Sound:** the new surround engine (Windows 10 and later) and spatial sound for headphones.
- The auto setup's window lists what of these it chose.
- How to check: on a computer with a fractional interface press the auto setup — the window says "The interface at
  100 %", the game then at the screen's own size; on an RTX 3060 at 1080p — "Smoothing: NVIDIA DLAA".

# uaRO Graphics 1.3.2 — testing

- **The settings app started from the uaro-graphics folder.** Opened not from the client folder but from its
  uaro-graphics subfolder, it did not see the game: "Cannot start uaRO.exe (error 2)", "driver unknown", the update
  without update.bat. The app now finds the client in the folder above. With no game there either, the start's error
  says where the app belongs.
- How to check: run uaro-graphics\uaro-graphics.exe — the driver's version shows, "Apply and play" starts the game,
  "Update now" works.

# uaRO Graphics 1.3.1 — testing

- **"Update now" without update.bat.** With a client copied without the install's files, the Service page's button
  said "no update.bat in the game's folder". The app now puts its own update.bat into the game's folder and runs it.
- How to check: delete update.bat from the game's folder, press "Update now" — the file appears, the update runs.

# uaRO Graphics 1.3.0 — testing

- **Music makes way in a fight** (the Sound page, on by default; works without the new engine too). While many
  sounds play at once — a fight, a crowd's skills — the music goes quieter, down to 65 % at 14 sounds and more, and
  comes back over two seconds when the fight is over.
- **The maps' ambient sounds** (birds, water, machines — placed by the maps' makers: 22 thousand sources in 480 maps)
  are played by the game itself; with "Surround sound (new engine)" they now come from their side too, duller far
  off and with the place's echo.
- How to check: in a fight with several monsters or in a crowd with skills the music is quieter and comes back after;
  on a map with ambient sounds (1@gef, towns with fountains) the birds and the water come from their side with the
  new engine.

# uaRO Graphics 1.2.0 — testing

- **Headphones: Windows' spatial sound** (the Sound page, on by default, works with "Surround sound (new engine)").
  When spatial sound is on in Windows — "Windows Sonic for Headphones" (free, built in) or Dolby Atmos / DTS
  Headphone:X — the sounds of monsters, skills and hits go to Windows as objects in space, and Windows itself makes
  them surround in headphones: behind, in front, beside. The place's echo stays. With Windows' spatial sound off —
  as in 1.1 (placed on the speakers).
- To turn it on in Windows: right-click the sound icon → Sound settings → the output device → Spatial sound →
  Windows Sonic for Headphones.
- The log: `spatial sound: Windows spatial sound, N objects - ...` (or `not on in Windows`), and
  `sound engine: the game's sounds placed around the player (Windows spatial sound, ...)`; every minute how many sounds
  went to the spatial sound.
- How to check: in headphones turn your back to a monster — the hits sound behind; turn the camera — the sound moves.

# uaRO Graphics 1.1.0 — testing

- **Surround sound (new engine)** — the Sound page, "Sound through uaRO Graphics", off by default, after the game is
  restarted; Windows 10 and later (Windows 7 keeps the game's sound). The sounds of monsters, skills and hits are
  played by uaRO Graphics itself through XAudio2 instead of Miles:
  - around you on any speakers — stereo, 5.1, 7.1: what is behind sounds behind (the camera's turn counted);
  - far sounds quieter and duller (the highs fade with the distance), at the edge of hearing a sound fades out instead
    of breaking off;
  - the place's echo: caves and dungeons long, houses short, towns a little, fields hardly; the "Echo of the place"
    slider;
  - quieter in the background and the crowd's even loudness — as before.
  The music stays Miles'. Sounds the engine cannot play (not PCM) play as before.
- The log: `sound engine: XAudio2 at ... Hz on N channels ..., reverberation`, then
  `sound engine: the game's sounds placed around the player (stereo|5.1|7.1, the place N)` and a count every minute.
- How to check: turn it on, restart the game; stand with a monster on the left/right/behind — the sound comes from
  there (in headphones left/right; surround for headphones is the next step); go into a cave — the echo; walk away
  from a monster — duller. Should sounds be missing or distorted — turn it off and send a report.

# uaRO Graphics 1.0.2 — testing

- **Monsters and the character blinking with the fade-in.** A sprite already shown was now and then taken for a new
  one and faded in again: when it gained a layer (a weapon in an attack, a head turned), when it was missing from one
  frame or a frame was shown twice. A sprite is now remembered for 0.4 s after it was last drawn, and a new layer
  next to a shown sprite of the same kind is shown at once.
- How to check: turn on the fade-in, hit monsters, walk in a crowd, turn the camera — nobody blinks; monsters coming
  into view at the screen's edge still fade in.

# uaRO Graphics 1.0.1 — testing

- **Fireflies in Direct3D 11.** The fireflies the game draws itself (forests, night maps) are now drawn anew in
  Direct3D 11 too — soft twinkling lights, hidden behind trees and houses, as bright as the slider says. The maps'
  hidden fireflies from their files are not there in D3D11 yet (they need the map read, which D3D12 alone does). The
  log has `fireflies: the game's fireflies drawn anew (Direct3D 11)`.
- How to check: Direct3D 11, turn the fireflies on, go to a map that has them (the woods by Payon at night, say) —
  soft blinking lights instead of the game's squares.

# uaRO Graphics 1.0.0 — testing

- **The effects' glow in Direct3D 11.** The "Glow of effects" switch (with its sliders) now
  works in Direct3D 11 as in D3D12: skills, fire, auras shine with a soft halo. Water under the glow is drawn by its
  own shader. The log has `glow: ...x..., N levels (Direct3D 11)`.
- How to check: Direct3D 11 on the Render page, turn on "Glow of effects", use fire skills or stand by a portal — a soft halo
  around the bright effects; the frame counter Ctrl+Shift+F11 before and after (on a weak card the glow costs
  noticeably). Should the picture darken, flicker or effects vanish — turn the switch off and send a report.

# uaRO Graphics 0.99.0 — testing

- **Modern water in Direct3D 11.** The water with waves, reflections and clarity was Direct3D 12's alone; old cards
  (Radeon HD 5000/6000 and the like) showed the game's water. The "Modern water" switch now works in Direct3D 11 too —
  the same shaders, the waves', reflections' and clarity's sliders. No ripples from steps in D3D11 yet. The log has
  `water: the map's water found, drawn anew (Direct3D 11)`.
- How to check: choose Direct3D 11 on the Render page, turn on "Modern water", go to a map with water (Comodo's beach,
  Izlude) — waves and reflections as in D3D12; the frame counter Ctrl+Shift+F11 — what it costs on a weak card. Should
  the water turn black or vanish — turn the switch off and send a report.

# uaRO Graphics 0.98.0 — testing

- **Sun flare** (Effects page, off by default; Direct3D 12). Under the open sky in the day, when the sky shows towards
  the sun (up and to the left, against the way the shadows fall), a soft glow at the screen's edge and a few faint
  coloured rings on the line through the middle. None in dungeons, in towns without sky, or at night.
- How to check: turn it on, in a field turn the camera so that the sky past the map's edge shows at the top left —
  the glow and the rings come; cover the sky (turn towards houses or trees) — the flare goes. Should it be too bright
  — tell us.

# uaRO Graphics 0.97.0 — testing

- **Wet ground in the rain** (Effects page, off by default; Direct3D 12, needs "Weather" on). On maps where it rains
  the ground is a little darker, and here and there puddles mirror the houses, the trees and the characters. The
  puddles keep their places on the map; they do not travel with the camera.
- How to check: turn on "Weather" and "Wet ground in the rain", go to a map with rain — the ground is darker, the
  puddles reflect; maps without rain do not change.

# uaRO Graphics 0.96.0 — testing

- **Flash on a hit** (Effects page, off by default; Direct3D 12 and 11). A monster that is hit lights up white for a
  moment (0.14 s). The hit is known by the sound the game plays where it lands (a weapon's `_hit_...` or the
  monster's `..._damage`), so the game's effect sounds must be on. The log has `effects: a monster hit flashes`.
- How to check: turn it on, hit a monster — it flashes at every hit; monsters next to it do not. Should the wrong one
  flash or none — send a report and a screenshot.

# uaRO Graphics 0.95.0 — testing

- **Cards on the ground sparkle** (Effects page, off by default; Direct3D 12 and 11). A dropped card flashes a small
  golden star now and then (about every 2 seconds, each at its own moment). A card is known by its sprite on the
  ground (every card has the same one, "이름없는카드"); other items do not sparkle.
- How to check: turn it on, get a card to drop (or drop one of yours) — a star flashes over it; nothing over other
  items.

# uaRO Graphics 0.94.0 — testing

- **Ripples from steps** (Effects page, off by default; Direct3D 12, needs "Modern water" on). Where characters and
  monsters walk through water, rings of waves spread (the three newest steps). The steps are now followed without the
  characters' shadows too — footprints in snow and sand work with the shadows off as well.
- How to check: turn on "Modern water" and "Ripples from steps", walk through shallow water (Comodo's beach, say) —
  rings spread behind the character; on land nothing changes.

# uaRO Graphics 0.93.0 — testing

- **Fade in monsters and items** (Effects page, off by default). Monsters, characters, NPCs and items on the ground
  fade in over a quarter of a second where they come into view instead of popping up. Entering a map nothing fades —
  the map is shown at once. Direct3D 12 and 11.
- How to check: turn it on, walk across a field towards the screen's edge — new monsters fade in; turning the camera
  and running, the ones in view do not flicker. The log has `effects: sprites fade in as they come into view`; should
  it say `not this one`, the effect cannot work with this client — send a report.

# uaRO Graphics 0.92.0 — testing

What is new in 0.92.0 — sound (the Sound page, "Sound through uaRO Graphics", after the game is restarted):
- **Left and right.** The client changed only a sound's loudness by distance — a monster on the left was heard from
  both speakers. A sound beside the character now comes from that side of the screen (the camera's turn counted).
  The "Left and right" slider, 60 % by default, 0 = the game's. Should the sides be swapped — set
  `stereo_flip = 1` in `[sound]` of uaro-graphics.ini and tell us.
- **Never lose a new sound.** With every sound channel busy the client simply did not play a new sound. Now the
  quietest (the farthest) stops and gives its channel to the new one.
- **Smooth change of music.** Moving to another map the old piece fades out and the new one in over 1.5 seconds.

How to check:
1. Stand with a monster on your character's left and hit it: the sound is on the left; turn the camera — it moves.
2. Many skills in a crowd: your own hits are heard.
3. Take a portal to a map with other music: the change is smooth, not cut off.

# uaRO Graphics 0.91.1 — testing

- **3D sound without effects.** In 3D mode (setup.exe) the client plays the skills' and monsters' sounds only through
  the "Miles Fast 2D Positional Audio" provider and makes no ordinary sounds; without that provider only the music is
  heard. The driver now makes the ordinary sounds then and the effects play through them. The log's `sound: ...`
  line says which 3D providers there are and what the effects go through.
- Check: set 3D sound, enter the game — both the music and the hits/skills are heard; send a report (the `sound:` line).

# uaRO Graphics 0.91.0 — testing

What is new in 0.91.0 — sound (the Sound page, "Sound through uaRO Graphics", after the game is restarted):
- **Sound quality.** The client mixes its sound at 22050 Hz at most while the music is recorded at 44100 Hz — the highs
  were lost. The sound is now opened at 44100 Hz (or 48000, or the game's).
- **48 sounds at once** whatever setup.exe says (16 or 32 there): skills in a crowd are not cut short.
- **Volume in the background** — with another window in front or the game minimised, 30 % by default (100 = as it
  is, 0 = silence).
- **Even out the loudness in a crowd** — past six sounds at once each is a little quieter, so it does not crackle.
- It works through uaRO.exe's calls of Miles Sound System (`mss32.dll`); the videos (Bink) are untouched. The log
  has `sound: Miles opened at 44100 Hz (the game asked 22050), 48 sounds at once ...`.

How to check:
1. The music in a town sounds brighter than before (compare with "the game's").
2. Many skills in a crowd: no sounds missing, no crackling.
3. Minimise the game or switch to another window — quieter; back — as before.
4. Should the sound be gone or the game crash at start — set "Sound quality: the game's" and send a report.

# uaRO Graphics 0.90.3 — testing

- The Speed preset (the auto setup's for old and small cards) now turns the characters' shadows off too: in a crowd
  they took 5–10 ms of the card's time a frame even on an RTX with Direct3D 11. Check: on a weak computer press the
  auto setup — the characters' shadows are off on the Effects page.

# uaRO Graphics 0.90.2 — testing

- The fast cell under the mouse: in a new map's first 60 picks the client is still setting the map up and answers
  "no cell" with the mouse over the ground. Such disagreements no longer count towards turning it off (the map is
  just checked anew). Check: the log has no `pick: the game's own cell under the mouse from now on` line.

# uaRO Graphics 0.90.1 — testing

What is new in 0.90.1 — the fast cell under the mouse fixed:
- In 0.90.0 the fast pick turned itself off at its first check: the client does not count the ray's hits behind the
  camera (it drops the pieces of the map wholly behind it) and ours did. Now only hits in front of the camera count,
  as the client's.
- One disagreement no longer turns it off: the map is checked anew (240 picks); it is turned off after three. The
  log's line about a disagreement now has both answers' distances.

How to check: after entering a map the log has `pick: the cell under the mouse found the fast way on this map ... ms
a time, the game's own ... ms`; there should be no `pick: ... checked anew` or `from now on` lines — if there are,
send a report.

# uaRO Graphics 0.90.0 — testing

What is new in 0.90.0 — draws merged (Direct3D 11):
- The client draws almost everything as quads of their own: a sprite's layer, a piece of a window, a ground cell —
  each a call of its own. Draws one after another with the same textures and settings, their vertices one after
  another, now go to the driver as one call. The order and the triangles are the same; the picture should not change.
- The gain is the processor's time in the driver on weak computers with old drivers. The card's load does not change.
- The log line `Direct3D 11: N draws a frame in M draw calls ...`: N is the game's draws a frame, M the calls they
  became.
- Switch: `[debug] merge_draws = 0` in uaro-graphics.ini — each draw by itself, as in 0.89.
- Direct3D 12 is unchanged.

How to check:
1. With D3D11 walk around a town and a field, open windows, the inventory, use skills: nothing disappears or flickers,
   sprites and windows are where they belong.
2. Should something be drawn wrong — set `merge_draws = 0`, check whether it is gone, and send a report.
3. The frame counter Ctrl+Shift+F11 before and after; a report with a measurement (Diagnostics → "Measure where the
   game spends its time").

# uaRO Graphics 0.89.0 — testing

What is new in 0.89.0 — weak computers and the cell under the mouse:
- **The cell under the mouse (both renderers).** Every frame the client tested every cell of every piece of the map
  the mouse's ray passes over — thousands of cells when the camera looks far (14–15 % of the game's thread while
  loading and with the mouse far away, as measured). Now the ray goes from cell to cell in order and stops at the
  first one it hits. On each map the first 240 picks are checked against the client's own answer, then every
  120th; at the first disagreement only the client's own way is used until the game is restarted. Switch:
  `[debug] fast_pick = 0` in uaro-graphics.ini.
- **Direct3D 11: textures reused.** The game keeps deleting and creating small textures of the same sizes; an old
  driver takes long to create each. Textures up to 512×512 that the game lets go (32 MB at most) now go to its next
  texture of the same size.
- **Direct3D 11: statistics in the log** every half minute: draws and driver calls a frame (how many were spared), the
  processor's time in drawing, textures made/reused/freed, MB uploaded a second; the card's time for each effect
  (characters' shadows, torches' light, night, colour).
- **Blurry status icons on the right.** When the interface is not enlarged (for example a 1600×900 window without
  scaling), the AI pictures of the interface are not used any more: they were pictures four times larger, shrunk back
  with smoothing — the icons came out soft. The interface's AI pictures are used only with the interface enlarged
  (1.2× and up).

How to check:
1. Move the mouse over the ground far away (camera raised), walk by clicking: the cell cursor and the clicks go where
   they used to. The log has `pick: the cell under the mouse found the fast way on this map ... ms a time, the game's
   own ... ms`. Should `pick: the game's own cell under the mouse from now on` appear — send a report.
2. On a weak computer with D3D11: the `Direct3D 11: ... draws a frame` and `GPU: effects a frame` lines in the log;
   the frame counter Ctrl+Shift+F11 before and after the update; a report with a measurement (Diagnostics → "Measure
   where the game spends its time").
3. In a 1600×900 window without scaling the status icons on the right are as sharp as in the original client.

# uaRO Graphics 0.88.0 — testing

What is new in 0.88.0 - less processor work a frame:
- **Direct3D 11 (small cards such as the Radeon HD 6570).** Before each of the game's draws we set everything anew:
  blending, depth, rasterizer, samplers, textures, vertex layout, buffers and constants - about 12 calls even when
  nothing changed. Now only what differs from the previous draw is set, the constants are written only when they
  change, the vertex buffer is bound once. Old AMD drivers spend noticeable processor time on each call, and on D3D11
  that is the game's own thread.
- **Both renderers.** Setting render and texture stage states (the client does it tens of thousands of times a frame)
  no longer takes the global lock - about 1 ms a frame on a fast processor.
- Should the D3D11 picture come out wrong (wrong textures or blending), set `state_cache = 0` in the `[debug]` section
  of uaro-graphics.ini - everything is set every time as before; and send a report.

How to test: on a small computer with D3D11 - the same place (the prt_fild08 field, Prontera), the frame counter
Ctrl+Shift+F11 before and after updating; a report with the measurement (Diagnostics → "Measure where the game spends
its time").

# uaRO Graphics 0.87.1 — testing

Fixed by a tester's report (Radeon HD 6570, Direct3D 11):
- The Speed preset (which the auto setup also picks on small cards) no longer lowers the game's own texture quality
  (`TEXTUREMODE` = 1, textures at half size). The first game after it crashed within 8 seconds in the game's own sprite
  unpacking code (uaRO.exe 0x417B94). Every preset now keeps the game's textures and sprites at full size
  (`SPRITEMODE` = 2, `TEXTUREMODE` = 2) - our textures fit a 2 GB card, they are kept compressed.

How to test: on a small computer "Service" → "Update now", then "Renderer" → "Set up automatically" → "Apply", start
the game and enter a map.

# uaRO Graphics 0.87.0 — testing

What is new in 0.87.0:
- **Measuring where uaRO.exe spends its time** (Settings → Diagnostics → "Measure where the game spends its time";
  applies at once, in a running game too). About a thousand times a second the driver notes where the game's thread
  is and every 10 seconds writes into the log:
  - `profile: N samples in 10 s - uaRO.exe X%, ntdll.dll Y%, ...` - the time by module (the waits show too);
  - `profile: uaRO.exe's busiest places - 0x412cc0 7.1%, ...` - the client's own busiest places;
  - `profile: called from uaRO.exe - 0x40e0d0 into ddraw.dll 5.0%, ...` - where the client goes into other modules.
  It slows the game a little - turn it off after measuring.

How to test: turn it on, play a minute in a town with people, change a couple of maps (loading), fight monsters with
skills; turn it off and send the log. The addresses are matched to the client's decompilation.

# uaRO Graphics 0.86.0 — testing

What is new in 0.86.0 (Direct3D 12) - less load:
- **Floor reflections at half the size**: a quarter of the rays, then laid over the picture smoothly (on a floor a
  reflection is soft anyway). Log: `floor reflections: on <map>, at WxH`; the cost - `floor reflections` in the
  `GPU: effects a frame` line.
- **The driver's processor time** every half minute: `CPU: N ms a frame in the driver's drawing` - what the processor
  spends on the drawing and the effects (not the waits).
- The AI textures on D3D12 already go out of video memory when it runs short (since 0.44); the compression stays on
  D3D11, where memory is small.

How to test: Prontera's church or Glast Heim - the reflections in place, not smeared on the walls; send a minute of the
game's log (the GPU and CPU lines).

# uaRO Graphics 0.85.0 — testing

What is new in 0.85.0 - less load:
- **60 frames by default** (Frames → "fps_limit"). Before, the game drew as many as it could (140-150 on the
  developer's), though it looks the same. The limiter is exact now: a steady 60 without stutter (Sleep woke up to
  16 ms late). **An installed game** has `fps_limit = 0` in uaro-graphics.ini - set 60 on the Frames page or press the
  auto setup.
- **Saving power in the background** (Frames → "Save power in the background"; on by default): while another window
  is in front or the game is minimised - 15 frames; on the login and the character select - 30.
- **The auto setup and the presets** no longer set render_scale over 100 % (150 % at 4K is 2.25 times the pixels for
  slightly smoother edges); every preset sets 60 frames and the power saving; the Speed preset turns the costly effects
  off (soft shadows, depth of field, glow, clouds, heat haze, floor reflections and more).

How to test: the frame counter Ctrl+Shift+F11 - a steady 60; switch to another window and back; the log's
`GPU: ... ms of work a frame` over a minute - compare with before.

# uaRO Graphics 0.84.0 — testing

The picture by default is as before 0.77 ("the lighting effects are too much"): off by default now - the shadows of
houses and trees, the bloom of bright places, film-like colours, the regions' colours, footprints and dust,
thunderstorms, the skills' sparks, the smooth map change and the light in the air (the halo about fire and skills). The
round characters (0.79) are apart from the rim light - their own box "Round characters", off as well. The code stays:
any of them comes back with its box on the Effects page.

One turned on by hand in the settings app is written in uaro-graphics.ini and stays on - clear its box.

How to test: the same square - it should look as before 0.77 (no big shadows on the tiles, no halo by Blessing).

# uaRO Graphics 0.83.1 — testing

Fixed by the square's screenshot (0.83.0): the sunlit tiles went nearly white.
- **Bloom of bright places** glowed from any light surface (threshold 0.72) and strongly - a light square glowed whole.
  Now only what is near white (0.88), 2.3 times weaker.
- **Ground relief** brightened the stones' sides facing the light up to x1.3 - light tiles went white. Now at most
  x1.1, the relief mostly from the shading.

How to test: the same square by day - the tiles light but with the stones' pattern visible; Pause - compare.

# uaRO Graphics 0.83.0 — testing

What is new in 0.83.0 (Direct3D 12):
- **Smooth map change** (Effects; on by default). After a warp or entering a new map the world comes in from black
  over 0.7 seconds; the interface shows at once.
- **Film grain** (Effects → "Film grain" and strength; off by default). A fine noise over the picture as on film,
  weaker in the bright parts.

How to test: several warps - any black flashes or the world staying dark; the grain - turn it on, strength 30-60.

# uaRO Graphics 0.82.0 — testing

What is new in 0.82.0 (Direct3D 12):
- **Sparks of skills** (Effects → "Sparks of skills" and their brightness; on by default; needs the skills' light).
  Embers rise from burning skills (Fire Wall, Fire Bolt, Meteor Storm), swaying and dying out; glints of their colour
  spread from the other bright skills (Soul Strike, Heal, lightning).

How to test: Fire Wall and Meteor Storm, Soul Strike, Heal - are the sparks visible, too many or too bright; do sparks
hang where the skill is gone.

# uaRO Graphics 0.81.0 — testing

What is new in 0.81.0 (Direct3D 12):
- **Grass** (Effects → "Grass" and how thick; **off** by default - the costliest effect). Blades of grass grow on the
  green ground of the fields and towns: they stay put as the camera moves, sway in the wind and lie down where
  characters just walked (with "Footprints and dust"). Characters stand over the grass. Log: `grass: on <map>, blades
  up to N pixels`; the cost - `grass` in the `GPU: effects a frame` line.

How to test: turn it on, a Prontera or Payon field; does it shimmer, does grass grow on green walls and roofs, what it
costs (the frame counter).

# uaRO Graphics 0.80.0 — testing

What is new in 0.80.0 (Direct3D 12):
- **Footprints and dust** (Effects → "Footprints and dust"; on by default; needs the characters' shadows). Characters
  and monsters leave footprints in the snow (Lutie and other snowy maps, ~25 s) and in the sand (Morroc, ~14 s), left
  and right by turns; on dry fields little puffs of dust rise under the feet. Log: `footprints: in the snow on xmas`.
- **Thunderstorms** (Effects → "Thunderstorms"; on by default). Where it rains (Niflheim, Einbroch, with the weather
  on), lightning flashes now and then - two flashes in a row, the picture lit for a moment.

How to test: run through Lutie and Morroc's fields - are the prints visible, do any hang on walls; a Prontera field -
the dust; Niflheim - lightning about every 10-20 seconds.

# uaRO Graphics 0.79.0 — testing

What is new in 0.79.0:
- **Round characters (HD-2D)** - with "Rim light" (Direct3D 12 and 11). Torches, lamps and fire light a character or a
  monster as if it were round: the side towards the fire brighter, the far one darker, smoothly across the sprite's
  width instead of evenly.
- **Colours of each region** (Effects → "Colours of each region" and strength; on by default; D3D12 and D3D11). Payon
  warm and green, Niflheim cold and pale, Morroc sun-bleached, Lutie blue, Glast Heim grim, Einbroch smoky, and a dozen
  more. Log: `region: Payon (pay_fild01)`.

How to test: a torch next to a character (Payon's caves) - walk past the fire; travel the towns: is the colour too much
(strength 0-100), which regions want changing.

# uaRO Graphics 0.78.0 — testing

What is new in 0.78.0 (Direct3D 12):
- **Bloom of bright places** (Effects → "Bloom of bright places" and its strength; on by default, 60 %). The brightest
  parts of the picture - sunlit tiles, snow, windows, fire - glow softly around (before, only the effects glowed).
- **Film-like colours** (Effects; on by default). What is bright (with the bloom) rolls off softly into white instead
  of clipping into a flat patch; a light contrast, the middle tones stay as they were.
- In the `GPU: effects a frame` line - `bloom`.

How to test: Lutie (snow), Morroc by day, fires at night; is it too soft or bright; Pause - compare; if the picture got
darker or more contrasty than you like, turn "Film-like colours" off and tell.

# uaRO Graphics 0.77.0 — testing

What is new in 0.77.0 (Direct3D 12):
- **Shadows of houses and trees** (Effects → "Shadows of houses and trees" and their darkness; on by default, 60 %).
  On the fields and in the towns houses, walls and trees cast shadows on the ground away from the sun (as the
  characters' do). With "Time of day" they turn and lengthen towards the evening; at night there are none. Dungeons and
  houses are unchanged. Log: `sun's shadows: over <map>`; the cost - `sun's shadows` in the `GPU: effects a frame` line.

How to test: Prontera, Payon (trees), Alberta; do they double with the map's baked shadows, are there stripes or noise
on the ground; turn the camera - the shadows must stay put by the houses.

# uaRO Graphics 0.76.0 — testing

What is new in 0.76.0:
- **Keep the frame rate** (Settings → Frames → "Keep the frame rate", 0 = off). When the game runs slower than this
  for 10 seconds (under 90 % of it), the costliest effect is turned off for the rest of the game, ten seconds later
  the next if needed. On D3D12 by the graphics card's measures, on D3D11 by a list. The settings file is not changed;
  restarting the game brings everything back. Log: `keep_fps: 38 fps, under 45 - effects.ao off for this game (it cost
  1.20 ms)`. The auto setup sets 45 on small and middle cards.
- **What the effects cost, in the settings app.** The game (D3D12) writes every half minute how many milliseconds each
  effect cost (`uaro-graphics\effect-costs.txt`), and the Effects page shows it next to the box: "Reflections on the
  floor · ~0.8 ms". The sky, the light in the air and the floor reflections now have their own measures in the log
  (`GPU: effects a frame`).
- **Shader safety.** If the shader compiler on someone's computer will not take the newest parts of the main shader
  (soft effects, ground relief), the driver builds it without them instead of showing no picture. Log:
  `shader draw_ps: compiled without the newest features ...` - send that line if you see it.

How to test: set "Keep the frame rate" well above your frame rate (say 500), play a minute - the log turns effects
off one by one; set 0 - they come back. After the game open the Effects page - the numbers by the boxes.

# uaRO Graphics 0.75.0 — testing

What is new in 0.75.0 (Direct3D 12):
- **TAA for any graphics card** (Settings → Smoothing → "TAA (any graphics card)", off by default). The renderer's own
  temporal anti-aliasing for cards without DLSS (AMD, Intel): the world is drawn with a sub-pixel jitter and each frame
  is blended with the last along the camera's motion - edges, fences, grass and thin lines stop shimmering. The history
  is held within the colours of the pixels around, so moving characters leave no trail. DLSS goes first; MSAA and
  render_scale are not used with TAA. Applies at once. The auto setup turns it on for strong and middle cards that are
  not NVIDIA's (middle ones up to 2560x1440). Log: `TAA: on, WxH`, `TAA: the game's camera found`.

How to test: turn TAA on (an NVIDIA card can too, with DLSS off), stand and walk on a field with a fence and grass,
turn the camera; are there trails behind the characters, a shaking picture, blurred text (the interface is not
smoothed). The frame counter - what it costs.

# uaRO Graphics 0.74.0 — testing

What is new in 0.74.0 (Direct3D 12 and Direct3D 11):
- **Time of day** (Effects → "Time of day", off by default). By the computer's clock the sun moves: it rises on the
  right at 6:00 and sets on the left by 20:00. The characters' shadows turn with it - in the morning they fall left, in
  the evening right, long in the morning and the evening, short at noon. The light is pinkish in the morning, white by
  day, golden in the evening. With "Night by the clock" the evening turns into night. The rays from the sky (0.73) come
  from the sun.

How to test: turn it on, look at the characters' shadows and the colour (a field by day); set the Windows clock to
7:00, 13:00, 19:00 and re-enter the map - do the shadows and the colour change.

# uaRO Graphics 0.73.0 — testing

What is new in 0.73.0 (Direct3D 12):
- **Light in the air** (Effects → "Light in the air" and its strength; on by default). Around torches, lamps and
  burning skills the air glows as in a light mist; a wall or a pillar in front of the fire hides the glow. At night it
  shows more. On the fields and in the towns, where the edge of the map shows, rays of light stream from the sky - from
  the sun, up and to the left.

How to test: caves with torches (anthell, pay_dun), Geffen at night, Fire Wall in the dark; the edge of a field for the
rays. Is it too bright, does it flicker.

# uaRO Graphics 0.72.0 — testing

What is new in 0.72.0 (Direct3D 12):
- **Reflections on the floor** (Effects → "Reflections on the floor" and the floor's shine; on by default, shine 35 %).
  Indoors - dungeons, castles, houses - and in the rain (Niflheim, Einbroch) the floor reflects the characters, the
  monsters and what stands on it; more in the rain. Dry fields and towns do not shine. Log:
  `floor reflections: on <map>`.

How to test: Glast Heim (gl_church, gl_cas01), Prontera's church (prt_church), Niflheim; does it slow down (the frame
counter Ctrl+Shift+F11), are there doubled stripes on the walls.

# uaRO Graphics 0.71.0 — testing

What is new in 0.71.0 (Direct3D 12 and Direct3D 11):
- **Ground relief** (Effects → "Ground relief" and its strength; on by default). The brightness of the ground's
  picture is taken as height: cracks, stones and grass are lit from the side (light from the screen's upper left) and
  look raised. Best with the AI textures - they have more detail. Walls and models are unchanged.

How to test: Prontera's road and tiles, Morroc's sand, a dungeon's stone floor; strength 0-200 - where it starts to
look dirty. Pause - compare.

# uaRO Graphics 0.70.0 — testing

What is new in 0.70.0 (Direct3D 12):
- **Motes in the air** (Effects → "Motes in the air" and how many; on by default). Where the map has no weather of its
  own, tiny motes drift slowly in the air, now catching the light, now not: pollen over the fields and the towns (less
  of it at night), dust in the dungeons and the houses. Log: `weather: pollen over <map>` or `dust`.

How to test: Prontera and its fields by day, any dungeon; are the motes in the way, too many (the slider).

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
