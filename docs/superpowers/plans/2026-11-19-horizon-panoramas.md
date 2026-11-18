# Horizon panoramas (card-drawn) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** In the 3D view the card draws a 360° procedural landscape for the map's region (hills, alps, desert, sea,
karst, canyon, wasteland) behind the world, lit every frame by the time of day.

**Architecture:** core holds the climates (one per core/region_look region), the strip's geometry and a CPU twin of
the terrain and material functions (tests + a host preview tool). The shaders mirror that twin: `horizon_bake_ps`
ray-marches the terrain into two RGBA8 strip textures (colour+coverage, normal+shadow+haze) a slice per frame; the sky
passes sample them in `skyAlong` and light them by the night, the sun and the haze. Direct3D 12 (uaro-render64) and
Direct3D 11 (Renderer11) each own their strip, bake pass and binding; the setting crosses the bridge.

**Tech Stack:** C++17 (MinGW for Windows, g++ host tests), HLSL SM 5.0 compiled by FXC on players' PCs, Direct3D 11/12.

**Spec:** docs/superpowers/specs/2026-11-19-horizon-panoramas-design.md

## Global Constraints

- Drawn by the card from formulas: no picture files, no AI art.
- On by default: `[view3d] panorama = 1` («сразу включать»).
- Both renderers; a weak level = `effectgroups::cardLevel(d3d12, vramMb) <= 1` (Direct3D 11 or under 4 GB).
- Strip: 360° wide, rise from kRiseLow = -0.05 (bottom row) to kRiseHigh = 0.35 (top row); 4096 x 384 (weak 2048 x
  192); 128 ray steps (weak 64); 24 shadow steps (weak 0); baked over 8 frames (weak 16).
- Shaders pass `dxc -HV 2018` (the FXC rule); loops `[loop]` with constant bounds.
- No new picture files in the package; no frame-rate cost the player feels; no hitch at a map's entry.
- Each shipped step a beta 3.0.22-beta.N via gship; builds `nice -n 19 ionice -c3`, `-j4`; lines in src/tests <= 120.
- Commits by «KWEB Team <kwebmn@users.noreply.github.com>» with the Co-Authored-By trailer.

## Review Focus

- A map change while a bake is half done: the old strip must not show with the new map's climate (fade restarts).
- A device lost / recreated (D3D12 renderer restart, D3D11 reset): the strip and its bake state rebuilt, no stale SRV.
- The water's reflection must keep the procedural ranges (the panorama bit only in the sky passes).
- Turning `[view3d] panorama` off live, or leaving the 3D view: the ranges come back at once, the bake stops.
- A dungeon / house / hung map: no bake at all (wanted() false), t0/t1 keep their vault use.

---

### Task 1: core climates and the strip's geometry

**Files:**
- Create: `src/core/horizon.hpp`, `src/core/horizon.cpp`
- Test: `tests/test_horizon.cpp`

**Interfaces:**
- Produces:
  - `namespace uagfx::horizon`
  - `enum class Land : uint8_t { Hills, Alps, Desert, Sea, Karst, Canyon, Waste };`
  - `struct Rgb { float r, g, b; };`
  - `struct Climate { Land land; uint32_t seed; float hillAmp, hillFreq, mountAmp, mountFreq, mountStart, mountFull,
    snowLine, treeLine, haze; Rgb grass, forest, rock, snow, sand; };` (distances in world units from the map's
    middle, heights in units up, haze per unit)
  - `Climate climateOf(const std::string& region);` - region_look's names; unknown or "" -> the default (Prontera's)
  - `constexpr float kRiseLow = -0.05f, kRiseHigh = 0.35f;`
  - `struct Level { int width, height, steps, shadowSteps, slices; };` `Level levelFor(bool weak);`
  - `float uOf(float x, float z);` - [0,1): atan2(z, x) / 2π wrapped (u 0 at +x, 0.25 at +z)
  - `float riseOfRow(float v);` `float rowOfRise(float rise);` - v 0 the top row, 1 the bottom
  - `bool wanted(bool on, bool wideView, bool outdoors, bool hung);`
  - `struct Key { std::string region; float sun[3]; bool weak; float mapRadius; bool operator==(const Key&) const; };`
    - what a bake depends on (sun compared within 1e-3)
  - `constexpr int kPackedFloats = 44; void pack(const Climate& c, float mapRadius, const float sun[3], const Level& l,
    int row0, int row1, float out[kPackedFloats]);` - index layout written as a table in horizon.hpp

- [ ] **Step 1: Write the failing tests** `horizon_climates_by_region`, `horizon_strip_geometry`, `horizon_wanted`,
  `horizon_key_and_pack`:
  - `climateOf("Prontera").land == Land::Hills`, `"Lutie"` Alps, `"Morroc"` Desert, `"Alberta"` and `"Comodo"` Sea,
    `"Louyang"` Karst, `"Veins"` and `"Rachel"` Canyon, `"Niflheim"` and `"Glast Heim"` Waste; `climateOf("")` equals
    `climateOf("Prontera")` field by field; the 20 region names give 20 different seeds.
  - `levelFor(false)` == {4096, 384, 128, 24, 8}; `levelFor(true)` == {2048, 192, 64, 0, 16}.
  - `uOf(1,0) == 0`, `uOf(0,1) == 0.25`, `uOf(-1,0) == 0.5`, `uOf(0,-1) == 0.75` (±1e-5);
    `rowOfRise(0.35) == 0`, `rowOfRise(-0.05) == 1`, `riseOfRow(0.5) == 0.15f` (±1e-5).
  - `wanted(true,true,true,false)` true; any of on/wideView/outdoors false or hung true -> false.
  - Key equality ignores sun differences under 1e-3; `pack` puts land at [0], seed at [1], row0/row1 at [36]/[37],
    the level's width/height at [32]/[33], sand at [38..40] (Ruling 19.11: 44 floats - sand needed f0..farZ).
- [ ] **Step 2:** `./build-host/uagfx_tests horizon` -> FAIL (does not compile: no core/horizon.hpp).
- [ ] **Step 3: Implement** horizon.hpp/.cpp. Climate numbers start from the mock-up's (Prontera: hillAmp 170,
  hillFreq 1/1400, mountAmp 4600, mountFreq 1/7600, mountStart 3000, mountFull 9000, snowLine 1500, treeLine 1300,
  haze 2.6e-5, grass (0.30,0.46,0.14), forest (0.08,0.19,0.06), rock (0.45,0.41,0.36), snow (0.92,0.94,0.98), sand
  (0.76,0.66,0.46)); the others by the spec's climate list, tuned in Task 3.
- [ ] **Step 4:** `./build-host/uagfx_tests horizon` -> PASS; `./build-host/uagfx_tests` -> 0 failed.
- [ ] **Step 5: Commit** `feat: horizon climates and strip geometry (core/horizon)`.

### Task 2: the CPU twin of the terrain

**Files:**
- Create: `src/core/horizon_terrain.hpp`, `src/core/horizon_terrain.cpp`
- Test: `tests/test_horizon.cpp` (add)

**Interfaces:**
- Consumes: Task 1's Climate.
- Produces (namespace uagfx::horizon):
  - `uint32_t hash2(int x, int y, uint32_t seed);` - integer hash, the HLSL twin bit-identical
  - `float heightAt(const Climate& c, float mapRadius, float x, float z);` - units up; 0 within mapRadius
  - `Rgb albedoAt(const Climate& c, float x, float z, float height, float upness);` - upness = the normal's up part
  - `struct Hit { bool hit; float distance; float height; float coverage; };`
    `Hit march(const Climate& c, float mapRadius, float eyeHeight, float dirX, float dirUp, float dirZ, int steps);`
    - adaptive steps, the skyline's coverage from the ray's least angular clearance (soft, anti-aliased)

- [ ] **Step 1: Write the failing tests** `horizon_terrain_shape`, `horizon_terrain_materials`:
  - `heightAt(c, 1500, 100, 200) == 0`; the same call twice returns the same value; for Hills and Alps the mean of
    `heightAt` over 64 points on the circle r=10000 exceeds the mean on r=3500; for Sea some point on r=8000 is below
    0; for Desert every point on r=8000 is under `mountAmp + hillAmp`.
  - Alps: `albedoAt(c, x, z, c.snowLine + 400, 0.9)` within 0.1 of `c.snow`; `albedoAt(..., c.snowLine + 400, 0.4)`
    within 0.15 of `c.rock` (a steep slope holds no snow).
  - `march` from eye 40 toward rise 0.30 on Hills misses (`hit == false`); toward rise -0.02 hits, distance > 1500.
- [ ] **Step 2:** run -> FAIL (no horizon_terrain).
- [ ] **Step 3: Implement** - the mock-up's math ($S/horizon_mock/mock.cpp: noised, fbm, eroded, the four land kinds;
  add Karst = tall narrow pillars from thresholded noise, Canyon = terraced mesas cut by ridged valleys in red rock,
  Waste = low eroded grey ridges); a min step of 0.4 + 0.0016 t, factor 0.24 x clearance; 6 bisection steps.
- [ ] **Step 4:** run -> PASS, whole suite 0 failed.
- [ ] **Step 5: Commit** `feat: the horizon terrain's CPU twin (core/horizon_terrain)`.

### Task 3: the preview tool and the climates' tuning

**Files:**
- Create: `tools/horizon_preview.cpp`
- Modify: `CMakeLists.txt` (host branch: `add_executable(horizon_preview tools/horizon_preview.cpp)` linked to
  uagfx_core)

**Interfaces:**
- Consumes: Tasks 1-2.
- Produces: `./build-host/horizon_preview <outdir> [width height]` writing `<region>.bmp` (day) and
  `<region>-dusk.bmp` (sunset light) for the 20 regions - the perspective the 3D camera sees, eye 40 units up,
  pitch 7°, field of view 70°, sky gradient + sun + soft shadow + haze; BMP via `texelcheck::bmpOf`.

- [ ] **Step 1:** write the tool (no test: a picture tool; its functions are Task 2's).
- [ ] **Step 2:** build host, run into `$S/horizon_prev`, convert to JPG, look at all 40; tune `climateOf` numbers
  until each region reads as its place (snow only on the highest peaks of Prontera; Lutie white; Morroc warm; seas
  dark blue; Niflheim grey and foggy).
- [ ] **Step 3:** republish the preview page https://claude.ai/artifact/Sze9VhwrhVwx5JQND6fFUW with all regions.
- [ ] **Step 4:** whole suite 0 failed; commit `feat: horizon_preview - the climates as the 3D camera sees them`.

### Task 4: the setting over the bridge

**Files:**
- Modify: `src/core/config.hpp` (Effects: `bool skyPanorama = true;` next to `skyRanges`),
  `src/core/settings/schema.cpp` (row after «ranges»: `{"view3d", "panorama", Page::Game, Control::Check, 0, 1, 1,
  {"0","1"}, {}, Text::View3dPanorama, Text::View3dPanoramaHint, "", &E::skyPanorama}`),
  `src/core/settings/strings.cpp` (+ the Text enum): «Horizon panorama» / «Панорама обрію» / «Панорама горизонта»;
  hint: the card draws the region's mountains, hills, desert or sea all round at the region's entry, lit by the day
  and the night; off - the far ranges,
  `src/win/gpu/remote.cpp` (write after skyRanges), `src/win/bridge/serve.cpp` (read after skyRanges), the bridge's
  protocol number +1, `dist/uaro-graphics.ini` ([view3d] panorama = 1, CRLF kept)
- Test: the existing settings tests (schema/strings/ini coverage)

- [ ] **Step 1:** add the field, row, strings, bridge lines.
- [ ] **Step 2:** `./build-host/uagfx_tests` -> 0 failed (the settings suites check every row has strings and an
  ini default); `grep -c "Â"` on strings.cpp unchanged; dist ini still CRLF.
- [ ] **Step 3: Commit** `feat: [view3d] panorama - the setting (on), over the bridge`.

### Task 5: the shaders

**Files:**
- Modify: `src/win/gpu/shaders.cpp` (a «horizon» block before skyRanges; skyAlong; sky3d_ps; sky3d_haze_ps),
  the shader lists of both renderers (device.cpp and renderer11.cpp: `horizon_bake_ps`)
- Modify: `src/core/view3d.hpp` (`constexpr int kPanoramaPart = 128;` beside skyParts)

**Interfaces:**
- Consumes: Task 1's pack layout (the bake reads the climate from r0-r11, hole*, fogColor, sourceRect, sourceClamp,
  twin*, jitter by that table through `#define hz...` names); Task 2's functions (same names with an `hz` prefix).
- Produces:
  - `horizon_bake_ps` - out SV_Target0 = (albedo, coverage), SV_Target1 = (octahedral normal xy, shadow, haze amount
    1 - exp(-t·haze)); one ray per texel from the map's middle at eye 40, rows row0..row1 drawn
  - `float3 skyPanorama(float3 sky, float3 dir, float3 horizon, float night, float3 toSun, float fade)` - manual
    bilinear from tex1 (colour) and tex0 (detail) by `Load` with u wrapped; light = albedo x (ambient by night +
    sun x max(N.L,0) x shadow), dawn glow on the sun's side, then lerp to `horizon` by the haze amount; composited over
    `sky` by coverage x fade
  - `skyAlong(..., float2 pano)` - pano.x the fade; called with (holeX, 0) by sky3d_ps / sky3d_haze_ps when
    `(parts & 128)`, the ranges skipped then; the water's call passes 0

- [ ] **Step 1:** write the block and the calls.
- [ ] **Step 2:** `bash /root/uagfx-tools/allshaders.sh` (dxc -HV 2018) -> every entry compiles; Windows build
  `cmake --build build-win -j4` and build-win64 -> no warnings.
- [ ] **Step 3: Commit** `feat: the horizon's shaders - the bake and the sky's panorama`.

### Task 6: Direct3D 12

**Files:**
- Create: `src/win/gpu/horizon.cpp` (Impl methods)
- Modify: `src/win/gpu/impl.hpp` (state below), `src/win/gpu/haze.cpp` (skyPass binding), the frame's start (where
  the D3D12 picture begins) to call `horizonFrame()`, the device-lost path to drop the strip

**Interfaces:**
- Consumes: Tasks 1, 4, 5.
- Produces: `struct HorizonStrip { ID3D12Resource* tex[2]; D3D12_RESOURCE_STATES state[2]; UINT srv[2]; UINT rtv[2];
  horizon::Key key; horizon::Level level; int rowsDone = 0; bool ready = false; float fade = 0; ULONGLONG readyAt; };`
  in Impl; `void horizonFrame();` (start/continue a bake: key from the map's region (region_look::of(mapName,
  100).region), mapSun, weak, mapRadius = half the ground's larger side x 5 x 1.2 or 1000; a slice of rows a frame
  with its own PSO of two RGBA8 targets, viewport the strip, scissor the rows); `bool horizonBound(UINT& colour,
  UINT& detail) const;` used by skyPass: srv1 = colour (t1), srv0 = detail (t0), `c.hole[0]` = fade,
  `c.uiPixels[2] = float(skyPartsNow() | view3d::kPanoramaPart)` - only there.
- Log once a bake: «horizon: <map> - the <region> climate baked (WxH, N steps, N frames, N ms)»; on a failure
  «horizon: ... could not be made: <why> - the far ranges» and the feature off until the device is made anew.

- [ ] **Step 1:** implement; a key change restarts (rowsDone 0, ready false, fade 0); fade rises to 1 over 1 s once
  ready; not wanted -> nothing bound, no bake.
- [ ] **Step 2:** Windows builds (x86 + build-win64) without warnings; whole host suite 0 failed.
- [ ] **Step 3: Commit**; notes uk/ru/en; ship `3.0.22-beta.N` «панорама обрію на Direct3D 12».

### Task 7: Direct3D 11

**Files:**
- Modify: `src/win/gpu/renderer11.hpp/.cpp` (the same state and `horizonFrame()` at the frame's start, binding in
  applySky3d, the reset path), always the weak level unless cardLevel says otherwise

**Interfaces:** as Task 6 with D3D11 objects (`ID3D11Texture2D* tex[2]; ID3D11RenderTargetView* rtv[2];
  ID3D11ShaderResourceView* srv[2];`).

- [ ] **Step 1:** implement; restore the render targets, viewport and scissor the frame had after each slice.
- [ ] **Step 2:** Windows build without warnings; suite 0 failed.
- [ ] **Step 3: Commit**; notes; ship `3.0.22-beta.N` «панорама обрію на Direct3D 11».

### Task 8: the owner's check and tuning

- [ ] Ask the owner for 3D-view screenshots on Prontera's fields, Lutie, Morroc, Alberta by day and at dusk; read the
  «horizon:» lines; tune the climates in core (and the shader's light numbers) a beta at a time.
