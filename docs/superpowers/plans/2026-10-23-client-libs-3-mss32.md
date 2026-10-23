# Client Libraries, Step 3: mss32.dll Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (inline, the user's way); self-review.

**Goal:** our mss32.dll in place of RAD's Miles Sound System 6 - the 46 `_AIL_*@N` uaRO.exe takes (and
`_AIL_set_sample_pan@8` the driver looks up) over XAudio2 2.9, the music's mp3 streamed by minimp3; mp3dec.asi no
longer used (kept for the switch back) - 3.0.1-beta.7 on «Бета».

**Spec:** `docs/superpowers/specs/2026-10-23-own-client-libraries-design.md`.

## What the game does with Miles (uaRO.exe, read 2026-10-23)

- Start (0x4214F0): voices 48/32/16 (setup.exe), `set_redist_directory(".\\")`, `startup`, `set_preference(15, ..)`,
  `open_digital_driver(rate 22050/11025/8000, bits 16/8, channels, 1)` (NULL: no sound). Mode 1: 2D sample handles;
  mode 2: `enumerate_3D_providers` until "Miles Fast 2D Positional Audio", `open_3D_provider`, speaker type
  (-1: unsupported), `open_3D_listener`, 3D sample handles. Shutdown (0x4217C0) releases all in reverse.
- A sound (0x421AF0, 2D): a handle whose `sample_status` is 2 (done); `init_sample`, `set_sample_file(image, 0)`,
  `set_sample_volume(0-127)`, `start_sample`. 3D (0x421C20): `3D_sample_status` 2; `set_3D_sample_file` (0: failed),
  effects level, `set_3D_position` (relative to the player), `set_3D_orientation`, volume, `set_3D_sample_distances(max,
  min)`, `start_3D_sample`. No provider: the 2D one.
- Music (0x4219C0): `open_stream(driver, "BGM\\..mp3", 0)`, `set_stream_volume`, `set_stream_loop_count(0)` (forever),
  `start_stream`; `pause_stream`, `stream_volume`, `close_stream`.
- A sound's file (0x439900): `file_type` 1 PCM kept; 2 ADPCM: `WAV_info` + `decompress_ADPCM` (a new WAV image);
  13 MP3: `decompress_ASI`; freed by `mem_free_lock`. data.grf: 2105 PCM, 5 IMA ADPCM, 1 MS ADPCM, 1 mp3.

## Tasks

1. Vendor minimp3 (CC0, lieff/minimp3 ea99364f); core/miles: `fileType`, `wavInfo`, `decodeAdpcm` (IMA 0x11, MS
   0x02), `makeWav`, `decodeMp3`, `Mp3Stream` (rewind for the loop); core/miles mix: `gainOf`, `panGains`, `place3D`.
   Tests: `tests/test_miles.cpp`. Check: every sound of data.grf decodes.
2. mss32.dll (src/clientlibs/mss32*.cpp, .def): the driver, 2D and 3D samples, the listener, streams (a thread),
   the utilities; checker table; CMake; package.
3. Installed only where XAudio2 2.9 is (Windows 10+): client_libs entry `mss32.dll`/`mss` needing it; the app's
   system check (fake flag); tests; switch, texts, template.
4. Notes, self-review, ship-beta; the build's handoff.
