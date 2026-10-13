# uaRO.exe: surfaces, present path and caps (DDraw7/D3D7)

Source: `/root/BornRok/winEXE/decomp/uaRO_decomp.c` (L = line); key sites verified by disassembly. "Dead" = no call/immediate xref. `C3D` = C3dDevice @0x6ebe18: +2c primary, +30 back buffer, +34 RT alias, +38 Z, +3c device, +40 DD7, +44 D3D7, +1e0 sysmem copy, +1d4 fullscreen flag, +4/+8 width/height. Init flags are 6 (windowed) or 7 (fullscreen) (L459034): bit0 fullscreen, bit1 back buffer.

## 1. CreateSurface

|Line|Stored in|dwFlags|dwCaps / dwCaps2|Other|
|-|-|-|-|-|
|L3051|+2c primary, windowed|0x1 CAPS|0x200 PRIMARYSURFACE|–|
|L3073|+30 back buffer, windowed|0x7 CAPS\|HEIGHT\|WIDTH|0x2040 3DDEVICE\|OFFSCREENPLAIN|client size (GetClientRect L3039)|
|L3107|+2c primary, fullscreen|0x21 CAPS\|BACKBUFFERCOUNT|0x2218 PRIMARY\|3DDEVICE\|FLIP\|COMPLEX|1 back buffer; GetAttachedSurface({0x4 BACKBUFFER}) L3119 → +30|
|L3196|+1e0 sysmem copy|0x7|0x840 OFFSCREENPLAIN\|SYSTEMMEMORY|screen size|
|L3234|+38 Z|0x1007 (+PIXELFORMAT)|0x24000 ZBUFFER\|VIDEOMEMORY (HAL/TnLHAL GUID), else 0x20800 ZBUFFER\|SYSTEMMEMORY|enumerated pf; RT->AddAttachedSurface L3238; SetRenderTarget(RT,0) L3243|
|L20048|CSurface+0xc|0x10007 (+CKSRCBLT)|0x40 OFFSCREENPLAIN|w,h rounded up to ×4; key = primary R\|B mask (magenta); pf copied but DDSD_PIXELFORMAT not set|
|L20919|CTexture+0xc|0x101007 CAPS\|H\|W\|PIXELFORMAT\|TEXTURESTAGE (+0x20000 MIPMAPCOUNT if trilinear; +0x10000 CKSRCBLT only for R5G6B5)|0x1000 TEXTURE (+0x400008 MIPMAP\|COMPLEX if trilinear) / 0x10 TEXTUREMANAGE|stage 0; enumerated pf; dwMipMapCount=log2(min(w,h)), or 1 if w≤2|
|L21098, L21283|bump texture|0x101007|0x1000 / 0x10|DDPF_BUMPDUDV 0x80000 16-bit; dead (format 5 never requested)|

The RT alias +34 is +30 plus an AddRef (L3184-3190). Texture size (L3618): next power of two ≥ the request, clamped to the caps min/max and aspect cap. All factory callers request only formats 0 (A1R5G5B5) and 1 (A4R4G4B4). Trilinear means registry TRILINEARFILTER≠0 plus the LINEARMIPLINEAR cap (L3568). Init sets COLORKEYENABLE=1 (L2750). The glyph atlas (L454794) is filled only by Blt, so new surfaces must be zeroed.

## 2. Lock (vtbl 0x64, dwSize 0x7c)

|Line (return addr)|Surface|Rect, flags|Reads|Access|Unlock|
|-|-|-|-|-|-|
|L12860 (0x40fdf7)|RT, every present|NULL, 0x1 WAIT|lpSurface, lPitch, bitcount → renderer+90/+94/+38|none found|L12865, immediately|
|L13477 (0x410ddc), L14296 (0x411c10)|RT, screenshots after present|NULL, WAIT|same|read|L13536, L14399|
|L13909 (0x4116a2)|RT, wave effect|NULL, WAIT|same|read+write|L14040|
|L3857 (0x4055cf)|calibration CSurface|NULL, WAIT|lpSurface, lPitch, bitcount>>3|read|L3918|
|L19837|CSurface|{x,y,x+w,y+h}, 0x21 WAIT\|WRITEONLY|lpSurface at rect origin, lPitch, bitcount (16 → converts; otherwise copies 32-bit)|write; skips magenta|L19902|
|L20399|CTexture|sub-rect, 0x21|lpSurface at rect origin, lPitch|write|L20517|
|L20569|CTexture (sprite atlas)|NULL, WAIT|lpSurface, lPitch|writes a sub-rect; the rest must persist|L20633|
|L2155|A1R5G5B5 CTexture → GDI DIB|NULL, WAIT|lpSurface, lPitch/2|read|L2245|
|L224756|back buffer (Bink)|NULL, WAIT|lpSurface, lPitch, dwHeight|Bink writes|L224761|

L1536/L1548 (no caller found), L11028 and L21103-21296 are dead. Masks come from the primary (§9), never from Lock.

## 3. Blt (0x14) / BltFast (0x1c)

|Line|Dest ← source|Rects|Flags|
|-|-|-|-|
|L3340, L3353|primary ← back|dest = screen client rect; src {0,0,w,h}|0x1000000 WAIT|
|L224765|primary ← back (Bink)|src {0,0,512,347}, stretched|WAIT|
|L3849|CSurface ← RT|NULL/NULL|WAIT|
|L4012/4014, L4033/4035|sysmem ↔ RT|NULL/NULL|0x200 ASYNC, then WAIT|
|L19927, L20230|CSurface/CTexture COLORFILL|dest argument (may be NULL)|0x1000400; DDBLTFX size 0x64; dwFillColor = COLORREF via primary shifts, &0xffff|
|L20311/20314|mip N ← level 0|dest = rect>>N|ASYNC, then WAIT (stretch)|
|L21403|texture ← texture (glyphs)|32×32 → 16/32×32|WAIT (stretch)|
|L19989; L20120|RT ← CSurface; CSurface ← temporary CSurface|dest {0,0,w,h}, src NULL; NULL/NULL|WAIT (stretch)|
|L19965 BltFast|RT ← CSurface|src clipped|0x11 SRCCOLORKEY\|WAIT|

L19965, L19989 and L20120 run only for bitmaps larger than the max texture size (L3447). L1483 (BltFast) and L3304/L3311 are dead.

## 4. Present path

FUN_0040fdb0(p) (L12840) does a Lock/Unlock of the RT, then calls FUN_00404bc0(p) (L3320):
- **p=0** (map-loading bar, L196394; both modes): GetClientRect + ClientToScreen, then Blt (L3340). No vsync.
- **p=1, fullscreen:** Flip(NULL, 0x1 DDFLIP_WAIT) (L3345); C3D+1d8 (Blt fallback) is never set.
- **p=1, windowed:** WaitForVerticalBlank(0x1 BLOCKBEGIN, NULL) (L3352), then Blt (L3353).
- **WM_MOVE** while inactive and windowed calls FUN_00404bc0(1) (L457929). It re-presents the old frame.
- **Bink:** Lock, BinkCopyToBuffer, Unlock, stretch Blt (L224765); also in fullscreen, without Flip.

Clipper (windowed only): CreateClipper(0,…) (L3054), SetHWnd(0,hwnd) (L3058), SetClipper (L3059). The source rect C3D+1c is the client size at init; the dest rect C3D+0c is recomputed on every present. **The back buffer must survive Present:** the mode fade (L206339-206361) saves RT→sysmem, then each frame draws an alpha quad, presents and restores; screenshots read the RT after present.

## 5. Caps the client reads

**DD7 GetCaps** (L3553, dwSize 0x17c): only BLTSTRETCH 0x200 is stored (+22c), and nothing reads it. Return 0.

**Device GetCaps** (L3218, L3557) writes into C3D+48. The fields are copied out only if the call returns exactly 0 (L3558); otherwise the max texture size stays 0.

|Offset|Bit/field|Effect|Report|
|-|-|-|-|
|0x44|RasterCaps 0x8000 ZBUFFERLESSHSR|set → no Z-buffer (L3219)|clear|
|0x44|0x80 FOGVERTEX|set → FOGTABLEMODE 0, FOGVERTEXMODE, RANGEFOGENABLE 1 (L13006, L13069); clear → FOGENABLE 0, or a "does not support fog effect" box|set|
|0x60|FilterCaps 0x20 LINEARMIPLINEAR|enables mipmaps with TRILINEARFILTER|set|
|0x7c/0x80|min texture W/H → +218/+21c|lower clamp|1|
|0x84/0x88|max texture W/H → +220/+224|clamp; ≥0x400 sets flag +22d, which is unused|4096|
|0x90|max aspect ratio → +228|clamp if ≠0|0|
|0xb4+0xb8|TextureOpCaps 0x20000 MODULATEALPHA_ADDCOLOR and wMaxTextureBlendStages ≥2|DAT_006b73bc=1 → single-pass ground+lightmap (stage1 op 0x12, FVF 0x2c4; L11753, L192357); else 2 passes|set; 8|
|0xb4|0x200000/0x400000 BUMPENVMAP*|bump path only|dead|
|0x98-0xa4|guard band|unused; the band is fixed at −640/−480/w+640/h+480 (L3596)|any|

wMaxSimultaneousTextures is not read.

## 6. Enumeration

**EnumTextureFormats** (L20881) runs once per format code. Callback 0x418150 (L20928) gets ctx=&code and returns 0 (CANCEL) after copying a match:
- code 0: dwFlags&1 (ALPHAPIXELS), 16-bit, alpha mask 0x8000 (A1R5G5B5).
- code 1: ALPHAPIXELS, 16-bit, alpha mask 0xf000 (A4R4G4B4).
- code 2 (no ALPHAPIXELS, 16-bit, R mask 0xf800) and code 4 (ALPHAPIXELS, 32-bit, alpha 0xff000000) are unused.

DDPF_RGB and the colour masks are not checked. Unsupported codes (L20878) and missing formats (L20884) give error boxes.

**EnumZBufferFormats**(device GUID, 0x404530, C3D+138) (L2980): ctx dwFlags is preset to 0x400 (0x4400 only with the unused stencil flag). The callback copies any format whose dwFlags equals that value exactly. It returns 0 (stop) when dwZBufferBitDepth==16 and 1 otherwise, so the result is the first D16 or the last match. No match → init fails with 0x82000005 (L2981).

Never called: EnumDevices, EnumDisplayModes, DirectDrawEnumerate (sole DDRAW import: DirectDrawCreateEx).

## 7. Device loss and lifecycle

- **Init:**
  - DirectDrawCreateEx(registry GUIDDRIVER pointer, possibly all-zero; IID_IDirectDraw7) L2859.
  - GetDisplayMode L2869 (the bit count is stored but unused).
  - SetCooperativeLevel L2878: 0xC08 = NORMAL, or 0xC13 = FULLSCREEN\|ALLOWREBOOT\|EXCLUSIVE; both add MULTITHREADED\|FPUSETUP. Fullscreen first sets WS_POPUP + SW_MAXIMIZE (L2731).
  - SetDisplayMode(w,h,bpp,0,0) L3094.
  - Surfaces, then QI IID_IDirect3D7 L2951.
  - CreateDevice(registry GUIDDEVICE, RT; no Z yet) L2956, then SetViewport{0,0,w,h,0,1} L2964.
  - Z-buffer, then caps and calibration (L459039), then Bink.
- **Loss checks:** IsLost is effectively unreached. Restore and TestCooperativeLevel are never called. RestoreAllSurfaces (L3408) runs only after fullscreen re-activation (flag 0x7735e8 set at L457936, consumed at L286200/L344375), followed by a UI image reload. Present results are ignored.
- **Error boxes:** before every error MessageBox, the client calls Unlock on the back buffer and primary (L2603-2604) and FlipToGDISurface (L2605), even when nothing is locked.
- **EvictManagedTextures:** L10277.
- **Shutdown** (L2651):
  - Device Release must return 0, else "D3D object is still referenced %d times" (L2664).
  - Then, in order: back buffer (windowed only), Z, RT alias, primary, D3D7, sysmem copy.
  - SetCooperativeLevel(hwnd, 0x8) (L2701).
  - DD7 Release must return 0, else "DDraw object…" (L2703).
- **x87 state (unverified):** native D3D7 may leave the x87 FPU in single precision.

## 8. GetDC/ReleaseDC

These are called only on the RT, through wrapper vtbl 0x69569c (L1419/L1436), from FUN_0040ffb0 (L12946): AddRef, GetDC, SetBkMode TRANSPARENT, SetTextColor, TextOutA (L12977), ReleaseDC, Release. The only live caller is the screenshot caption (L207011-207015): date/time text drawn 5× as an outline, after present, right before the screenshot Lock, which must see it. The FPS overlay caller is dead.

## 9. Primary and Bink

- **Primary:** only the pixel format from GetSurfaceDesc (L3126) is used.
  - It builds the shift tables C3D+1e4…+214 (L3127-3182). Every CPU conversion and the magenta keys use them.
  - Screen mode (L3523-3545): R mask 0xf800 → 2; 0x7c00 → 3; 0x1f → "BGR 565 … NOT SUPPORTED" box; 32 bpp → 4; anything else → "NOT SUPPORTED MODE".
  - The primary is never locked.
- **32-bit quirks:** COLORFILL truncates to 16 bits. Calibration masks pixels with 0xffff, so 8 white texels sum to at most 4080, below the 0x167f threshold. It never succeeds and keeps the default UV offset 0.5 (0x6b7968/0x6b796c).
- **Bink:** BinkDDSurfaceType(back buffer) (L224755) receives our surface. Unverified (binkw32.dll is not available): it reads the format via GetPixelFormat (vtbl 0x54). BinkCopyToBuffer (L224760) writes into our locked memory.

## What our implementation must return/do

- **DirectDrawCreateEx:** ignore the GUID.
- **SetCooperativeLevel:** accept 0xC08, 0xC13 and 0x8; fullscreen means a borderless window.
- **SetDisplayMode, WaitForVerticalBlank, FlipToGDISurface, RestoreAllSurfaces, DD7 GetCaps, EvictManagedTextures:** return 0. Never return DDERR_SURFACELOST.
- **CreateSurface:** every row of §1; zeroed memory; non-textures use the primary's format; build flip and mip chains.
- **Clipper:** store the hwnd (present target).
- **Release (DD7, device):** count only client references, so the shutdown checks see 0.
- **CreateDevice:** accept any GUID and an RT with no Z.
- **EnumZBufferFormats:** offer at least D16 {0x20, 0x400, 0, 16, 0, 0xffff, 0, 0}.
- **Device GetCaps:** return 0 with the §5 values.
- **EnumTextureFormats:** offer A1R5G5B5 {0x41, 16, 0x7c00, 0x3e0, 0x1f, 0x8000} and A4R4G4B4 {0x41, 16, 0xf00, 0xf0, 0xf, 0xf000}; stop on CANCEL.
- **SetRenderTarget:** bind the attached Z.
- **Lock:**
  - For a sub-rect, lpSurface points at the rect origin and lPitch is the full pitch. Fill dwWidth, dwHeight and dwRGBBitCount.
  - Keep persistent CPU copies of textures (partial writes; reads at L2155).
  - RT locks must return real pixels. The exception is the per-frame lock (return address 0x40fdf7), which touches nothing: skip its readback with a call-site check or guard-page lazy fill.
- **Unlock:** upload the dirty rect; if not locked, return DDERR_NOTLOCKED (no crash).
- **Blt:**
  - NULL rect means the whole surface. Support stretching, COLORFILL in the surface's format, and texture-to-texture copies including mips. Treat ASYNC as synchronous.
  - Dest = primary means present the source sub-rect scaled to the window (Bink uses 512×347).
  - RT↔sysmem copies may stay on the GPU.
- **BltFast:** honour SRCCOLORKEY.
- **Flip:** present with copy semantics; the back buffer keeps its contents.
- **GetAttachedSurface:** support the back buffer and the next mip; AddRef the result; return NOTFOUND at the end.
- **AddAttachedSurface:** attach the Z-buffer.
- **GetSurfaceDesc, GetPixelFormat (needed for Bink), IsLost:** implement.
- **GetDC/ReleaseDC on the RT:** the DC holds the current pixels and is written back on release.
