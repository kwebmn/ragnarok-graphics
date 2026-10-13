# uaRO.exe start-up texel-offset calibration (`FUN_004051f0`)

Binary: `/root/BornRok/winEXE/uaRO.exe` (2007). `Lnnnn` = line in `decomp/uaRO_decomp.c`; `0x40xxxx` = instruction address, cross-checked with `objdump -M intel`. Ghidra mistypes the loop counters `fVar6`, `local_14` and `local_30` as `float`. In the asm (`inc edi`, `div`, `jb`) they are `uint32`. Ghidra `local_N` = `[ebp-(N-4)]`.

## Summary

- The calibration runs **once per process and unconditionally**. WinMain (`L459039`) calls `FUN_00404ed0`, which calls `0x4051f0` at `0x405121` (`L3609`) right after D3D init (`FUN_00403f30`). The return value (1/0) is ignored.
- **Output:** two `float`s in texel units, `g_texOffU @0x6B7968` and `g_texOffV @0x6B796C`. Each value is `n/256` with n = 0..255. The `.data` initial value of both is `0.5f` (0x3F000000). The calibration writes them once (`0x4057F9`, `0x40580B`).
- **16-bit render target:** the calibration really measures the rasterizer. **24/32-bit:** every read pixel is truncated to 16 bits (`and eax,0xFFFF` at `0x4056AA`/`0x405706`), so red is lost. The largest possible bright sum is 8·510 = 4080, below the 5760 threshold. No quad is ever accepted, so the result is 128/256 = **(0.5, 0.5) on every card**.

## 1. Pseudo-code

```c
// FUN_004051f0, __fastcall ECX = C3dDevice* dev. L3671-3944 / 0x4051F0-0x405888
bool CalibrateTexelOffset(C3dDevice* dev) {
  IDirect3DDevice7* d3d = dev->d3dDevice;                       // dev+0x3C
  uint32_t tex32[16] = { 0,0,~0u,~0u,  0,0,~0u,~0u,            // L3750-3765, row-major 4x4,
                         ~0u,~0u,0,0,  ~0u,~0u,0,0 };          // texel (c,r) = tex32[4r+c]
  CSurface rb(dev->W, dev->H);       // L3767 FUN_00416d50 -> FUN_00417170 (L19998): IDirectDraw7::CreateSurface,
                                     // dwFlags 0x10007 (CAPS|HEIGHT|WIDTH|CKSRCBLT), dwCaps 0x40 OFFSCREENPLAIN,
                                     // W,H rounded UP to a multiple of 4, no DDSD_PIXELFORMAT => primary's format
  CTexture tex(4, 4, /*fmt idx*/0);  // L3769 FUN_00417360 -> FUN_00417fd0 (L20844): ARGB1555 (A 0x8000,
                                     // R 0x7C00, G 0x03E0, B 0x001F, found via EnumTextureFormats/FUN_00418150),
                                     // flags 0x101007 (+MIPMAPCOUNT if mip option), caps TEXTURE (+MIPMAP|COMPLEX),
                                     // caps2 0x10 TEXTUREMANAGE, no colour key (CKSRCBLT only for fmt 2).
                                     // Size via FUN_00405140: pow2, clamped to device min/max => 4x4.
  tex.Update(0,0,4,4, tex32, /*keyFlag*/0, /*pitch*/0);  // L3771 FUN_004176c0 (L20328): Lock({0,0,4,4},
                                     // DDLOCK_WAIT|WRITEONLY 0x21); case 0: 0xFFFFFFFF->0xFFFF, 0->0x0000
  d3d->SetRenderState(0x0F /*ALPHATESTENABLE*/,  FALSE);       // L3772
  d3d->SetRenderState(0x1B /*ALPHABLENDENABLE*/, FALSE);       // L3773
  d3d->SetRenderState(0x07 /*ZENABLE*/,          FALSE);       // L3774
  d3d->SetTextureStageState(0, 0x11 /*MINFILTER*/, 2 /*LINEAR*/); // L3775
  d3d->SetTextureStageState(0, 0x10 /*MAGFILTER*/, 2 /*LINEAR*/); // L3776
  d3d->SetTexture(0, tex.surf);                                 // L3777
  uint32_t cols = dev->W >> 2, rows = dev->H >> 2, perPass = cols * rows;   // L3779-3780, L3789
  int32_t best = -1024;  /*0xFFFFFC00*/   uint32_t bestIdx = 0x8080;        // L3781-3782
  for (uint32_t base = 0; base < 0x10000; base += perPass) {               // L3783, L3919-3920
    d3d->Clear(0, NULL, 3 /*TARGET|ZBUFFER*/, 0x00000000, 1.0f, 0);       // L3785
    if (FAILED(d3d->BeginScene())) return false;                          // L3787 (globals untouched)
    for (uint32_t i = 0; i < perPass && base + i <= 0xFFFF; ++i) {         // L3792
      uint32_t idx = base + i, lo = idx & 0xFF, hi = idx >> 8;
      float x0 = 4*(i % cols), y0 = 4*(i / cols);
      float du = lo * (1/256.f), dv = hi * (1/256.f);   // K = 0x3B800000 @0x695720
      float u0 = du / tex.W, u1 = (du + 4.0f) / tex.W;  // C = 0x40800000 (4.0) @0x69571C; tex.W = tex+4
      float v0 = dv / tex.H, v1 = (dv + 4.0f) / tex.H;  // tex.H = tex+8 (both 4)
      // FVF 0x1C4 = XYZRHW|DIFFUSE|SPECULAR|TEX1, 32-byte stride
      // z = 0x3727C5AC (1e-5f), rhw = 0x3F7FFF58 (0.99999f), diffuse = specular = 0xFFFFFFFF
      Vtx v[4] = { {x0,  y0,  z,rhw,~0u,~0u,u0,v0},    // TL
                   {x0,  y0+4,z,rhw,~0u,~0u,u0,v1},    // BL
                   {x0+4,y0,  z,rhw,~0u,~0u,u1,v0},    // TR
                   {x0+4,y0+4,z,rhw,~0u,~0u,u1,v1} };  // BR  (CCW on screen; drawn only because CULLMODE=NONE)
      d3d->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0x1C4, v, 4, 0);         // L3843
    }
    d3d->EndScene();                                                     // L3848
    rb.surf->Blt(NULL, dev->renderTarget /*dev+0x34*/, NULL, 0x01000000 /*DDBLT_WAIT*/, NULL); // L3849
    DDSURFACEDESC2 sd = { 0x7C };
    if (FAILED(rb.surf->Lock(NULL, &sd, 1 /*DDLOCK_WAIT*/, NULL))) return false;            // L3857
    uint32_t Bpp = sd.ddpfPixelFormat.dwRGBBitCount >> 3;                                   // L3859
    for (uint32_t i = 0; i < perPass && base + i <= 0xFFFF; ++i) {
      int bright = 0, dark = 0;
      for (int k = 0; k < 16; ++k) {                                     // L3871-3909
        int x = 4*(i % cols) + (k & 3), y = 4*(i / cols) + (k >> 2);
        uint8_t* row = (uint8_t*)sd.lpSurface + sd.lPitch * y;
        uint32_t p = (Bpp == 2) ? *(uint16_t*)(row + 2*x) : *(uint32_t*)(row + Bpp*x);
        p &= 0xFFFF;                                                     // also for 24/32-bit!
        int c = (((p & dev->Rmask) >> dev->Rshift) << dev->Rpad)        // +0x208,+0x1E4,+0x1E8
              + (((p & dev->Gmask) >> dev->Gshift) << dev->Gpad)        // +0x20C,+0x1EC,+0x1F0
              + (((p & dev->Bmask) >> dev->Bshift) << dev->Bpad);       // +0x210,+0x1F4,+0x1F8
        if (tex32[k]) bright += c; else dark += c;                      // pad = 8 - channel bits
      }
      if (bright >= 0x1680 && dark <= 0x180 && bright - dark > best) {  // L3910, 0x405769-0x40578A
        best = bright - dark;  bestIdx = base + i;                      // strict '>' => first maximum wins
      }
    }
    rb.surf->Unlock(NULL);                                               // L3918
  }
  g_texOffU = (bestIdx & 0xFF) * (1/256.f);                             // L3925 -> 0x6B7968
  g_texOffV = (bestIdx >> 8)   * (1/256.f);                             // L3927 -> 0x6B796C
  d3d->SetTextureStageState(0, 0x11, 2); d3d->SetTextureStageState(0, 0x10, 2);  // L3928-3929
  return true;                                                          // ZENABLE etc. not restored
}
```

`dev->renderTarget` (+0x34) is the flip-chain back buffer in fullscreen, or an offscreen `DDSCAPS_3DDEVICE|OFFSCREENPLAIN` surface in windowed mode (`L3064-3078`, `L3184-3189`). WinMain always passes the double-buffer flag. The channel masks, shifts and pads come from the primary surface's `DDPIXELFORMAT` (`FUN_00404570`, `L3125-3182`). For 565 the shifts are 11/5/0 and the pads 3/2/3, so a white pixel sums to 248+252+248 = 748.

**Other device state in force.** Set in `FUN_00403f30` (`L2745-2776`), plus the D3D7 defaults:

- **Render states:** `CULLMODE=NONE`, `COLORKEYENABLE=TRUE`. `SPECULARENABLE`, `FOGENABLE` and `DITHERENABLE` are left at their default FALSE.
- **Stage 0:** `COLOROP=MODULATE(TEXTURE, DIFFUSE)`, `ALPHAOP=MODULATE`, `TEXCOORDINDEX=0`. `ADDRESS=CLAMP` (`L2774` overrides `ADDRESSV=WRAP` from `L2769`). `MIPFILTER=NONE`.
- **Stage 1:** `COLOROP=0x12` (MODULATEALPHA_ADDCOLOR) with `ARG1=TEXTURE`, `ARG2=CURRENT`, and `ALPHAOP=DISABLE`. No texture is bound to it.
- **Stage 2:** `COLOROP=DISABLE`.
- **Effect:** the output colour equals the filtered texel. The renderer later re-sets the Z and alpha states (`L11653-11655`).

## 2. Consumers

There are only two readers (verified by scanning the binary for the absolute addresses). Both are `CTexture` virtuals, vtable `0x6957C8`:

- slot 3 `FUN_00417c70(x,y,w,h,argb)` (`L20679-20682`)
- slot 4 `FUN_00417e20(x,y,w,h)` (`L20772-20777`)

```c
u0 = offU / texW;            v0 = offV / texH;          // texW/texH = this+4/+8 (pow2 size)
u1 = (srcW + offU) / texW;   v1 = (srcH + offV) / texH; // srcW,srcH = w,h (c70) | this+0x18/+0x1C (e20)
// quad TL,TR,BL,BR at integer (x,y)-(x+w,y+h); z=1e-5f, rhw=0.99999f, spec=0xFFFFFFFF.
// Positions get NO -0.5; the UV offset is the only pixel-centre correction.
```

The client never selects POINT filtering for stage 0, so UI sprites are always bilinear. Under D3D7 rules, pixel `x+k` samples `s = u·texW − 0.5 = k + offU − 0.5`. With offU = 0.5 that lands exactly on the centre of texel k.

## 3. What hardware produces

**D3D7 rules.** Per quad, `δ = lo/256 − 0.5` and `ε = hi/256 − 0.5`, so pixel column c samples `s = c + δ`. With CLAMP addressing, the bilinear "white fraction" of the right half along x is:

- `A = (0, δ, 1, 1)` for δ ≥ 0
- `A = (0, 0, 1−|δ|, 1)` for δ < 0

B is the same along y. The texture is an XOR pattern, so `pixel = A + B − 2AB`.

- **idx 0x8080 (δ = ε = 0):** each pixel equals its texel. bright = 8·748 = **5984**, dark = 0. This is the maximum contrast possible.
- **idx 0x0000 (δ = ε = −0.5):** rows are `0,0,½,1 / 0,0,½,1 / ½,½,½,½ / 1,1,½,0`. bright = 4·384 + 4·748 = 4528, dark = 3·384 = 1152. Rejected.
- **Neighbours of 0x8080 also reach 5984,** because small errors vanish in 5/6-bit quantisation. At (lo 128, hi 126), ε = −2/256: row-2 white pixels = 0.9921875 → G6 = round(62.508) = 63, R5/B5 = 31; dark ones = 0.0078125 → G6 = round(0.492) = 0. Still perfect.

`hi` is the outer scan axis, so the perfect quad with the smallest `hi` wins. CPU simulation of all 65,536 quads (565 unless noted):

| Bilinear weights | float→565 conversion | Perfect set | Result idx | (U, V) |
|---|---|---|---|---|
| exact / ≥ 8-bit | round to nearest | \|Δlo\|+\|Δhi\| ≤ 2 (13 quads) | 0x7E80 | (0.5, 0.4921875) |
| exact / ≥ 8-bit | floor | 0x8080 only | 0x8080 | (0.5, 0.5) |
| exact | 8-bit round, then `>>3`/`>>2` | \|Δ\|₁ ≤ 3 | 0x7D80 | (0.5, 0.48828125) |
| 6-bit, truncated | round | — | 0x8080 | (0.5, 0.5) |
| 6-bit, rounded | round | — | 0x7E7E | (0.4921875, 0.4921875) |
| exact, X1R5G5B5 target | round | — | 0x7C80 | (0.5, 0.484375) |
| any, 24/32-bit target | any | none | 0x8080 (default) | **(0.5, 0.5)** |

**Unverified:** which row a 2007 GeForce or Radeon matched in 16-bit mode. We have no hardware captures. Every row stays within 4/256 texel of 0.5, which means at most 1.6 % bleed at a sprite edge. In 32-bit, every card produced exactly (0.5, 0.5).

**D3D10+ rules (centres at +0.5, no compensation).** Now `s = c + lo/256`, so offset 0 is the ideal. At idx 0x8080 the readback is the 50/50 blur (4528 / 1152).

- **16-bit:** idx 0x0000 is the first perfect quad, giving **(0.0, 0.0)**. Sprites then sample `s = k + 0.5 + 0 − 0.5 = k`, so they stay crisp: the calibration compensates. But all other geometry is off by half a pixel, and the result differs from real hardware.
- **32-bit:** the result is (0.5, 0.5), so sprite pixel k samples `s = k + 0.5`. That is exactly between two texels, giving a 50/50 blur on every UI element.

**Therefore the translation must emulate D3D7 centres.** Map XYZRHW positions to `x' = x + 0.5`, `y' = y + 0.5` in D3D12 pixel space: `NDC = ((x+0.5)·2/W − 1, 1 − (y+0.5)·2/H)`. Integer-aligned coverage is unchanged, and attributes are evaluated at the D3D7 centres.

**Pitfalls:**
- If LINEAR is ignored and the translation point-samples, every quad is perfect from idx 0 under both rules. The result is (0, 0), and sprites then sample on texel boundaries.
- Do not rely on the GPU's own B5G6R5 conversion of near-ties such as 62.508. Do the 565 conversion in our own code.

## 4. Caching and modes

- **Not cached.** No registry or file I/O; `0x6B7968`/`0x6B796C` are referenced only by the writer and the two readers. The registry (`HKLM\Software\Gravity Soft\Ragnarok`: `ISFULLSCREENMODE`, `WIDTH`, `HEIGHT`, `BITPERPIXEL`, `L458363-458381`) only selects the mode.
- **Runs once per start,** including windowed and fullscreen, 16- and 32-bit. It is not re-run on device loss or mode change: `FUN_00404ed0` has a single caller.
- **When it is inert:** in 24/32-bit, and in windowed mode on a 32-bit desktop, the outcome is always (0.5, 0.5). If BeginScene or Lock fails, the globals keep their `.data` value of 0.5.
- **Edge cases:**
  - **W or H not a multiple of 4** (e.g. the ROExt window 1600×863): `rb` is 1600×864, and the NULL-rect `Blt` stretches the image. The DirectDraw stretch semantics are unverified. This only matters in 16-bit.
  - **Stage 1 with no texture bound** must act as a pass-through. Wine's `is_invalid_op` turns such an op into `SELECTARG1(CURRENT)`. D3D7 driver behaviour is unverified. If the stage returned white, every pixel would saturate and the result would fall back to (0.5, 0.5).
  - **Black texels are 0x0000, i.e. alpha 0.** Colour-key or alpha emulation must not discard them.
  - **Minimum texture size:** report `dwMinTextureWidth/Height` ≤ 4. Otherwise the texture grows to 8×8 and the CLAMP edge reads unwritten texels.
  - **Throughput:** 65,536 four-vertex `DrawPrimitive` calls, plus one synchronous readback per pass. 640×480 needs 4 passes; 1024×768 needs 2.

## 5. Test recipe (CPU reference rasterizer)

**Setup**
- **Render target:** 1024×1024 R5G6B5. With cols = rows = 256, one pass covers all quads, and quad `idx` sits at (4·lo, 4·hi). Also run 800×600 to exercise the pass loop: 30000 + 30000 + 5536 quads.
- **Texture:** 4×4 ARGB1555 with rows `0000 0000 FFFF FFFF` ×2, then `FFFF FFFF 0000 0000` ×2. It decodes to white = 1.0 and black = 0.
- **States and vertices:** exactly as in §1.
- **Reference model:** D3D7 pixel centres at integers, top-left rule, `s = u·4 − 0.5`, exact bilinear with CLAMP, float→565 rounded to nearest, no dither.
- **Per-pixel value:** `c = (R5<<3) + (G6<<2) + (B5<<3)`. Add it to `bright` when the texel is white, otherwise to `dark`.

**Expected per-quad sums**

| idx | bright | dark | Accepted |
|---|---|---|---|
| 0x8080, 0x7E80, 0x807E, 0x7F7F | 5984 | 0 | yes (maximum contrast) |
| 0x8282 | 5984 | 4 | yes |
| 0x0080, 0x8000 | 5256 | 768 | no |
| 0x4040, 0xC0C0 | 5216 | 672 | no |
| 0x0000 | 4528 | 1152 | no |

**Expected pixel words**
- **idx 0x8080 and 0x7E80:** identical to the texture.
- **idx 0x0000:** `0000 0000 8410 FFFF | 0000 0000 8410 FFFF | 8410 8410 8410 8410 | FFFF FFFF 8410 0000`. With floor conversion, 0x8410 becomes 0x7BEF.

**Expected final results**

| Variant | Quads passing | Result idx | g_texOffU | g_texOffV |
|---|---|---|---|---|
| Reference (round) | 3017 (best 5984) | 0x7E80 | 0x3F000000 = 0.5 | 0x3EFC0000 = 0.4921875 |
| Floor conversion | 2153 | 0x8080 | 0.5 | 0.5 |
| D3D10 centres (negative test) | 755 | 0x0000 | 0.0 | 0.0 |
| X8R8G8B8 target (per-pixel sum is G8+B8 only) | 0 | 0x8080 | 0.5 | 0.5 |
| WRAP instead of CLAMP | 833 | 0x7E80 | — | — |

The WRAP row gives the same result index, so assert the pass count to catch it.

**Suggested policy (a recommendation, not a hardware fact):** do the 565 readback conversion deterministically with floor. 16- and 32-bit then both yield (0.5, 0.5), which matches the shipped default and every 32-bit D3D7 card.
