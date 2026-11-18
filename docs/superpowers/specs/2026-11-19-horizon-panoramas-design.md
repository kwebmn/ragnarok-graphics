# Horizon panoramas in the 3D view - design

## Intent

The owner saw an AI-retouched frame of our 3D view (the YouTube preview of «Ragnarok Online in 3D»: snowy peaks,
pine forests, a wide sky) and wants the game itself to look like that: «я хочу панорамы горизонта в клиенте
красивые», «я хочу в клиенте сделать, делай». It is for the players and for the promotion the owner is starting on
YouTube and RateMyServer - the horizon is what a 3D-view video or screenshot shows most.

Today the 3D view's horizon is procedural: `sky3d_ps` / `skyAlong` (src/win/gpu/shaders.cpp) paint the sky where
nothing is drawn, and `skyRanges` adds three layers of noise crests (view3d::Ranges: mountains, dunes, snowy peaks,
low islands) - flat, pale silhouettes. Past the map's edge Direct3D 12 also draws land with hills
(core/outskirts, D3D12 only). Both renderers draw the sky (haze.cpp skyPass, renderer11 applySky3d).

## Decided by the owner (19.11)

- Painted horizons in the client, not only in videos.
- Approach A of three (below): a painted 360° panorama per region, drawn by the sky shader.

## Approaches

| | What | Trade-off |
|---|---|---|
| **A (picked)** | A painted 360° strip per region, sampled by the sky shader by the view's direction, under our live sky (clouds, sun, stars stay ours), tinted by the time of day, its foot into the haze | Most of the look for the least work; both renderers; nearly free on the card. No parallax - but far mountains barely move in life either, and D3D12's outskirts hills stay in front |
| B | Grow the outskirts into real 3D mountains (rock and snow by slope and height), port outskirts to Direct3D 11 | Honest parallax and sunlight, but procedural terrain rarely looks «like the picture»; long; heavy for weak cards |
| C | Two or three rings of painted silhouettes at different distances (theatre flats) | Real depth, but 2-3x the art and geometry code in both renderers; can come later on top of A |

## Goals

- On every outdoor map of a known region (core/region_look's 20: Alberta, Amatsu, Ayothaya, Comodo, Einbroch, Geffen,
  Glast Heim, Hugel, Juno, Kunlun, Lighthalzen, Louyang, Lutie, Morroc, Niflheim, Payon, Prontera, Rachel, Umbala,
  Veins) the 3D view shows that region's painted horizon all round, fixed to the world's directions.
- Our sky stays alive above it: clouds, the sun, the stars, the moon and the dawn glow keep working; the sun sets
  behind the mountains.
- The time of day reaches the panorama: by night dark silhouettes under the stars, at dawn and dusk a warm rim on the
  sun's side, in any weather its foot pales into the haze (the same horizon colour `view3d::skyOf` gives).
- Direct3D 11 and Direct3D 12 alike (the R7 M370 laptop players run D3D11).
- No measurable cost: one texture sample per sky pixel.
- A map with no panorama (unknown region, a missing file) keeps today's procedural ranges.

## Non-goals (this design)

- Parallax layers (C), real mountain meshes (B).
- Indoors, dungeons (the vault), maps hung in the sky (view3d::hungMap) - none of them see a horizon.
- The classic view from above (no horizon in sight).
- Seasonal variants (a winter Prontera) - later, the same files with a suffix.
- The water's reflection of the panorama - v1 may keep the procedural ranges there (builder's call, below).

## Anti-goals

- A photo pasted behind a painted game: the art must match the game's painted look (the owner judges each region).
- A heavier update for every player on every beta: the panoramas travel in their own update part (below), not in
  the main package.
- Showing players what the game does not draw: card and video images are taken from the game with this on.

## Constraints

- Shaders compile with FXC on players' PCs: check with `dxc -HV 2018` as the project's rule says.
- Direct3D 11 feature level 11_0: textures up to 8192 a side are fine (16384 max).
- New visual effects ship OFF by default (the owner's restrained-look rule) - see Open questions.
- Settings: [view3d] section, Alt+O «3D-режим» page, strings in uk/ru/en; the D3D12 renderer is a separate process
  (uaro-render64.exe): a new effects field goes over the bridge (remote.cpp writes, serve.cpp reads, protocol
  version bumped) - as `skyRanges` (3.0.20-beta.72) did.
- Each change a beta (3.0.22-beta.N); builds `nice -n 19 ionice -c3`, `-j4`.

## The design

### The panorama file

- One per region: `uaro-graphics\horizon\<region>.webp`, region = core/region_look's name in lower case, spaces as
  `-` (`prontera.webp`, `glast-heim.webp`).
- 8192 x 512 texels, ARGB, alpha 0 where the sky is (our sky shows through), 1 on the land and mountains, a soft edge
  of 1-2 texels on the skyline.
- Horizontal: the full 360°, u = 0 at the world's +x way, growing toward +z (fixed in core, tested; the art is
  placed by the tool, so any convention works if it is one).
- Vertical: «rise» (the sine of the elevation, as `skyAlong` uses it: rise = -dir.y) from kRiseLow = -0.05 at the
  bottom row to kRiseHigh = 0.35 at the top row. 8192 / 360° ≈ 22.8 texels a degree; 512 rows over ~23° ≈ 22.5 a
  degree - square texels; at 1080p and a 70° field of view about one texel a screen pixel.
- About 0.3-0.6 MB a file as lossy WebP with alpha.

### Choosing and loading

- core: `horizon::fileOf(region)` -> the file name or "" (unknown region); `horizon::wanted(on, outdoors, hung,
  vault)`; the u/v of a direction and the level of detail (`horizon::lodOf(textureWidth, focalPixels)`) - all tested.
- At a map's start the renderer asks region_look for the map's region; if the file exists it is decoded (core
  webp::decode) off the render thread (MapWorld's thread on D3D12, a worker on D3D11), its mip levels made on the
  CPU, then uploaded once; until then and on failure the procedural ranges show. The texture of the previous region is
  kept while the next region is the same (no reload between Prontera's fields).
- Log: «horizon: <map> - the <region> panorama (WxH, N KB, decoded in N ms)» / «... none for <region>: the far
  ranges» / «... could not be read: <why>».

### Drawing

- The sky passes bind the panorama to t1 (tex1) under the open sky - t1 is used only under a vault (the dungeon's
  heights), which never coexists with an open sky. Direct3D 12: haze.cpp skyPass passes it where it passes vaultT
  today; Direct3D 11: renderer11 applySky3d likewise.
- `skyParts` gains a bit (128): «a panorama is bound». `skyAlong` then composites the panorama where `skyRanges`
  went: after the gradient, the sun, the clouds, the stars and the moon, so mountains stand in front of them; the
  ranges are not drawn when the panorama is.
- Sampling: `SampleLevel` with the level from `lodOf` (the frame's focal length in pixels x the scale) - no derivatives
  at the u seam; u wraps (a wrap sampler on U, or the shader's own `frac` and a clamp sampler with the seam's last
  texel duplicated by the tool), v clamps.
- Colour: the panorama's texel x a light tint by the night (day 1 -> night a dark blue silhouette ~0.10-0.15 of it),
  plus the dawn/dusk glow on the sun's side (the factor skyAlong already computes), then a lerp toward the horizon
  colour by height - the foot ~45 % haze, the top ~15 % (aerial perspective; numbers tuned on the owner's shots).
- The haze pass (`sky3d_haze_ps`) takes the same `skyAlong`, so far land fades into the mountains behind it.

### The setting

- `[view3d] panoramas` - «Панорами обрію» / «Панорамы горизонта» / «Horizon panoramas», a hint saying what it does and
  that a region without its picture keeps the far ranges. Default: off (the rule) unless the owner says otherwise.
- A change applies at once (live config), the texture loads at the next look at the sky.

### Delivery: the «horizon» update part

- Like DLSS and the AI models (app/update_run runPart): `dist/horizon-version.txt`, a zip
  `release/uaro-graphics-horizon.zip` holding `uaro-graphics/horizon/*.webp`, a `[horizon]` section in update.ini
  (tools/release_ini.py `--horizon`), `update::Manifest::horizon`, fetched by every install when its version differs
  (no NVIDIA/AI condition: it is small and anyone may turn the 3D view on).
- The ship scripts (gship / ship-beta.sh) build and upload that zip when its version changed.

### The art pipeline - tools/horizon_pack.py

- Input per region: one picture - best a 360° equirectangular panorama (2:1, seamless), else any wide landscape; a
  line in `tools/horizon_regions.json` says the file, where its horizon row is, a yaw (which part faces the world's
  +x), the crop.
- Steps (Pillow only, no numpy on the build machine): take the band from rise -0.05 to 0.35 (an equirectangular row
  by its elevation), scale to 8192 x 512; cut the sky - the picture's own alpha if it has one, else a skyline per
  column found from the top (the sky's colour sampled at the top rows; a pixel stops being sky when its distance from
  that colour and its local contrast pass thresholds), smoothed along the columns, a 2-texel soft edge; a wide
  picture that is not 360° gets its ends cross-faded (seam) over 4 %; the bottom rows faded toward transparent so the
  haze takes over; written as WebP (quality ~85, alpha) + a preview PNG with the skyline drawn for checking.
- `--test` makes a procedural test panorama (ridged noise mountains, painted shading) - no licence questions, for the
  first beta's check before the owner's art exists.
- Prompts for the owner's AI generator (one per region, style matching the game's painted look) are written in the
  second phase; the owner generates, the tool packs, the owner judges each region in the game.

## Phases (betas)

1. Engine on both renderers + core + setting + the horizon part + tools/horizon_pack.py with `--test`; the test
   panorama for Prontera ships in the part.
2. Prompts and the first six regions from the owner's art: Prontera, Payon, Geffen, Morroc, Alberta, Lutie.
3. The other fourteen regions.

## Left to the builder

- The exact tint and haze numbers (tuned against the owner's screenshots), the skyline thresholds.
- Wrap sampler vs `frac` + duplicated seam texel.
- Whether the water's reflection takes the panorama in v1 (it needs t1 free in the water pass) or keeps the ranges.
- Where on D3D11 the decode runs (a worker like the AI textures' or the map world's).

## Open questions (the owner's)

- On by default for the 3D view, or off as the rule says?
- The art's style: painted to match the game (my pick) or photo-like as the AI preview?
- Who generates the art - the owner in his AI tool from my prompts (my assumption), or something else?
