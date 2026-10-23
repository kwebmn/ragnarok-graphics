# Faster game start and HD textures - implementation plan

**Goal:** the game's first picture 1-2 s after its start instead of 12-15 s, and the texture pack's pictures on a
map without a frame waiting behind a big texture's decoding.

**Spec:** the client build session's doc «Ускорить запуск игры и загрузку HD-текстур в uaRO Graphics»
(claude.ai/artifact/2i6ZqoccqfyqhL6nHSKtvP, 2026-10-23), from the user's two logs (3.0.1-beta.9 and 3.0.1: i7-8700
6/12, RTX 5060 Ti, 3840x2160) and the user's uaro-textures.zip (306 749 pictures, 6.1 GB, every entry stored).

**Architecture:** uaro-render64 sends the driver its Hello only after `Renderer::startAiTextures`, and the game
waits for the Hello. `AiTextures::start` compiles five compute shaders with FXC at optimisation 3 on every start (the
main renderer keeps its own in `uaro-graphics\cache`), opens the pack, reads and parses its 28 MB index - all before
the Hello. The pack's pictures are read and decoded on one I/O thread, one request at a time.

## Global constraints

- The pack's format stays; no BC7 of the whole pack. uaRO.exe is not touched.
- Each task is its own beta (3.0.2-beta.7 ... beta.11) through ship-beta.sh; releases wait for the user's word.
- The 64-bit renderer (Direct3D 12) only; Direct3D 11's ready pictures stay as they are.
- The generating mode ([ai] generate = 1, the developer's) keeps its start as it is: its networks' self-test runs on
  the renderer's command list.
- Code comments English, lines up to 120, the gate before every beta.

## Task 1 (beta.7): an honest measurement

- `core/ai/timings.hpp`: `ai::Steps` (named steps' milliseconds, `line()`: "shaders 9512 ms (ai_conv 8650, ...),
  ...") and `ai::LoadTimes` (parts Queue, Read, Decode, Fit, Frame; `add(const double (&ms)[kLoadParts])`,
  `line(const char* what)`: "sprite frames 412 - queue 0.4 (12), read 0.3 (2.1), WebP 1.2 (9.8), fitted 0.1 (0.6),
  the next frame 7.9 (16)": average (most), milliseconds).
- `AiTextures::start`: QueryPerformanceCounter around createGpu (each shader: compiled and its pipeline), the weights,
  the self-test, Reader::open, the index's read, parseIndexFile, ReadyPictures with addPack; one line
  "AI textures: started in N ms - ...".
- A picture of the pack: its request's QPC time; the I/O thread times the queue, the read, webp::decode, the fitting
  (halve, keyAway, recolour) and when the result was put out; takeLoaded the wait for the frame. Kept apart for
  sprite frames, icons, the world's textures and the interface's; logNow writes them in place of "read in N ms on
  average".
- Tests: tests/test_ai_timings.cpp (formatting, average and most, nothing measured - no line).

## Task 2 (beta.8): the network's shaders only for the network, all of them kept

- `ai::shadersFor(bool network)`: {"ai_mip"} for the pack's pictures, all five for the networks (ai_conv, ai_shuffle,
  ai_alpha, ai_place only run the networks).
- createGpu(network): start() passes generate_, startForPack and startForStudio true.
- Their code kept between starts in `uaro-graphics\cache\ai-shaders64.bin` with core/shader_cache (key: the source
  with the settings), written back when one was compiled; a line "AI textures: shaders - N kept, M compiled".
- Check: from "draws on" to the pack's line under 1 s from the second start.

## Task 3 (beta.9): the pack opened alongside the game

- Pack-only mode: start() does the settings, the shaders, the I/O thread - its first request opens the pack, reads
  and parses its index and lists its pictures (ReadyPictures) there, and hands the reader and the list over as a
  result; ready at once, the Hello goes.
- Until the pack is there nothing is decided "not in the pack": begin() keeps the texture (its pixels) and its kind
  for later, a sprite frame's job and an icon wait in lists of their own (capped); at the handover they are looked
  up, the textures noted meanwhile looked up ahead too. A pack that cannot be read: the line as before, AI off.
- Check: "renderer: the first picture is on the screen" 1-2 s after the start; the pack's pictures come by
  themselves, with no map entered again.

## Task 4 (beta.10): a pool for decoding

- The I/O thread reads (the pack's one FILE*), workers decode: webp::decode, halve, keyAway, recolour - half the
  logical processors (1-8), the most urgent first (sprite frames and icons, then the textures in view, then ahead).
- `ai::DecodeQueue` (core): push(rank, item), pop() the lowest rank, the oldest of it; the I/O thread reads on only
  while the queue holds fewer than twice the workers - a map's hundreds known ahead stay requests, a frame asked
  later still goes first.
- Check (task 1's line): a sprite frame no longer waits for a ground texture.

## Task 5 (beta.11): cps.dll on libdeflate

- Measure first: every entry of the game's GRFs inflated by miniz and by libdeflate on this host.
- third_party/libdeflate (MIT, a release tag), core/client_zlib's uncompress (and compress) through it, the zlib API
  as before (Z_OK, Z_BUF_ERROR, Z_DATA_ERROR, the out length).
- Check: "map: ..." to the map drawn on prt_fild08 (now about 3.7 s).
