# A super-light mode for the weakest graphics cards

Asked for on 2026-10-19: as light as it can be, flat colours instead of textures accepted.

## Where a frame goes on a weak card

A tester's Radeon HD 6570 at 1600x900 drew 35 frames a second with every improvement off (Scroll Lock): about 27 ms of
GPU work for the game as it is. A 2007 game should cost such a card 5-8 ms.

- The shader. The one draw shader for every state is about 1200 instructions and 136 branches in DXIL; a sprite needs
  43 (2.5.16 measured both with DXC). A frame at 1600x900 shades 4-7 million pixels (the game's overdraw is 3-5), and
  old VLIW cards (TeraScale) pay for every branch. 2.5.16 makes a shader for each state the game uses; the base should
  fall to 6-10 ms - the tester's report will tell.
- Pixels. The rest is fill: the world with its overdraw, the water (blended), the skills' effects (large blended
  sprites), the interface. It grows with the pixels: `world_scale` 50 % draws a quarter of the world's.
- Memory bandwidth. The HD 6570 has 12.8 GB/s of DDR3, and integrated graphics share the processor's memory. Each
  pixel reads its texture (4 texels bilinear, twice that trilinear, up to 4 times more with anisotropy 4 - the
  tester's setting), writes its colour (4 bytes) and its depth (4 bytes, D32F).
- The processor is not the limit there: the client takes about 2-2.5 ms a frame, the driver about 4.5 ms, and
  Direct3D 11 draws on a thread of its own.

## What the mode strips

| step | gain | price |
|---|---|---|
| a shader for each state (2.5.16) | x2-4 of the GPU's time on old AMD and Intel cards (to be measured) | none |
| `world_scale` 50-60 % (2.5.16) | x3-4 fewer pixels of the world | a softer world, the interface sharp |
| 30 frames a second | x2 of everything | smoothness |
| flat colours instead of the world's textures | one texel a pixel, no cache misses: +10-30 % where bandwidth is short | a clay or paper look |
| a 16-bit world target (B5G6R5) and depth (D16) | half the colour and depth writes: +10-20 % there | banding in gradients; rare depth faults far off |
| anisotropy 0, no trilinear filtering | up to 4 fetches a pixel instead of 16 | a blurrier ground far off |
| no CAS sharpening, no frame counter, no AI pictures, no xBR windows | 0.2-1 ms each | small |

What it cannot fix: the game's own overdraw (its sprites and effects stay, or skills become invisible) and truly old
processors.

## Flat colours almost for free

The world's textures have mip levels (the client builds them; the driver keeps them). Each gets a second shader
resource view limited to its last level, 1x1 or 2x2 - its average colour: no extra memory, the same shader. The ground's
lightmap (its second stage) and the models' vertex colours stay and give the shape: shadows, the lamps' light, the
faces' shading. Textures with a colour key or alpha (leaves, grilles, effects) keep a 16x16 level, or their shapes go;
sprites and the interface are not touched; the water flat or as it is. The result looks like a low-poly stylisation in
the map's own palette. A 16-bit world target needs a fallback where the card cannot render to B5G6R5 (optional at
feature level 11_0). About a day each in `renderer11`.

## The preset

"Super-light" in the settings app and the console: effects off, AI pictures off, `world_scale` 50, anisotropy 0, no
trilinear filtering, flat colours, 16-bit targets, no CAS, no frame counter, an optional 30-frame cap; all applied in a
running game, Scroll Lock still showing the game's own picture.

## Order

First the tester's 2.5.16 report: Scroll Lock against the old 35 frames (the shaders' gain) and `world_scale`
100/75/50 (the share of a frame that follows the pixels). With the base still at 15 ms or more, flat colours and 16-bit
targets are needed; at 6-10 ms they are a matter of taste.

## Built (2.5.20)

`[quality] super_light` (the Quality page's first box), over the other settings while it is on and given back when it
ends, in a running game (`core/light_mode.hpp`, applied in `Context::present`):

- every effect off (`effect_groups::atMost(.., 0)`), the AI pictures not drawn - and not read at all when the game
  starts with it on;
- `world_scale` 50 %, `render_scale` 100 % at most, no anisotropy, sharpening, MSAA, TAA or supersampling;
- Direct3D 11's own (`Renderer::setLight`, taken at a frame's start): the ground's and the models' textures from their
  last mip level - a view of it, no memory - or, with transparent texels, the last of 16 texels a side
  (`light::flatLevel`); sprites, effects and the interface as they are; the world's set in B5G6R5 with D16 depth where
  the card can draw into, blend and sample B5G6R5 (else 32-bit); mip levels without blending.

Not done: a 30-frame cap (the frame limit stays the player's), Direct3D 12's flat textures and 16-bit targets (the
weakest cards are Direct3D 11's). No measurement yet on the weak card.

Fixed after the first test (RTX 5060 Ti, 4K screen, Direct3D 11): 2.5.21 - a black world on the maps of a region, the
last frame frozen after Scroll Lock: the night pass ran with the region colours off and copied the 16-bit world into a
32-bit copy (nothing copied); it runs only when it changes the picture now (`region_look::changes`), and the copies
take the picture's format (`Renderer11::copyScene`). 2.5.22 - the world made larger plainly (`light::kPlainUpscale`,
one bilinear tap): its own sharpening (Catmull-Rom under CAS, 45 taps a pixel) cost about 1.6 ms of the 2.2 there,
ten times the world. A client whose setup turns mipmaps off (one level a texture, as on that PC) keeps its textures:
flat ones need the mip levels.
