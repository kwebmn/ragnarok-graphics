# Wings on characters - design

Concept page (the owner's): https://claude.ai/artifact/S1rYPnLYM9yByk6wz5FTok («Крылья на персонажах»).

## Intent

Big wings on the back of every character who wears one of nine rare headgears, drawn by uaRO Graphics and seen by
everyone who has it. The server and the game's data stay as they are; the wings are a reward the GM hands out.

## Decided by the owner (12-13.11)

- Our driver draws the wings; the server is not touched (no new items, no bonuses).
- Variant «б»: wings for whoever wears a chosen item, seen by every player with uaRO Graphics.
- A full art set, not a test look. Variant А of the distribution: only GM-only items carry wings.
- The item table of the concept stays; looks drawn by new art (fairy, mechanical) are not made; the extra looks are
  made only where a recolour does it (valkyrie, gold, sakura, crimson).
- Other players' wings always show: no switch to hide them.

## Looks and items

| Look | Garment set (uaRO HD data.grf) | Recolour | Item (view the server sends) |
|---|---|---|---|
| angel | 로브\천사날개 (Angel Wings) | - | Angel Blessing 5207 (view 260) |
| fallen | 로브\타락천사의날개 (Wings of Fallen Angel) | - | Helm of Darkness 5181 (view 233) |
| demon | robe\darkwing (Dark Wing, one .act for every job) | - | Wings of Diablo 5044, Refined 5328 (view 129) |
| fire | Angel Wings | fire gradient | Phoenix Crown 5152, 5271 (view 221) |
| ice | Angel Wings | ice gradient | Soul Wing 5297, Soul Ring 5257 (view 301) |
| valkyrie | Angel Wings | white-gold | Valkyrie Feather Band 5256 (view 300) |
| gold | Angel Wings | gold | Emperor's Laurel Crown 5205, Sacred Palma 5320, Emperor Wreath 5533 (view 261) |
| sakura | Angel Wings | pink | Flower Crown 5303 (view 325) |
| crimson | Dark Wing | crimson | Red Deviruchi Hat 5227 (view 271) |

A recolour maps the used palette colours by brightness onto a gradient (the concept page's stops); every frame,
side and movement of the set is kept.

3.0.18-beta.232 (the owner's «давай»): the items changed. .230's (views 365, 382, 613, 595, 568, 477, 438, 562) were
past the 2007 client's headgear names (about 327, «_화관» = 325 the last known): the game read another table's word for
them and showed «Cannot find File» to every plain client near the wearer (3.0.18-beta.231 stops that for uaRO
Graphics players alone). The views now are ones it names; every item is the GM's only and shares its view with no
item a player can get.

## Facts it rests on (uaRO.exe, decomp /root/BornRok/winEXE/decomp/uaRO_decomp.c)

- Player actors' vtables 0x69F604, 0x69F778, 0x69F8F8 (the headgear setters FUN_005ac140/1d0/260 at +0x54/58/5C).
- A player's headgear views at +0x36C, +0x370, +0x374 (the server's look packet 0x1D7 cases 3/4/5 for everyone);
  sex at +0x210; action (0..103, 8 sides each) at +0x24, frame at +0x28; size at +0x3C.
- Its layers' resources: +0x3C4 (sprites) and +0x3B4 (acts) point to 8 resource pointers each, layer 0 the body; a
  resource's name is a C string at +0x14 ("data\sprite\인간족\몸통\<sex>\<body>.spr"). The garment files are named after
  the body: 로브\<set>\<sex>\<body>.
- The game asks for a character's shadow (FUN_00547B80, hooked: win/effect_hook actorShadow) right before it sorts
  the character's sprite faces; the faces sorted after it are the character's (effect_exe::Owner).
- A character's screen point: FUN_00412C30 (thiscall on *(0x6EE3D8); pos = actor+4, matrix, &x, &y, &depth), its
  depth scale FUN_00412D60 (thiscall on the same, depth) - the player render FUN_005AE540 does exactly that; a layer's
  quad there is (left + anchor offset) * layer scale * size * depth scale + point.
- A garment frame sits on the body by the body frame's anchor minus the garment frame's anchor, both at the same
  action and frame (as the concept's renders, from rosprite.py, showed on four jobs and every pose).

## How it is drawn

- At a player's shadow: read the three views; none of the nine - nothing. Else read the body's name, action, frame,
  size; ask the game for the screen point and depth scale with the shadow's matrix; note a wing job for the actor.
- The faces sorted after it (until the next shadow) are counted for the job.
- At the draw of the job's first face (facing us: sides 0, 1, 2, 6, 7) the wing quads go right before it; facing away
  (3, 4, 5) right after its last face. Their colour (alpha, tint, fog's specular) and draw setup are that face's - a
  wall in front hides them as it hides the body; a cloaked character's alpha passes on; a hidden one has no faces, no
  wings.
- 3.0.18-beta.237 (R-FAFJWS «мигает тело персонажа»): their depth is not the first face's. FUN_005AECC0 gives each
  layer its flags: the body and the head 0x101 - the list +0xCC, drawn writing the depth (ZWRITE 1, ALPHAREF 1),
  sorted by the top vertex's rhw, so the head now and then comes first; layers 2-4 flags 1 (list +0xDC, ZWRITE 0);
  a layer whose frames are a multiple of the head's, or layer 4 under top headgear view 185, flags 0 (the opaque
  list, drawn before the ground); +0xD4 bits give 5 (the per-face list). With the head's depth the wings stood nearer
  than the body and hid it, flipping frame by frame. Now every face sorted during the render counts (the opaque
  list's for the depth alone): facing us the wings go 0.5 view units behind the farthest face, facing away 0.5
  nearer than the nearest (the game's own step between layers is 0.81 at the usual zoom; its sprites stand 4.7 off
  the ground). z of an rhw as the game's (FUN_00412E30: far/(far-near)*(1-near*rhw), near 10, far 1500).
- The wing frames are the driver's own textures (Renderer::createTexture + FrameRecord::uploadOwn), one per frame
  image and look, made once.
- Off in «Пауза» (the effects off), as the driver's other drawings. No switch: they always show.

## Files

- The sets come from the uaRO HD client (its data.grf, read with /root/uagfx-tools/hdgrf). tools/wings_pack.py packs
  them into resources/wings/wings.grf: per set its .spr and .act files by body name, identical files stored once (an
  index file maps names to contents). The package installs it as uaro-graphics\wings.grf; the driver reads it with
  core grf::Archive. The body .act files come from the game (win/game_files).

## Not now

- The glow of fire, ice and gold (the concept: a switch, off by default) - later if asked.
- Fairy and mechanical (new art), garments (the server never tells others what garment one wears).

## Risks

- The GM tries each item on a test character in the classic client first: views past ~330 have no name in its table
  (nobody wore them before).
- Faces counted after a shadow may include an effect's own: they are tagged as effects (effect_exe), not the actor's.
