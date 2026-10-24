# Driver features 2, 4, 5, 6, 7, 8 (3.0.4-beta.3 on)

The user (2026-10-24) chose six of my ideas: «делай 2,4,5,6,7,8» - notifications in the background, buff timers,
camera and a photo mode, world sounds, Steam Deck / Linux, the rest of the several-windows ideas. Built inline, one
beta each, no review stops; releases only on «выпускай». Facts from uaRO.exe (sha256 63902c46..., Ghidra decomp
/root/BornRok/winEXE/decomp/uaRO_decomp.c).

## Facts

- Packets: CGameMode's loop FUN_00579900 reads each zone packet with `call 0x419350` at 0x579b02 (thiscall: ECX the
  connection FUN_00419480, one argument - the buffer [ebp-0x54b4]; returns the id in ax), then switches on the id and
  passes the same buffer to the handlers. Our account id: DAT_00771f7c.
- Status icons: ZC_MSG_STATE_CHANGE 0x196 <icon>.W <id>.L <flag>.B (FUN_00593d00); the player's icons in the vector
  0x728484..0x728488 (int ids). FUN_005d56a0 lays them out: x = screen width (0x6ee3d8+0x24) - 32 - 35 per column,
  y = 150 + 35 per icon, a new column past height - 50; five passes by FUN_00648bc0(id, pass).
- Camera: zoom = view (CGameMode+0xc8) +0x3c, wheel step 24.3, clamp FUN_00565c00: min 230 (0x6d66b0), max 2040
  outdoors (0x6d66b4) / 300 in buildings (0x6d66b8) by CGameMode int[0xa0] (0 = map in data\indoorrswtable.txt, 101
  building maps; dungeons are "outdoors"). The same flag frees the yaw (else 45 +-20) and the tilt (k 42 vs 10).
  Projection far plane 1500 (SetSize 0x40d760).
- World sounds: 563 of the 1255 maps hold 25,949 sound objects (RSW type 3: name, wav, place, volume, size, range,
  cycle - birds, brooks, waves, wind, insects, frogs, waterfalls, cave drops), 119 files, all in data.grf. The 2007
  client loads them into a list (FUN_0043a200 case 3, world [0x4b]) and never plays them.
- Durations: /root/uAthena (the server's source): db/skill_cast_db.txt durations by level, status.c icon table,
  item_db scripts.

## Rulings (mine; the user may overturn any)

1. Packet tap (foundation): the call at 0x579b02 redirected to a thiscall wrapper after a byte check; the wrapper
   calls 0x419350 and gives (id, bytes) to core parsing. Independent of uaRO Pad.
2. Notifications (beta.3): only while the game window is not in front - whisper, party invite, trade request, guild
   invite, the character's death; the taskbar button flashes and a Windows notification (a tray icon's balloon,
   shell32 by name) shows who and what. [notify] on + a box per kind. The settings app's language.
3. Several windows (beta.4): a background window's events (death, whisper, HP under 30 %) marked on its card in the
   windows' panel of the main window; «Грати» starts a set of N windows ([windows] start_count 1-8, 2 s apart).
   No logins stored.
4. Camera (beta.5): [camera] indoor = 1 - building maps get the outdoor freedom (rotation, tilt, zoom to 2040; the
   flag int[0xa0] kept at 1); smooth_zoom = 1 - the wheel's steps eased over 150 ms; far plane pushed past the
   farthest zoom only if the world is cut there (checked first). Photo mode Ctrl+Shift+F11: the game's interface, the
   cursor, the panel and the counters hidden; Ctrl+Shift+F12 then shoots the picture without them.
5. Buff timers (beta.6): a table generated from uAthena (icon -> skill/item -> duration by level); the player's own
   cast on self gives the level, another's buff or an item the longest; the time left drawn under each icon at the
   game's own layout. [buffs] timers = 1.
6. World sounds (beta.7): the maps' own sound objects played by the driver's XAudio2 engine (the chosen output
   device), each looping by its cycle within its range, quieter with the distance, at most 8 at once, the game's
   effects volume times [sound] ambience_volume; [sound] ambience = 0 by default (new effects ship off).
7. Linux / Steam Deck (beta.8): Wine detected (ntdll wine_get_version) and logged; paths Wine lacks avoided
   (DirectComposition presentation, Windows' spatial sound); a guide written. Testing needs a Linux PC or a Deck.
