# uaRO.exe: interface text (GDI → DIB → texture)

Binary: `/root/BornRok/winEXE/uaRO.exe`. `Lnnnn` = line in `/root/BornRok/winEXE/decomp/uaRO_decomp.c`. `0x4xxxxx` = code address; key sites were checked with `objdump -M intel`. Vtables and tables were read from the exe's `.rdata`/`.data`. Class names are ours (there is no RTTI) and follow the usual Sakexe naming. "Unverified" marks anything not proven from the code.

## Summary

- **No per-string text textures.** Every interface window owns one `CDCBitmap` (vtable `0x6956EC`). `UIWindow` is vtable `0x69BFE8`.
  - A `CDCBitmap` is a memory DC plus a 32-bit top-down DIB section of **exactly the window's size**.
  - The window's `OnDraw` paints everything into it on the CPU: skin bitmaps, boxes, sprite frames, child windows and GDI text (`TextOutW`).
- **Composition and upload.**
  - Child windows are copied into their parent's DIB on the CPU (`CopyFrom`, magenta-keyed).
  - Only **top-level** windows are uploaded. They are the list at `g_windowMgr 0x727ED8 + 0x38`.
  - Each top-level DIB is cut into tiles of at most 256×256. Each tile is an A1R5G5B5 `CTexture` (vtable `0x6957C8`) with a power-of-two size.
  - When the DIB is dirty, **every** tile is re-uploaded through `CTexture::Update 0x4176C0`. The call comes from `0x403BBA` (return address `0x403BBD`).
  - Tiles are drawn every frame as 1:1 quads through `CTexture` slot 3 (`0x417C70`), in the UI list U1.
- **Fonts.**
  - Every font is created as `CreateFontA(h, 0,0,0, 400|700, 0,0,0, charset, 0, 0, NONANTIALIASED_QUALITY, 0, face)`.
  - Fonts are cached for the whole process in a vector at `0x6EC068`.
  - For uaRO (sclientinfo `langtype 14`), the default font is **Arial, RUSSIAN_CHARSET, height = requested + 3** (usually 12 → 15). Strings are converted with CP1251.
- **Transparency is a magenta key.**
  - DIBs are cleared to `0xFFFF00FF`.
  - Before each upload the client sets every alpha byte to `0xFF`. The converter then maps 5-bit magenta to texel `0x0000`.
  - Text texels therefore have 1-bit alpha and 15-bit colour, tinted only by the window alpha in the vertex colour.
- **Outlines, shadows and layout.**
  - An outline is 5 `TextOutW` calls: ±1 px in x and in y, then the main colour.
  - A shadow is 2 calls: +1,+1, then the main colour.
  - All layout uses `GetTextExtentPoint32W` in game pixels: alignment, `^RRGGBB` colour segments, per-character word wrap, the "..." cut-off, balloon sizes, caret and selection.
- **Which text goes through GDI.**
  - GDI: chat, lists, NPC/book text, buttons, tooltips, edit boxes and their `"|"` caret, and the **name and other balloons over heads**. All of these are top-level UIWindows.
  - Not GDI: damage and other floating numbers are sprites (`이팩트\숫자.spr`, `msg.spr`).
- **Main risk for spec §4.8: a "text texture" does not exist.**
  - Each texture is one tile of a whole window, so pictures and text are mixed.
  - Child→parent composition and all non-text drawing are CPU class methods, not GDI. IAT hooks on GDI alone cannot rebuild a high-resolution copy.

## 1. The text bitmap objects

### 1.1 `CDCBitmap`: vtable `0x6956EC`, base `0x6956C4`, size 0x2C

Constructor `FUN_00402e20(w,h)` (L1697):
- Sets the base vtable, then the derived one (L1711, L1720).
- Creates the list head at +0x24.
- Calls `CreateCompatibleDC(NULL)` (L1721). Call site `0x402E77` is the only one in the exe.
- Calls `FUN_00403960(w,h)` (L1723).

| Offset | Field | Evidence |
|---|---|---|
| +0x00 | vtable `0x6956EC` | L1720 |
| +0x04 / +0x08 | width / height (= window size) | L2374–2375 |
| +0x0C | HDC (memory DC) | L1721 |
| +0x10 | HBITMAP (DIB section) | L2407 |
| +0x14 | object previously selected in the DC | L2411 |
| +0x18 | bits: 32-bit BGRA, top-down, pitch = width·4 | L2396–2406 |
| +0x1C | byte "dirty": set by every write and by GetDC; cleared after upload | L2373, L1817, L2517 |
| +0x20 | byte, garbage (STL allocator byte, `mov al,[ebp+0xf]` at 0x402E45) | — |
| +0x24 / +0x28 | `std::list<CTexture*>` head / count = the **tile textures** | L2440–2475 |

The vtable at `0x6956EC` (10 slots, read from `.rdata`):

| Slot (+off) | Address | Role |
|---|---|---|
| 0 (+0x00) | 0x402EB0 | Scalar deleting destructor → `FUN_00402ee0` (L1744). Deletes the tiles, then `SelectObject(old)`, `DeleteObject(HBITMAP)`, `DeleteDC` (L1789–1791). |
| 1 (+0x04) | 0x403780 | Grow-only resize: when w or h is larger, reselect the old object, delete the bitmap, call `FUN_00403960` (L2254). No caller found. |
| 2 (+0x08) | 0x402FF0 | GetDC(&hdc): sets dirty, writes the HDC and returns true in AL (`mov al,1`, L1812). The painter tests that result (0x406478): when it is false, it skips GetCurrentObject and SetBkMode(TRANSPARENT). |
| 3 (+0x0C) | 0x403010 | ReleaseDC: `ret 4`, a no-op. |
| 4 (+0x10) | 0x4037C0 | CopyPixels(x,y,w,h,src32,key) (L2269). Does not clip. With `key` set it skips source pixels equal to `0xFFFF00FF` (L2293). Quirk: it stops when the destination pixel at a row start is `0x00000000` (L2285, `cmp [edx+eax*4],0` at 0x40380A). |
| 5 (+0x14) | 0x4030E0 | CopyFrom(x,y,w,h,CDCBitmap* src) (L1882). Copies a child into this DIB and skips `0xFFFF00FF` (L1922). Clipping uses unsigned compares (`jae` at 0x4030FF/0x40310D), so a negative x or y copies **nothing**. Right and bottom edges are clipped. |
| 6 (+0x18) | 0x4031B0 | Draws an ACT/SPR sprite (palette RLE) into the DIB (L1940). |
| 7 (+0x1C) | 0x403460 | Copies from an A1R5G5B5 texture with integer scale, testing the alpha bit (L2117; Lock at L2155). `FUN_004410a0` (L56833) uses it for animated UI sprites. |
| 8 (+0x20) | 0x403020 | Clear(RECT* or NULL, COLORREF). Converts COLORREF to ARGB and keeps the top byte as alpha (L1839). |
| 9 (+0x24) | 0x403880 | Draw(x,y,w,h,argb): uploads if dirty, then draws one quad per tile (L2312). |

The only direct (non-virtual) calls to these methods are internal: `0x402E8A` and `0x4037B4` → `0x403960`, and `0x403896` → `0x403A40`. All other code calls slots 2 and 4–9 through the vtable.

### 1.2 `FUN_00403960` (L2361): create or recreate the DIB

1. Sets dirty, w and h (L2373–2375).
2. Deletes every tile through its virtual destructor and frees the list nodes (L2376–2394).
3. Fills `BITMAPINFOHEADER {0x28, w, −h, 1, 32, BI_RGB, 0, 100, 100}` (L2395–2405).
4. Calls `CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &this->bits, NULL, 0)` (L2406). Call site `0x403A10` is the only one in the exe.
5. Calls `SelectObject(hdc, hbm)` and stores the old object at +0x14 (L2411).

Callers: the constructor and vtable slot 1. The memory starts zeroed; the factory clears it to magenta afterwards (§1.3).

### 1.3 Who creates them, with which size, and how many

The factory is `FUN_00404d50(w,h)` (L3414): `new(0x2C)`, the constructor, then `Clear(NULL, 0xFFFF00FF)` (L3438).

It has three callers:

| Caller | Where | Effect |
|---|---|---|
| `UIWindow::Create(w,h)`, `FUN_004c9aa0` | L170483, call at L170493 | Deletes the old `m_dc`, makes a new DIB, sets `m_w,m_h = w,h`, then calls Invalidate, `OnCreate` (+0x44) and +0x5C. |
| `UIWindow::Resize(w,h)`, `FUN_004c9af0` (UIWindow vtable slot 3) | L170505, call at L170515 | Same, without `OnCreate`. |
| `FUN_004ce140` | L174640, call at L174648 | One temporary DIB for a bitmap resource. It is created, drawn and deleted on every call. The only user is `rag_logo.bmp` during the mode fade (L206351). No text. |

**Size: one DIB per UIWindow object, exactly the window size.** The size is not fixed and not per string.
- Windows that fit themselves to their text call Resize on every text change, which creates a new DC, DIB and tiles.
- Name balloon `FUN_00441d80` (L57630): width = text width + 8 (at least 32), height = `DAT_006bd394` + 8.
- `FUN_00442580` (L58072) does the same.

**Count (static only; not counted at run time):**
- 64 call sites of the UIWindow constructor `FUN_004c9760`.
- 434 `Create` calls.
- 34 `AddWindow` (top-level) sites.
- 360 `AddChild` (`FUN_004c9950`) sites.
- Every control (button, edit box, scroll bar, list) has its own DIB. Typically dozens exist at once.

**Recreation:** on every Create or Resize, with a new HDC, HBITMAP, bits and tiles. A DIB is never resized in place, because the grow-only slot 1 has no caller.

**The +0x24 list** holds the tile `CTexture*` objects (§4). They are created lazily by the first upload and deleted when the DIB is recreated or destroyed.

### 1.4 UIWindow, composition and the window manager

`UIWindow` constructor: `FUN_004c9760` (L170224), vtable `0x69BFE8`.

| Offset | Field |
|---|---|
| +0x04 | parent |
| +0x0C / +0x10 | children list / count |
| +0x14 / +0x18 | x / y, relative to the parent |
| +0x1C / +0x20 | w / h |
| +0x24 | dirty. `Invalidate` (slot 1, `FUN_004c9d70`, L170701) sets it. |
| +0x28 | `m_dc` (the `CDCBitmap*`) |
| +0x38 | show |
| +0x3C / +0x40 / +0x44 | current alpha / target alpha / time |

Virtuals used by this path:
- Slot 3: Resize.
- Slot 8 (+0x20): GetTransBoxInfo. The default `0x43E6E0` returns false.
- Slot 15 (+0x3C): GetColor, `FUN_004cb630`.
- Slot 19 (+0x4C): OnProcess.
- Slot 20 (+0x50): OnDraw. The default `0x44A0B0` is `ret`.

`DoDraw(force)` is `FUN_004cb4e0` (L172221; asm 0x4CB4E0–0x4CB54B). It runs in this order:
1. If the window is dirty, call `OnDraw` (L172229), clear dirty and set `force = 1`.
2. Run `DoDraw(force)` on each child, in list order.
3. If `force` is set and there is a parent, call `parent->m_dc->CopyFrom(x, y, w, h, m_dc)` (slot 5, call 0x4CB544, L172241).

A dirty grandchild reaches the top-level DIB only when its own parent is also forced.

`UIWindowMgr` is the global `0x727ED8`:
- +0x38: top-level list. `AddWindow` is `FUN_004cc9e0` (L173467).
- +0x6C: the focused edit box (getter `FUN_004ce540`).

`Render` is `FUN_004cd720` (L174261). It is called from 0x4EAD24, 0x559264, 0x559972 and 0x5A030E. For each top-level window that is shown:
1. `DoDraw(0)` (L174284).
2. If GetTransBoxInfo returns true, draw a translucent box with `FUN_00405ce0` (L174285–174288).
3. `m_dc->Draw(m_x, m_y, m_w, m_h, GetColor())` (L174291, call 0x4CD793).

## 2. Fonts

### 2.1 The four `CreateFontA` calls

All four take the same arguments:
`CreateFontA(height, 0, 0, 0, bold ? 700 : 400, FALSE, FALSE, FALSE, charset, OUT_DEFAULT_PRECIS(0), CLIP_DEFAULT_PRECIS(0), NONANTIALIASED_QUALITY(3), 0, face)`.

| Line / call | Function | Use |
|---|---|---|
| L4385 / 0x405EB2 | `FUN_00405e00`: font entry → HFONT | No caller and no pointer found, so dead (unverified). |
| L4614 / 0x4061FD | `FUN_004060f0`: font cache `GetFont(type, height, charset, bold)` | The main path, used by `TextOutW` (L4933) and `GetTextExtentPoint32W` (L5260). |
| L5416 / 0x40705A | `FUN_00406f30`: TextOutW of a `"|XY"` charset segment | Only when `DAT_0074b559 ≠ 0`. |
| L5500 / 0x4071EA | `FUN_004070c0`: measures a `"|XY"` segment | Same condition. |

How the face is chosen (L4590–4612, the same code in each creator):
1. Call `EnumFontFamiliesExA(GetDC(0), {lfCharSet = charset}, 0x405F60, name)`. The callback runs `_stricmp(lfFaceName, name)` (`FUN_0067d120`).
2. Try the native name first (table `0x6B74AC`), then the English name (`0x6B7480`). If neither matches, pass NULL and let GDI pick the default.
3. `GetDC(0)` is never released; `ReleaseDC` is not even imported.

Font types (index = type). The code page comes from `FUN_004eed50` (L200069).

| Type | Native face (`0x6B74AC`) | Fallback (`0x6B7480`) | Charset (`0x6B74D8`) | Code page |
|---|---|---|---|---|
| 0 | 굴림 (CP949) | Gulim | 0x81 HANGUL | 949 |
| 1 | Arial | Arial | 0x00 ANSI | 1252 |
| 2 | ＭＳ Ｐゴシック (SJIS) | MS Gothic | 0x80 SHIFTJIS | 932 |
| 3 | 宋体 (GBK) | SimSun | 0x86 GB2312 | 936 |
| 4 | 細明體 (Big5) | MingLiU | 0x88 CHINESEBIG5 | 950 |
| 5 | Tahoma | Tahoma | 0xDE THAI | 874 |
| 6 | Arial | Arial | 0xA2 TURKISH | 1254 |
| 7, 8 | Verdana | Verdana | 0x00 ANSI | 1252 |
| 9 | Arial | Arial | **0xCC RUSSIAN** | **1251** |
| 10 | Arial | Arial | 0xA3 VIETNAMESE | 1258 |

### 2.2 Type and height selection: `FUN_004064d0(type, height, bold)` (L4777, asm 0x4064D0)

- **type ≠ 0:** used as given; the height is unchanged.
- **type 0 (the default):** chosen by the font language `DAT_0074b564`. `FUN_004f4820` (L205346) sets that from the langtype.

| `DAT_0074b564` | Langtype | Font type | Height |
|---|---|---|---|
| 0 | — | 0 | +0 |
| 1 | 6 | 8 | +2 |
| 1 | **14** | **9** | **+3** |
| 1 | others | 1 (6 if `DAT_0074aea0+0xA4 == 0x1F`) | +3 |
| 2 / 3 / 4 | — | 2 / 3 / 4 | +0 |
| 5 | — | 5 | +3 |
| 6 | 12 or 18 | 7 | +1 |
| 6 | others | 7 | +3 |
| 7 | — | 10 | +3 |

The painter stores the result: +0x0C type, +0x10 height, +0x14 charset (from `0x6B74D8`), +0x1D bold.

**uaRO.** These are the steps that lead to Arial/RUSSIAN:
1. The shipped `data/sclientinfo.xml` (in `/root/uar/uAthena/exe.zip`) says `servicetype korea`, but every connection entry says `langtype 14`.
2. The langtype overrides `DAT_0074b560` (L204808–204816 and two copies).
3. `FUN_004f4820` case 0xE (L205407) then sets `DAT_0074b564 = 1`, code page `DAT_0074b54c = 1251` (L205409) and `DAT_0074b559 = 0`.
4. Result: the default font is **type 9, "Arial", RUSSIAN_CHARSET, height + 3**. `MultiByteToWideChar` and `CharNextExA` use CP1251.
5. Caveat: this assumes the player's chosen connection has langtype 14 (unverified for other client copies). ROExt can override the code page; the shipped ini has `CodePage=-1`, which means no override.

**Requested heights.** `UIWindow::TextOut` (`FUN_004c9f20`) has about 300 call sites:
- Height: 133 pass the constant 12, 18 pass 14, 11 pass 11; the rest pass variables.
- Font type: 144 pass type 0. 18 pass type 1 explicitly (Arial, ANSI, CP1252, no +3); for example L89202 uses type 1, height 14 (numeric fields, unverified).

**Names over heads.**
- Height is `DAT_006bd394` = 12 (13 for langtype 0/6). Bold is `DAT_0074b55c` = 0 (1 for langtype 0/6). Both are set at L203729–203738.
- The `/font` chat command (L428500 → case 0x6B, L299918) toggles bold through `FUN_004f4df0` (L205624). That only takes effect for langtypes 0, 6, 10 and 12, so not for uaRO.

**No registry or ini setting for fonts.** The registry values read include window geometry such as `M_CHATWNDHEIGHT` and `SAYDIALOGWNDINFO_*` (L175976ff), but nothing about fonts. The only "font" strings in the exe are `CreateFontA`, `EnumFontFamiliesExA`, `TimeFont`, `RankFont` and `/font`.

### 2.3 Caching and selection

- **Cache.** A global vector at `0x6EC068` (begin `0x6EC06C`, end `0x6EC070`). Entries are 0x14 bytes: `{type, height, charset, bold, HFONT}`.
  - Lookup is a linear search (L4575–4581).
  - On a miss: `CreateFontA`, `push_back` (L4616), then a sort (L4617–4737).
  - Fonts are never released until exit (`FUN_00406070`, L4506).
- **Selection.** The font is selected into the DIB DC right before every `TextOutW` and `GetTextExtentPoint32W`: L4932, L4960, L4981, L5259, L5295, L5425, L5509. The painter destructor restores the DC's previous font (`FUN_004064a0`, L4763).

## 3. Drawing text into the DIB

### 3.1 The painter and the call sequence

The painter is a stack object:

| Offset | Field |
|---|---|
| +0 | HDC |
| +4 | previous font |
| +8 | `CDCBitmap*` |
| +0x0C | font type |
| +0x10 | height |
| +0x14 | charset |
| +0x1D | bold |

`UIWindow::TextOut` is `FUN_004c9f20` (L170830). Arguments: x, y, string, length (0 = strlen), font type, height, COLORREF. It runs:
1. `FUN_00406460(m_dc)` (L4742): calls `m_dc->GetDC` (slot 2, sets dirty). Only when GetDC returns true (`test al,al` at 0x406478; it always does) does it save `GetCurrentObject(hdc, OBJ_FONT)` (L4754) and call `SetBkMode(hdc, TRANSPARENT)` (L4756, 0x40648F), the only SetBkMode call in the exe.
2. `FUN_004064d0(type, height, 0)`. No GDI call.
3. `FUN_004065d0(colour)` → `SetTextColor(hdc, colour)` (L4842, 0x4065DA).
4. `FUN_00406900` (L4989). With `DAT_0074b559 = 0` it calls `FUN_004065f0(x, y, str, len)` (L4848), which:
   - resizes the global wchar vector at `0x6EC080` (buffer `0x6EC084`);
   - calls `MultiByteToWideChar(cp(charset), 0, str, len, buf, len)` (L4929, 0x4066FC);
   - calls `SelectObject(hdc, GetFont(...))` (L4932);
   - calls **`TextOutW(hdc, x, y, buf, n)`** (L4933, call 0x40673F).
5. `FUN_004064a0` (L4763): `SelectObject(hdc, oldFont)`, then `m_dc->ReleaseDC` (a no-op).

What GDI receives:
- The exe does not import `SetBkColor`, `SetTextAlign`, `ExtTextOut*` or any clip-region function. So alignment is TA_TOP|TA_LEFT, y is the top of the character cell, only glyph pixels are written, and GDI clips only at the DIB edges.
- Positions are integer, window-local DIB pixels.
- `TextOutW` has two other call sites, `0x4068E4` (L4982) and `0x4070B1` (L5426). Both handle `"|XY"` inline charset codes and run only when `DAT_0074b559 ≠ 0`, which is not the case for langtype 14.

### 3.2 Text helpers (UIWindow methods; each builds its own painter)

| Function | Line | Behaviour | Calls |
|---|---|---|---|
| `FUN_004c9f20` TextOut | L170830 | Plain text. | ~300 |
| `FUN_004c9fc0` TextOutRight | L170872 | Draws at `x − width(strlen)`. | 40 |
| `FUN_004ca080` TextOutWithDecoration | L170924 | Splits at `^RRGGBB` codes. Each segment gets `SetTextColor` and `TextOut`, then `x += GetTextExtent(segment)`. | 3 |
| `FUN_004ca2e0` TextOutWithShadow | L171051 | Shadow colour at (x+1,y+1), then the colour at (x,y). | 14 |
| `FUN_004ca3a0` TextOutWithOutline | L171095 | Outline colour at (x−1,y), (x+1,y), (x,y−1), (x,y+1), then the colour at (x,y): **5 TextOutW**. | 3 |
| `FUN_004ca5d0` | L171205 | The same 5 passes, but x is offset by the measured *height* (`[ebp-0x10]` at 0x4CA649), as compiled. Only `FUN_00442d60` uses it. | 1 |
| `FUN_004ca6f0` | L171255 | Colour 1 at (x,y), then colour 2 at (x−1,y). | 1 |
| `FUN_004ca490` vertical text | L171142 | Character by character, centred. | 0 (dead) |
| `FUN_004ca7b0` / `FUN_004ca860` | L171299 / L171344 | Width / height through `GetTextExtentPoint32W`. | 84 / 4 |
| `FUN_004ca910` | L171390 | Sum of the per-character heights. | 9 |
| `FUN_004ca9f0` | L171446 | Cuts the string to a pixel width and appends "..." (`0x6B8490`). | 10 |
| `FUN_004cac20`, `FUN_004caef0` | L171652, L171861 | Word wrap, measured **per character** with font (0, 12, 0); `\r` forces a break. The second keeps `^` codes. | 3, 1 |
| `FUN_004cb470` DrawBox, `FUN_004cb3e0` DrawTriangle | L172194, L172158 | DIB Clear(rect), and GDI `FillRgn` (0x4CB441, the only `FillRgn` call). | 199, 8 |

### 3.3 Once per window redraw: no string cache

**When text is drawn.**
- There is no cache keyed by string, font or colour. Text goes into the window DIB whenever that window's `OnDraw` runs. The DIB itself persists between frames and acts as the cache.
- `OnDraw` usually starts with ClearDC (`FUN_004cb3a0`, L172132), which calls slot 8 with a NULL rect.
  - 20 call sites clear to magenta `0xFFFF00FF` (transparent).
  - The others clear to a solid colour or draw a skin bitmap.
  - A new DIB is cleared to magenta by the factory.

**Per-frame redraws.**
- `UIWindowMgr::ProcessInput` (`FUN_004ccfb0`, L173909) runs every frame. It is called from 0x566C80 (L297327) and at L344369, and it has no early return.
  - It invalidates the **focused edit box** (mgr+0x6C, L174209–174211) every frame.
  - It also invalidates mgr+0x20C, mgr+0x210 and, under a condition, mgr+0x94. What these three windows are is unverified.
- Consequences for the focused edit box (the chat input in game):
  - It re-renders its text and caret every frame.
  - It is copied into its top-level parent every frame, so all of the parent's tiles are re-uploaded every frame.
  - An edit box with +0x7C ≠ 0 also invalidates its parent (L59457–59459). Which edit boxes set this is unverified.
- Windows with UI sprite animation (`FUN_004410a0`) draw a new frame into the DIB on every `OnDraw`.

**Examples of `OnDraw`:**

| Window | Function | What it draws |
|---|---|---|
| Name balloon | `FUN_00442490` (L58020) | Magenta clear, optional guild emblem bitmap, then outlined text per line (y += 2 + 12). |
| Translucent balloon | `FUN_00441ad0` (L57435) | Magenta clear, shadowed text, plus a box through GetTransBoxInfo (`FUN_00441bd0`). |
| White balloon | `FUN_00441570` (L57100) | White clear, plain text. |
| List boxes | `FUN_0044ae50` (L66095), `FUN_0044dd00` (L68588) | Clear, highlight box, decorated lines at x = 3 − horizontal scroll. |
| Edit box | `FUN_004433a0` (L58971) → `FUN_00443970` (L59295) | Skin bitmaps or a colour fill, then text, caret and selection. |
| Text button | `FUN_004403c0` (L55976) | Centred, embossed: white at +1,+1, then black. |

### 3.4 The client reads the DIB back on the CPU

| What it does | Where |
|---|---|
| Before every upload, sets byte 3 (alpha) of **every** DIB pixel to 0xFF. GDI writes `0x00RRGGBB`. | `FUN_00403a40`, L2477–2486, loop at 0x403B41 |
| Update converts 32-bit pixels to A1R5G5B5 and turns 5-bit magenta into transparent black. | L20413–20416 |
| CopyFrom and CopyPixels skip an exact `0xFFFF00FF`. | L1922, L2293 |
| CopyPixels stops on a zero destination pixel. Black GDI text is `0x00000000`. | L2285 |
| Slot 7 reads texture pixels. | Lock at L2155 |

There is no alpha derived from luminance and no antialiasing anywhere: all fonts are NONANTIALIASED and keying is binary.

## 4. From DIB to texture

### 4.1 Upload: `FUN_00403a40` (L2418), called from Draw (0x403896)

Only when the DIB is dirty (+0x1C, L2439):
1. **Make tiles if there are none.** Loop y from 0 to H in steps of 256 and x from 0 to W in steps of 256. Each tile is `FUN_0040c440(min(256, W−x), min(256, H−y), 0 /* A1R5G5B5 */, 0)` (L2454), pushed in row-major order.
2. **Force alpha.** Set the alpha byte to 0xFF for all W·H pixels (L2483).
3. **Upload each tile.** Call `tile->Update(0, 0, tw, th, bits + (W·ty + tx)·4, key = 0, pitch = W)`. This is vtable slot 1 = `0x4176C0`, called at `0x403BBA` with **return address 0x403BBD** (L2505).
4. **Clear dirty** (L2517).

Every upload rewrites whole tiles; there are no dirty rectangles. The driver sees `Lock({0,0,tw,th}, DDLOCK_WAIT|DDLOCK_WRITEONLY)`. That rect is a sub-rect of the texture whenever the tile is smaller than its power-of-two texture.

### 4.2 `CTexture`: vtable `0x6957C8`, size 0x128

Creation: `FUN_0040c440(w,h,fmt,surf)` (L9825) → `FUN_00417360` (L20127) → `FUN_00417fd0` (L20842, CreateSurface). The size comes from `FUN_00405140` (L3616): the next power of two at or above the request, clamped to the caps minimum, maximum and aspect.

| Offset | Field |
|---|---|
| +0x04 / +0x08 | surface (power-of-two) size |
| +0x0C | `IDirectDrawSurface7*` |
| +0x10 | format: 0 A1R5G5B5, 1 A4R4G4B4, 2 R5G6B5, 4 32-bit, 5 bump |
| +0x14 | key byte: black → transparent |
| +0x18 / +0x1C | content size |
| +0x120 / +0x124 | use count / time |

Vtable:

| Slot | Address | Role |
|---|---|---|
| 0 | 0x417420 | Destructor → `FUN_00416df0` (L19769), which calls the surface's `Release`. |
| 1 | 0x4176C0 | Update |
| 2 | 0x4174E0 | ColorFill |
| 3 | 0x417C70 | Draw(x,y,w,h,argb) |
| 4 | 0x417E20 | Draw(x,y,w,h) |

`Update` (`FUN_004176c0`, L20325) runs:
1. Halves the content size until it fits the texture (L20357).
2. If the source is larger than the surface, picks every n-th pixel (L20380–20390). This never happens for tiles.
3. `Lock(sub-rect, 0x21)` (L20399).
4. For format 0 (L20413–20425):
   - `v = (p>>6 & 0x3E0) + (p>>3 & 0x1F) + (p>>16 & 0x8000) + (p>>9 & 0x7C00)`.
   - If `v == 0xFC1F`, write 0.
   - Otherwise, if key is set and `(v & 0x7FFF) == 0`, write 0.
   - Otherwise write v.
5. Unlock (L20517).
6. Regenerates mip levels through `FUN_004175d0` (L20279), by stretch Blt. This only happens when the texture has mips (TRILINEARFILTER).

**Example tile layout.** A 300×120 window becomes a 256×120 tile in a 256×128 texture plus a 44×120 tile in a 64×128 texture.

**Lifetime.**
- Tiles are created once per DIB, at its first upload.
- They are re-uploaded whenever the DIB is dirty.
- They are destroyed with the DIB: on window Create, Resize or destruction.

**One texture = one tile of one window.** That tile holds pictures and text together. There is no atlas across windows and no per-string texture.

## 5. Drawing text textures on screen

**Draw** (`FUN_00403880`, L2312) calls `tile->Draw(x+tx, y+ty, min(256, w−tx), min(256, h−ty), argb)` for each tile. That is slot 3, called at 0x403911 (L2344). Here x, y, w, h are the window's `m_x, m_y, m_w, m_h` (L174291).

**The quad** (`FUN_00417c70`, L20646):
- Integer corners (x,y)–(x+w,y+h), z = 1e-5, rhw = 0.99999.
- diffuse = argb, copied as a raw dword (`mov ebx,[ebp+0x18]` at 0x417D16); specular = 0xFFFFFFFF.
- `u = (g_texOffU + {0|w}) / texW`, and v likewise (L20679–20682). `g_texOffU/V` are at `0x6B7968`/`0x6B796C`, default 0.5. So **one texel maps to one game pixel**.
- `AddRP(rp, 0x201)` (L20738) puts it in list U1 (renderer+0xEC).

**The U1 list.**
- Drawn by `FUN_0040f320` (L12250) in insertion order, with no sort.
- Drawn after `ZENABLE 0`, with SRCALPHA/INVSRCALPHA blending and alpha test ≥ 1 (see `states.md`).

**Colour and alpha.**
- argb = `GetColor()` (`FUN_004cb630`, L172298) = `(alpha << 24) | 0xFFFFFF`. The alpha moves toward the target at one step per 16 ms.
- So the text colour comes only from the texture (15-bit). Alpha = 1-bit key × window alpha.

**Order per window.** First the optional box from GetTransBoxInfo (`FUN_00405ce0`, L4279): a quad with the white NULL texture `DAT_006EE290` and z = 1e-6. Then the tiles.

**Scaling.** The client **never scales text**: each DIB is the window size, and the client has no UI-scale option. Any stretching happens only in our renderer.

## 6. Which on-screen texts use this path

**GDI (DIB) path.** Everything that is a UIWindow:
- The chat window and list boxes (`FUN_0044ae50`). Text added to a list is wrapped when it is inserted; for example `FUN_00451920` (L72079) wraps to `m_w` minus an indent. Which class that is, is unverified.
- NPC and other dialogs (decorated list `FUN_0044dd00`; the exact class is unverified).
- Books (`FUN_0048c7e0` L120449 loads `book\%s.txt` and wraps it with `FUN_004caef0`).
- Menus and text buttons (`FUN_004403c0`).
- Item and skill names in windows, and tooltips.
- Edit boxes, including the caret and the IME composition string.
- **Names over heads.** `UINameBalloonText`: vtable `0x696100` (OnDraw `FUN_00442490`), with a variant `0x69F3F4` (OnDraw `FUN_00442d60`). `FUN_005674f0` (L297778) creates them with `Create(8,8)` and `AddWindow`, so each is a top-level screen-space window.
- **Other balloons over actors.**
  - Vtable `0x696068` (constructor `FUN_004418f0`, OnDraw `FUN_00441ad0`): shadowed text plus a translucent box. Example: skill-cast text such as "Capture Monster" (`FUN_005614d0`, L294030ff).
  - Vtable `0x695FD0`: the white balloon.

**Not GDI:**
- **Damage and other floating numbers are sprites.** No number path touches the DIB/GDI code.
  - Digit sprite `이팩트\숫자.spr/.act` (`0x6D6878`/`0x6D6888`): used by the number-effect class with vtable `0x69F6DC` (constructor `FUN_005aaeb0`, L352520; its only creator `FUN_0055f210` has no xref) and by the counter `FUN_0055b3f0` (L288040). It is also preloaded in `FUN_0063a900`.
  - `이팩트\msg.spr` / `bluemsg.spr` (`0x6D6918`/`0x6D6928`): effects made by `FUN_0055d8d0` (L289897), which the actor's message 0x59 calls (L279306).
  - Which of these draws hit damage was not traced (unverified).
- **Other sprite or bitmap fonts:**
  - `이팩트\TimeFont` (L13709) and `RankFont` (L288258).
  - `effect\alpabet.bmp`: 32×32 glyphs Blt'ed into a 384×384 texture (`FUN_00650330`, L454771; Blt at L21403).
- **The screenshot caption:** `TextOutA` on the back buffer's DC with the DC's default font (`FUN_0040ffb0`, L12944). See `surfaces-present-caps.md` §8.
- **Text baked into skin bitmaps** (button and title labels) is ordinary picture data. How much there is was not checked (unverified).

## 7. Complications for high-resolution text (k = 1.75–2.6)

1. **Pictures and text share every texture.** A tile can hold skin art, icons, boxes, `FillRgn` triangles, sprite frames, child windows and text. A twin texture must reproduce the non-text pixels too (upscaled). It also must not let the low-resolution glyphs show around the crisp ones.
2. **Child → parent composition is CPU code.** `CopyFrom` (slot 5) is a plain memory copy with a magenta key and clipping, so GDI hooks never see it. Text drawn into a child DC reaches a texture only through this copy, shifted by the child's x/y. Tracking it requires the DIB vtable `0x6956EC`; every pixel writer is called only through it (§1.1).
3. **Overdraw in painter's order.** Inside one DIB, later boxes (DrawBox → Clear rect, for example list highlights and selection), bitmaps, sprites, child copies and text can cover earlier text, and the reverse. Only a Clear or a picture write erases old text. Any per-DIB record of text runs must be invalidated by these writes, or checked against the final DIB pixels.
4. **Layout is fixed in game pixels.** It comes from low-resolution `GetTextExtentPoint32W` (§3.2): alignment, colour segments, per-character wrap (always measured with font (0,12)), the "..." cut-off, balloon size (text width + 8), and caret and selection boxes (`FUN_00443970`). Hinted Arial advances do not scale by k, so a natural high-resolution layout drifts or overflows the fixed DIB. Glyphs must be placed from the low-resolution positions (for example `GetTextExtentExPointW` → `lpDx`) and clipped to the DIB.
5. **Clipping.**
   - Only the DIB edges clip; there are no clip regions.
   - Lists scroll horizontally by drawing at negative x.
   - Partial lines at the bottom are cut.
   - `CopyFrom` drops a child with a negative x or y entirely (unsigned compare).
6. **Outlines and shadows are separate calls.** They are 5 or 2 `TextOutW` calls with ±1 game-pixel offsets and their own colours. Each must be replayed with offsets × k (or replaced by a real outline) consistently across all passes.
7. **Alpha is a 1-bit magenta key in an A1R5G5B5 texture.**
   - Antialiased glyphs need real alpha, so the twin must be RGBA.
   - AA text drawn over magenta in a DIB would leave purple fringes, so only a separate high-resolution layer may be antialiased.
   - Base colours are 15-bit.
8. **Frequency.**
   - Uploads always cover the whole window.
   - The focused edit box's top-level window is re-uploaded every frame (§3.3).
   - Balloons get a new DC, DIB, tiles and surfaces on every `SetText`.
   - Name and tooltip balloons come and go with the mouse.
9. **Strings.**
   - `TextOutW` receives a pointer into a reused global buffer (`0x6EC084`), so the hook must copy it.
   - The text is already wide: CP1251 for type 9, CP1252 for explicit type 1.
   - With other langtypes, `"|XY"` codes split one string into two `TextOutW` calls; the second is positioned from a `GetTextExtentPoint32W` (L4945–4982).
10. **Fonts.** At `TextOutW` time the DC holds a cached HFONT, so the face, height, charset and bold can be read from it. A replay needs the same face at height × k. Only the client's own low-resolution copy must stay NONANTIALIASED.
11. **The UV half-texel.** Tile UVs include `g_texOffU/V` (default 0.5) relative to the low-resolution texture. Sampling a k× twin must remove that offset.
12. **Tile seams.** Windows wider or taller than 256 px are split into tiles. A glyph that crosses a tile edge is split between two textures and two quads.
13. **Measurement marks the DIB dirty.** `GetTextWidth` and the wrap functions call `GetDC` (slot 2), which causes extra full uploads. This is harmless.
14. **Edit boxes and IME.**
    - The caret is a `"|"` (`0x6B858C`) drawn with `TextOutW` at `width(prefix) − 2` (L59453). It blinks on `timeGetTime()/500` (L59449).
    - Selection and IME composition highlights are DrawBox calls sized from measured widths (L59481, L59505). The text is drawn after the box (L59489).
    - Password boxes draw the mask character (edit+0x50).
    - IME input passes through `ImmGetCompositionStringW` → `WideCharToMultiByte(cp at lang+0x98)` (L201668–201669) before it is drawn as normal text.

## Hook reference

| What | Address |
|---|---|
| GDI32 IAT | `TextOutW` 0x695084, `GetTextExtentPoint32W` 0x695088, `SetTextColor` 0x69508C, `SetBkMode` 0x695094, `CreateFontA` 0x69509C, `CreateDIBSection` 0x6950A0, `SelectObject` 0x6950A4, `DeleteObject` 0x6950A8, `DeleteDC` 0x6950AC, `CreateCompatibleDC` 0x6950B0, `FillRgn` 0x69507C, `TextOutA` 0x695080 |
| KERNEL32 IAT | `MultiByteToWideChar` 0x69527C |
| Unique call sites | `CreateDIBSection` 0x403A10; `CreateCompatibleDC` 0x402E77; `FillRgn` 0x4CB441 |
| `TextOutW` call sites | 0x40673F (main), 0x4068E4 and 0x4070B1 (`"|XY"` only) |
| `GetTextExtentPoint32W` call sites | 0x406824, 0x406D7A, 0x406E95, 0x40723E |
| `CDCBitmap` vtable | 0x6956EC; slots 0–9 at 0x6956EC…0x695710 (§1.1) |
| `CTexture` vtable | 0x6957C8; Update at 0x6957CC → 0x4176C0; Draw at 0x6957D4 → 0x417C70 |
| Return addresses | Update from the DIB upload: 0x403BBD. Tile Draw from `CDCBitmap::Draw`: 0x403914. `CDCBitmap::Draw` from `UIWindowMgr::Render`: 0x4CD796. `CopyFrom` from `DoDraw`: 0x4CB547. |
| Globals | `g_windowMgr` 0x727ED8 (+0x38 top-level list, +0x6C focused edit); font cache 0x6EC068; wchar buffer 0x6EC080; langtype `DAT_0074b560`; font language 0x74B564; code page 0x74B54C; `"|XY"` flag 0x74B559; name height 0x6BD394; name bold 0x74B55C; `g_texOffU/V` 0x6B7968/0x6B796C |
