# The client's graphics updates (updates\ GRFs) - design

Status: approved 2026-10-23 in chat (the user: «grf в отдельную папку нравится», «норм, делай»).

## Intent

The admin tests new content in his client, packs it into a GRF of his own and uploads it to the site; every player's
game must show it, without the player doing anything, without repacking the game's archives and without DATA.INI.

## What the game does (uaRO.exe 63902c46, read 2026-10-23)

- `readfolder` is forced on (sclientinfo's test patched to NOPs at 0x4F31FE, 0x50D346): CFile::Open (0x50D310) reads
  a loose file `data\...` first (CreateFileA, VirtualAlloc), and only when there is none asks the archives.
- The archives: CFileMgr at 0x74BA18, a list of paks (DATA.INI's [Data], 9 down to 0). `CFileMgr::GetData`
  (0x50D120, thiscall: name, &size) walks the paks, VirtualAlloc(size + 8) and reads the first hit; the game frees it
  with VirtualFree (0x50D080). `CFileMgr::IsExist` (0x50D0A0, thiscall: name) and `CFile::IsExist` (0x50D690, cdecl:
  name) look in the paks, then on disk. The names arrive normalized: "data\" + the game's own (0x50D720).
- Prologues (9 bytes, whole instructions): 55 8B EC 81 EC 18 02 00 00 / 55 8B EC 81 EC 40 01 00 00 /
  55 8B EC 81 EC 40 03 00 00.
- `event.grf` is mounted only by `<event>` in sclientinfo.xml - the admin's file: not used.

## Decisions

1. **The folder:** `<client>\updates\`, the admin's GRFs as they are (version 0x200, what GRF Editor and GRF Builder
   make). Delivered by the site's «Оновлення клієнта» as `updates/<name>.grf` for a client - no change there (paths
   with folders and files up to 1 GB already pass); withdrawn ones are deleted by it as any.
2. **The order:** by file name, the later name wins over the earlier ones (date names: `2026-10-25.grf`).
3. **Priority (my call, changed from the chat's «even over the loose files»):** the game's loose `data\` files first
   (as the game reads them now), then updates\, then DATA.INI's archives. The admin tests new content as loose files:
   an older update must not hide them; players have none.
4. **How:** the driver hooks the three functions above at its load (their prologue bytes checked; another build: not
   mounted, said in the log); the archives open at the first lookup (outside the loader's lock), read-only, a lock
   around reads; a case-insensitive index (ASCII letters only, CP949's two-byte characters untouched, '/' as '\'),
   the later archive winning.
5. **The driver's own readers see the same files:** its map loader (the ground, grass, lights: data folder, then
   updates\, then DATA.INI's archives); the pack builder and uaRO Studio's catalog take updates\ first too.
6. **A switch:** `[client] updates` (on), «Сервіс» → «Оновлення графіки клієнта» with what the folder holds; read as
   the game starts.
7. **The log:** the archives mounted, each with its file count; one not mounted and why.

## Non-goals

No repacking at the player, no change to DATA.INI or the game's archives, no partial downloads inside a GRF, no
encrypted (DES) or other-version GRFs (said in the log), no change to the site.
