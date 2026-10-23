# Our own libraries in place of the client's old ones - design

Status: approved 2026-10-23 (the user: «приступай без остановки в инлайн режиме»). Source: the client build session's
doc «Свои DLL вместо старых библиотек клиента uaRO» (the user's decision: replace the client's old DLLs with ours,
granny2.dll stays), checked against uaRO.exe (sha256 63902c46...) on loki.

## Intent

The client ships libraries of the 2000s - zlib in cps.dll, Intel JPEG Library 1.5, RAD Game Tools' Bink and Miles
(paid libraries we hand out in their old versions), ROExt. Everything works; the aim is code of our own that we can
fix, and no RAD binaries in the client.

## Facts (uaRO.exe's import table and its call sites)

| Library | What the exe takes | Convention | Notes |
|---|---|---|---|
| cps.dll (73 728 B) | `compress`, `uncompress` by name | cdecl (`add esp` after) | zlib 1.x: `compress` = deflateInit_ at level -1 |
| ijl15.dll (372 736 B) | ordinals 2 ijlInit, 3 ijlFree, 4 ijlRead, 5 ijlWrite | stdcall | JPEG_CORE_PROPERTIES 0x4E68 bytes, IJL 1.5 layout |
| binkw32.dll (358 963 B) | 11 `_Bink*@N` by name | stdcall | only `openning.bik` (not in the client); a NULL BinkOpen is handled |
| mss32.dll (346 624 B) | 46 `_AIL_*@N` by name | stdcall | the driver's sound engine (1.1, XAudio2) already plays the effects |
| mp3dec.asi | loaded by mss32.dll | - | BGM\*.mp3 as a stream |
| dinput8.dll (ROExt) | NOT imported by the exe | - | the exe imports DINPUT.dll; Windows' DirectInput loads dinput8.dll from the game's folder |
| mpglib.dll | imported by nobody | - | likely dead (GPL) |

ijlRead: the loader (0x42BF50) reads from memory - mode 1 (JBUFF_READPARAMS: JPGWidth +0x2C, JPGHeight +0x30), then
mode 3 (JBUFF_READWHOLEIMAGE) into a BGR DIB (DIBColor 2, 3 channels, rows padded to 4 bytes, DIBHeight > 0 =
top-down). ijlWrite: the Print Screen (0x411F72) - mode 8 (JFILE_WRITEWHOLEIMAGE), the file name at +0x20, BGR,
quality as ijlInit sets it (the struct is zeroed before ijlInit); no ijlFree after it.

ROExt's settings (its ini): MouseFreedom, RemapMouseButtons, RemapAltF4, KeyRemap, MinimizeKey, AutoFreeCPU,
WindowOnTop, WindowLock, WindowPosX/Y, WindowWidth/Height, CodePage. The build's own exe patch «repeat a phrase» is
at 0x5706D1 - not the driver's chat fix (0x574487-0x57448F).

## Decisions

1. **Four steps, a beta each, in this order:** (1) cps.dll over miniz (already ours, the GRF reader's) + a binkw32.dll
   stub (BinkOpen answers NULL); (2) ijl15.dll over stb_image (decode, already ours) and stb_image_write (encode);
   (3) mss32.dll - the driver's sound engine as its own DLL with the music stream (minimp3), mp3dec.asi no longer
   needed; (4) ROExt's work inside the driver (its window, mouse, keys, CPU and code page), dinput8.dll gone.
2. **uaRO Graphics puts them in, not the build:** the site's client patch never carries programs. The package holds
   them in `uaro-graphics\clientlibs\`; the install, an update and the app's start put ours into the game's root and
   keep the game's own in `uaro-graphics\original\`; the removal puts the originals back. The root never lacks a
   library the exe needs. The build leaves them out of its manifest (uaRO Graphics owns them).
3. **Each switchable** (the user: every part its own box): `[client] cps`, `bink`, later `ijl`, `mss` - on by
   default, «Сервіс» → «Бібліотеки гри»; off puts the game's original back (with the game closed).
4. Each DLL: 32-bit, the exact exports the exe takes (names, ordinals, conventions; checked by a tool in the gate),
   imports Windows' own DLLs only, a marker "uaRO Graphics" in its bytes (to tell ours from the game's).

## My calls

- miniz, not libdeflate, for cps.dll: already in the tree and reading these GRFs since 0.42; faster inflate later.
- stb for ijl15.dll, not libjpeg-turbo: the decoder is already ours; a full libjpeg-turbo is a large import.
