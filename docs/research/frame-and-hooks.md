# uaRO.exe: the frame, where it spends its time, where to hook

Binary: `/root/BornRok/winEXE/uaRO.exe` (2007-05-07, MSVC 6, image base 0x400000, `.text` 0x401000-0x694000). Function
names are Ghidra's (`FUN_<address>`) in `/root/BornRok/winEXE/decomp/uaRO_decomp.c`. Profile numbers come from the
driver's `[debug] profile` (the game's thread sampled about 1000 times a second) in 11 reports of 2026-10-18: the
developer's RTX 5060 Ti (Direct3D 11 and 12) and a tester's Radeon HD 6570 (Direct3D 11, 1600x900).

## Threads

The whole game runs on one thread. The client's own other threads: the map's model loader `FUN_004ea130` (started by
the map load `FUN_004ea9a0`, waited for on `DAT_0074a874`), a pipe reader `FUN_0065e280`, `FUN_00659c60`, and an effect
demo `FUN_004d8d70`; Miles and Bink keep threads of their own. The C runtime is the multithreaded one (its locks at
0x674589), and the resource manager guards itself with a critical section (`FUN_005131f0`, at `this+0x40`).

## The frame

`WinMain` is `FUN_006555f0`; it sets `timeBeginPeriod(1)` at the start. Each turn of the loop runs
`CGameMode::OnRun` = `FUN_00559750`:

1. `FUN_00654100` - the window messages.
2. `FUN_00579900` - the network: `select`/`recv` and a 2000-line packet `switch`.
3. `FUN_00566c80` - input. Then the game's logic in fixed ticks: `FUN_00421e90` counts the ticks gone by
   (`(now - last) / DAT_0070ec10`) and each tick updates the actors (`FUN_004eb930`), the water (`FUN_004e3b80`) and
   the world (`vtbl+0x10`).
4. Drawing: the world (`vtbl+0xc`) fills lists of faces (RPFace), the interface `FUN_0055b5a0`, then `FUN_0040d8d0`:
   `BeginScene`, the flush `FUN_0040e4d0` (sorts its lists with `FUN_00415a80`, `FUN_00415c50`, `FUN_00416160` and
   draws each face with one `DrawPrimitive` through `FUN_0040bd90`; the lists sit at renderer `+0x9c`, `+0xac`,
   `+0xbc`, `+0xcc` ... `+0x16c`), `EndScene`.
5. `FUN_004111e0` - the cursor; `FUN_0040fdb0` - locks and unlocks the render target (reading nothing) and presents
   (`FUN_00404bc0`).

The client has no frame limiter (`FUN_00421e80` is empty): it runs as fast as its present lets it, so the driver's
present sets the pace.

## Where the time goes

The client's own code is 4-9 % of its thread on both machines: about 1.2 ms a frame at 60 fps on the RTX machine and
2-2.5 ms at 20-25 fps on the HD 6570. The rest is the driver (4-5 ms a frame since 2.5.11) and waiting for the GPU.

| function | what it is | share of the thread |
|---|---|---|
| `FUN_004176c0` | `CTexture::Update`: ARGB to 16 bits, then every mip level again by stretching Blts (`FUN_004175d0`) | 0.4-2.4 % |
| `FUN_0040b250` | ray-triangle test, from the ground's height (`FUN_0042a9a0`) and the cell pick (`FUN_004e6800`) | 0.2-4.5 %, 20 % in a map's first seconds |
| `FUN_00412cb0` | `ProjectVertex`, every vertex on the CPU | 0.1-0.45 % |
| `__ftol` | float to int by changing the x87 control word; starts at 0x671970 (Ghidra says 0x671980), 931 calls | 0.05-0.3 % |
| `FUN_004de9f0` | a model node's drawing: each face rebuilt every frame into `DAT_00731150`, colours through `__ftol` | ~0.2 % |
| `FUN_004fa8b0`, `FUN_004eb930`, `FUN_00403a40` | culling, the actors, the tiles of big pictures | 0.1-0.3 % each |

So the client does not slow a frame down. Its stalls are loads on its own thread: the resources through `FUN_005131f0`
and the GRF read `FUN_00510fd0` (DES for flags 2 and 4, `uncompress` from cps.dll); the textures through
`FUN_0049d830` to `FUN_0042b300` (`.bmp` `FUN_0042b610`, `.tga` `FUN_0042bcb0`, `.jpg` `FUN_0042bf50` with ijl15);
the sprites through `CSprRes::Load` `FUN_004385f0`, which makes its RGBA frames as A4R4G4B4 textures
(`FUN_0040c440(width, height, format, ...)`: 0 A1R5G5B5 with the key 0xfc1f, 1 A4R4G4B4, 2 R5G6B5, 4 32 bits).

## Where to hook, best first

The driver replaces DirectDraw through the import of `DirectDrawCreateEx` and changes no byte of the game's code; that
stays the base. Points inside the executable:

1. `FUN_004176c0` `CTexture::Update(this, x, y, w, h, ARGB*, key, pitch)`: every texture's 32-bit pixels before the
   client cuts them to 16 bits - full colour (no banding in skies and shadows) and no conversion or mip Blts on the
   game's thread.
2. The sprite cache's write (`sprite_exe::kWriteCall`) and `FUN_004385f0`: true-colour sprite frames instead of 16-bit
   ones.
3. `FUN_00510fd0` / `FUN_005131f0`: the driver's own GRF cache, filled ahead on its own thread - no disk and no inflate
   on the game's thread.
4. `FUN_0042bf50`: the map's JPEG textures decoded on every core when the map starts.
5. `__ftol` at 0x671970 (39 bytes): `fisttp` where the processor has SSE3.
6. `FUN_0040fdb0`: leave out the frame's useless lock.
7. The flush `FUN_0040e4d0`: take whole lists instead of 3-5 thousand COM calls. Ties the driver to this build.

## More threads

Without rewriting the client, only the loads (points 3 and 4) can leave its thread; the logic's ticks, the lists of
faces and the vertices' projection share one `OnRun` and its globals. The driver's part of a frame already has a thread
of its own (Direct3D 11's, or the 64-bit renderer for Direct3D 12). The large step would be drawing the world itself
from RSW, GND and RSM (the driver reads them already) with the client's camera (`CView+0x94`), hooking
`FUN_004de9f0` and the ground's drawing, and leaving the client its sprites, effects and interface.

## Built

- 2.5.18: point 1 - `CTexture::Update` through its vtable entry (`win/texture_hook`): the picture's own 8-bit texels
  drawn where the 16-bit ones are still theirs (`core/full_colour.hpp`). 2.5.20: the driver makes the whole update as
  the game does - its fields, sizes, steps, lock, rows, unlock, its own mip function - with the rows by SSE2
  (`core/texture_exe.hpp`, the same bits: 3.5 times the speed of the same loop on 512x512); `[debug] fast_textures`.
- 2.5.20: point 5 - `__ftol` by `fisttp` where the processor has SSE3 (`win/ftol_patch`, `[debug] fast_ftol`).
- 2.5.20, the driver's own part of the game's thread (report R-82's profile: 9.3 % of it): the effects' faces in a
  flat table cleared by the frame's stamp (a map's nodes were made and freed every frame), crisp text's windows
  compared a row at a time where nothing changed, the frame's record written for the bridge with inline writes.

Left: point 2 (true-colour sprite frames), 3 and 4 (loads off the game's thread), 6 and 7. `ProjectVertex` stays the
game's: its two divisions a vertex are most of its cost, and an SSE copy would round the depth differently.
