# uaRO.exe: Direct3D 7 state usage

Lnnnn = line in `/root/BornRok/winEXE/decomp/uaRO_decomp.c`. The device is renderer (`DAT_006ee3d8`)+0x7C. Vtable offsets were verified against `d3d.h`. "Dead" means there is no call or absolute xref.

RP is the client's primitive record:
- 0x1C4 RPs: +0 vertices, +4 nVerts, +8 texture, +0xC indices, +0x10 nIdx, +0x14 SRCBLEND, +0x18 DESTBLEND, +0x1C preset, +0x20 primitive type.
- 0x2C4 RPs: +8 indices, +0xC nIdx, +0x10 texture, +0x14 lightmap, +0x18 primitive type.

## 0. Frame and phases

- **Init:** `FUN_00403f30` (L2745–2776), then calibration `FUN_004051f0` (L3772–3929).
- **Frame:** BeginScene (L10995), flush `FUN_0040e4d0` (L11644–11860), EndScene (L10999).
- **Routing:** `FUN_00412020` (L14442) sorts RPs into lists by flag; the flush draws, in order:

| List (renderer+, line) | Flag | ZEN/ZWRITE | ALPHAREF | Blend SRC/DEST |
|---|---|---|---|---|
| A 0x9C, 11657 | 0 | 1/1 | 0xCF | off |
| C 0xAC ground, 11701 | 0x800 | 1/1 | 0xCF | off |
| C2 0xBC lightmap pass 2, 11733 | 0x1000 | 1/1 | 0x01 | ONE/SRCALPHA |
| D 0x16C lightmap, FVF 0x2C4, 11754 | `FUN_004124b0` | 1/1 | 0xCF | off |
| E 0xCC, 11778 | 0x101 | 1/1 | 0x01 | SRCALPHA/INVSRCALPHA |
| F 0xFC, 11793 | 0x103 | 1/1 | 0x01 | ONE/ONE |
| G 0xDC, 11804 | 0x001 | 1/0 | 0x01 | SRCALPHA/INVSRCALPHA |
| I 0x10C, 11825 | 0x003 | 1/0 | 0x01 | ONE/ONE |
| J 0x15C effects, 11835 | 0x005 | 1/0 | 0x01 | RP+0x14/+0x18, preset RP+0x1C |
| U1 0xEC 2D/UI, 11851 | 0x201 | 0/0 | 0x01 | SRCALPHA/INVSRCALPHA |
| U2 0x11C 2D, 11855 | 0x203 | 0/0 | 0x01 | ONE/ONE |

- Alpha test is on everywhere.
- Cull is CW for lists C, C2 and D, and NONE otherwise.
- Fog follows the map, except in lists I, J, U1 and U2.
- Ground path:
  - No lightmaps (`DAT_006e4be8` = 0): list C only.
  - `DAT_006b73bc` (≥2 stages and MODULATEALPHA_ADDCOLOR, L3564): list D.
  - Otherwise: lists C and C2.

## 1. SetRenderState (62 device call sites)

- TEXTUREPERSPECTIVE 4: 1 L2747.
- ZENABLE 7: 1 (D3DZB_TRUE) L2751, 11655; 0 L3774, 11851.
- ZWRITEENABLE 14: 1 L2752, 11656; 0 L11804.
- ALPHATESTENABLE 15: 1 L11653; 0 L3772.
- SRCBLEND 19:
  - 5 SRCALPHA: L2753, 11782, 11805, 11852, 11858.
  - 2 ONE: L11737, 11793, 11826, 11855.
  - RP+0x14: L11843.
- DESTBLEND 20:
  - 6 INVSRCALPHA: L2754, 11783, 11806, 11853, 11859.
  - 5: L11738.
  - 2: L11794, 11827, 11856.
  - RP+0x18: L11845.
- CULLMODE 22: 1 NONE L2745, 11779; 2 CW L11700.
- ZFUNC 23: 4 LESSEQUAL L2746.
- ALPHAREF 24: 0xF0 L2749; 0xCF L11652, 11751; 0x01 L11736, 11780.
- ALPHAFUNC 25: 7 GREATEREQUAL L2748.
- ALPHABLENDENABLE 27: 0 L3773, 11654, 11750; 1 L11735, 11781.
- FOGENABLE 28: renderer+0x44 L11650, 13013; 0 L11825, and L13125 when there is no FOGVERTEX cap.
- FOGCOLOR 34: `DAT_006ee27c` L13091.
- FOGTABLEMODE 35: 0 NONE L13104.
- FOGSTART 36 / FOGEND 37: float 10+1490·near / 10+1490·far, L13100–13101.
- FOGDENSITY 38: float L13102.
- COLORKEYENABLE 41: 1 L2750.
- RANGEFOGENABLE 48: 1 L13127.
- AMBIENT 139: 0xFFFFFFFF L11699, 0 L11778 (inert for XYZRHW).
- FOGVERTEXMODE 140: 3 LINEAR L13125.

Phases: init is L27xx; calibration L377x; fog is `FUN_00410080` (map load) and `FUN_004100f0` (when dirty); flush is L116xx–118xx.
- Dead sites: L13116–13117 (a dead branch) and L12749–12750 (`FUN_0040fb60`).
- Never set: SHADEMODE, FILLMODE, SPECULARENABLE, DITHER, LIGHTING, CLIPPING, ZBIAS, STENCIL*, TEXTUREFACTOR, WRAPn.

## 2. SetTextureStageState (160 sites)

**Init (L2755–2776)**
- **S0:** TCI 0; COLOROP and ALPHAOP MODULATE(TEXTURE,DIFFUSE); ADDRESSV WRAP, then ADDRESS CLAMP (both axes); MIN/MAG LINEAR.
- **S1:** TCI 1; COLOROP MODULATEALPHA_ADDCOLOR(TEXTURE,CURRENT); ALPHAOP DISABLE; MIN/MAG LINEAR; ADDRESS BORDER; BORDERCOLOR 0xFF000000.
- **S2:** DISABLE.

**Calibration:** S0 MIN/MAG LINEAR (L3775, L3928).

**Flush**
- L11644: S0 MIPFILTER LINEAR.
- L11646–11649: S0 ops MODULATE; S1 ops DISABLE. This is the only reset; args and TCI persist.
- List C2: S0 ops SELECTARG1 (L11733), then MODULATE (L11748).
- List D: S1 COLOROP MODULATEALPHA_ADDCOLOR (L11754), then DISABLE (L11776).
- L11860: S0 MIPFILTER NONE.

**Effect presets:** `FUN_00410320(RP+0x1C)` (L13137–13349), called from L11842.
- Each preset sets S0 TCI 0; presets ≥5 also set S1 TCI 1. Addressing and filters are untouched.
- Notation: `color/alpha`. T = TEXTURE, D = DIFFUSE, C = CURRENT, T̄ = TEXTURE|COMPLEMENT (0x12), Tᵃ = TEXTURE|ALPHAREPLICATE (0x22). "–" = DISABLE.

| # (case line) | S0 | S1 |
|---|---|---|
| 0 (13148) | MOD(T,D)/MOD(T,D) | –/– |
| 1, 2 (13159) | MOD(T,D)/SEL1(T) | –/– |
| 3 (13170) | ADD(T,D)/SEL2(D) | –/– |
| 4 (13180) | BLENDTEXTUREALPHA(T,D)/SEL2(D) | –/– |
| 5 (13190) | MOD(T,D)/SEL2(D) | MOD(T,C)/SEL1(T) |
| 6 (13201) | MOD(T̄,D)/SEL2(D) | MOD(T̄,C)/SEL1(T) |
| 7 (13213) | SEL1(T)/SEL1(T) | MOD(Tᵃ,C)/SEL1(T) |
| 8 (13226) | MOD(T,D)/SEL1(T) | ADD(T,C)/SEL1(T) |
| 9 (13238) | MOD(T,D)/SEL2(D) | BLENDTEXTUREALPHA(T,C)/SEL1(T) |
| 10 (13251) | MOD(T,D)/SEL2(D) | BLENDDIFFUSEALPHA(T,C)/SEL1(T) |
| 11 (13264) | MOD(T,D)/SEL2(D) | ADD(T,C)/SEL1(T) |
| 12 (13276) | MOD(T,D)/SEL2(D) | ADD(T̄,C)/SEL1(T) |
| 13 (13293) | ADD(T,D)/SEL1(T) | MOD(T,C)/SEL1(T) |
| 14 (13315) | MOD(T,D)/SEL2(D) | MODULATE2X(T,C)/SEL1(T) |
| 15 (13328) | MOD(T,D)/SEL2(D) | ADDSIGNED(T,C)/SEL1(T) |

- Shared tails: L13287, 13303, 13309, 13344–13347. Presets ≥16 change nothing.
- **No restore after presets.** Leftover args persist into the next frame (preset 6 leaves S0 COLORARG1 = T̄), so state tracking must be faithful.
- `FUN_004127d0` never initializes RP+0x1C, so non-STR flag-5 RPs carry a stale preset (unverified impact).

## 3. Blend factors

- **Hard-coded:** (SRCALPHA, INVSRCALPHA), (ONE, ONE), (ONE, SRCALPHA).
  - Per RP: (SRCALPHA, ONE) for sprites (L274656), (ONE, ONE) (L47079), (SRCALPHA, INVSRCALPHA) (L47743).
  - Effect faces: SRCALPHA with ONE or INVSRCALPHA (L382788). One case takes DEST from a caller argument (L383056, unverified).
- **Data-driven:** STR keyframes are copied verbatim (L367888). srcalpha, destalpha and mtpreset become SRCBLEND, DESTBLEND and the preset without validation (L368105–368107).
- **Census of all 2,309 local STR files** (`texture_x4.zip` plus `UaRO.grf`):
  - SRC: SRCALPHA 136,337; ZERO 533; ONE 511; SRCCOLOR 262; INVSRCALPHA 128; BOTHSRCALPHA 95; DESTALPHA 4.
  - DEST: DESTALPHA 115,881; INVSRCALPHA 20,985; ZERO 435; DESTCOLOR 271; SRCALPHA 98; BOTHSRCALPHA 95; ONE 79; SRCCOLOR 25; INVDESTALPHA 1.
  - Top pair: (SRCALPHA, DESTALPHA), 115,857.
  - mtpreset: 0 = 137,810; 1 = 60.
- **Required:**
  - Support all 13 D3DBLEND values.
  - The render target has no alpha (it uses the desktop or display-mode format, L3069–3110). DESTALPHA must read 1 and INVDESTALPHA 0, which makes the dominant STR pair additive.
  - SRCBLEND = 12 means (SRCALPHA, INVSRCALPHA) and 13 is the reverse. Both override DESTBLEND.

## 4. Draw calls

All live draws use flags 0 (no WAIT, DONOTCLIP or DONOTLIGHT):
- **L3843 (calibration):** DrawPrimitive TRIANGLESTRIP, FVF 0x1C4, 4 vertices.
- **`FUN_0040bd90`:** DP at L9407, DIP at L9403; called from 9 flush sites. FVF 0x1C4; primitive RP+0x20, count RP+4, index count RP+0x10.
- **`FUN_0040bf60`:** DP at L9510, DIP at L9506; list D. FVF 0x2C4; primitive RP+0x18, count RP+4, index count RP+0xC.
- **`FUN_0040f320` (L12279/12282) and `FUN_0040f9e0` (L12709/12712):** lists U1 and U2, drawn like `FUN_0040bd90`.

Primitive types:
- TRIANGLELIST: pool `FUN_00412730` (L187405), models (`FUN_0040c070`, nIdx 0), effect faces (`FUN_00412210`).
- TRIANGLESTRIP: four-vertex quads (`FUN_004127d0`: sprites, 2D, STR); 0x2C4 tiles (`FUN_004128a0`).
- Nothing else found (not exhaustive).

Limits and counts:
- No live RP with nIdx≠0 was found (unverified). Indices are WORD.
- No client cap; D3D7 maximum is 0xFFFF vertices per call. The ground count is bytes/32 (L187405).
- Calibration issues up to 65,536 tiny draws (W/4·H/4 per scene).

Vertex formats:
- 0x1C4 = 32 B {xyz, rhw, diffuse, specular, uv}; 0x2C4 adds uv1.
- World z = (1−10·rhw)·1500/1490 (L15076). 2D z = 1e-5 or 1e-6, rhw ≈ 0.99999.

Dead code (stub it):
- `FUN_0040baa0`: SetTransform(WORLD) and DrawPrimitiveVB (L9263). Its lists are filled only by `FUN_004122e0`, which has no xrefs.
- DIPVB (L9312), DPVB (L4194), CreateVertexBuffer (L4148), and `FUN_0040ec60`…`FUN_0040fc60` except f320/f9e0.

## 5. SetTexture

**Stage 0**
- Set to the RP's wrapper+0xC, cached at +0x50 (`FUN_00412b90`, L14991).
- NULL is set explicitly, and also through `DAT_006ee290`, whose surface stays NULL (constructor at 0x40C100: `FUN_00417450(1,1,0,0)`).
- Fade quads (`FUN_00405ce0`) and boxes (L289124) use that object with MODULATE(TEXTURE,DIFFUSE). **So NULL must sample as (1,1,1,1).**

**Stage 1:** lightmap only (`FUN_00412be0`, cache +0x54). It is set to NULL at L11678 and to the lightmap at L11771, then left bound.

**Calibration:** L3777 binds a 4×4 checker texture.

**Formats (`FUN_00418150`):** A1R5G5B5, A4R4G4B4, R5G6B5 and A8R8G8B8. R5G6B5 textures get colour key 0xF81F (magenta, DDSD_CKSRCBLT, L20893–20900). With COLORKEYENABLE on, keyed texels must be discarded.

## 6. Fog

**Map load:** `FUN_00567410` (L297731) reads near, far, colour and density, then calls `FUN_00410080(1|0)` (L12988). That sets:
- +0x44: fog enable.
- +0x4C: FOGVERTEX cap (`dpcTriCaps.dwRasterCaps & 0x80`).
- Fog mode LINEAR.

**`FUN_004100f0` (flush start, when dirty)**
- Sets WORLD and VIEW to identity and PROJECTION to fov 15°, near 10, far 1500 (L13087–13090). (Inert for XYZRHW.)
- Sets FOGCOLOR.
- With the cap: sets FOGSTART, FOGEND, FOGDENSITY, FOGTABLEMODE NONE, FOGVERTEXMODE LINEAR and RANGEFOGENABLE.
- Without the cap: sets FOGENABLE 0. L11650 re-enables it; specular alpha stays 255, so nothing is fogged.

**Specular (cap only):** computed in `FUN_00412da0` (called at L14459), and inline at L14683 for 0x2C4:
`d=(1/rhw−10)/1490; f = far≠near ? (d−near)/(far−near) : d; f=clamp(f,0,1); specular = trunc((1−f)·255)<<24`

**Shader:** when FOGENABLE is on, `rgb = lerp(FOGCOLOR.rgb, rgb, specular.a)`. FOGTABLEMODE is always NONE.

## 7. Clear and viewport

- **SetViewport:** once, after CreateDevice (L2964): {0, 0, W, H, 0.0, 1.0}.
- **`FUN_00404cb0` (L3374):** Clear(1, {0,0,W,H}, TARGET|ZBUFFER, colour, 1.0, 0).
  - Colour 0xFF000000 via `FUN_0040d890(1)`.
  - In-game, renderer+0x78: 0xFFE0D5C2, 0, 0xFF6699CC or 0xFF99CCFF (L286055–286073).
- **`FUN_00404cf0` (L3395):** ZBUFFER only.
- **Calibration (L3785):** Clear(0, NULL, TARGET|ZBUFFER, 0, 1.0, 0).
- Stencil is unused.

## 8. Shader and pipeline checklist

- Two stages; TCI is 0 or 1.
- **COLOROP:** DISABLE, SELECTARG1, MODULATE, MODULATEALPHA_ADDCOLOR. Presets add ADD, MODULATE2X, ADDSIGNED, BLENDDIFFUSEALPHA and BLENDTEXTUREALPHA.
- **ALPHAOP:** DISABLE, SELECTARG1, SELECTARG2, MODULATE.
- **Arguments:** TEXTURE, DIFFUSE and CURRENT, plus the COMPLEMENT and ALPHAREPLICATE modifiers.
- **Blend:** all 13 factors, including the BOTH* overrides.
- **Alpha test:** GREATEREQUAL, plus colour-key discard.
- **Depth:** LESSEQUAL, with test and write toggles.
- **Cull:** NONE, and CW (culls triangles that are clockwise on screen). Likely mapping: FrontCounterClockwise=FALSE + CULL_FRONT (verify).
- **Addressing:** S0 CLAMP; S1 BORDER with opaque black.
- **Filtering:** MIN/MAG LINEAR; S0 MIP LINEAR or NONE.
- **Fog:** vertex fog from specular alpha.
- **Caps the client checks:**
  - FOGVERTEX.
  - ≥2 stages with MODULATEALPHA_ADDCOLOR, which enables list D.
  - LINEARMIPLINEAR, which enables mipmaps (L3567).

## Hazards

1. A NULL texture must sample as white.
2. S1 can have ALPHAOP DISABLE while COLOROP is on. Pass CURRENT alpha through.
3. Shift XYZRHW x/y by −0.5 to match D3D7 pixel centres. 2D UVs already add the calibrated texel offset `_DAT_006b7968/6c` (default 0.5).
4. Output w = 1/rhw, and clip depth to [0,1].
5. Direct backbuffer access: calibration Blt/Lock (L3849–3857), per-frame Lock (`FUN_0040fdb0`), GetDC/TextOutA (`FUN_0040ffb0`), CPU edits (`FUN_004111e0`).
