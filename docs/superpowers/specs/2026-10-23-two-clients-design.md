# Two clients side by side - design

Status: approved 2026-10-23 («норм»). Source: the WSL client build's report to "the launcher session" (also
`C:\Games\uaRO-build\LAUNCHER.md` on the user's computer).

## Intent

The new full client is a separate client, «uaRO HD» (its manifest says `"client": "uaRO HD"`, the archive's folder is
`uaRO-HD\`), installed NEXT TO the old uaRO, both with uaRO Graphics. Today every install takes the same names and the
same patch list:

- the desktop shortcut is always «uaRO» - the copy installed later takes it over;
- Windows' Apps has one key, «uaRO Graphics» - every update points it at its own folder, and a removal deletes it
  whichever folder it names;
- the site's «Оновлення клієнта» has one list - a file uploaded for one client goes to both (the HD client's
  `data.grf` is newer than the old one's);
- the patcher would silently replace files a player changes on purpose (`Palettes.grf` made by «Палітри гри»,
  `dinput8.ini` - ROExt's window), never takes away a file the admin withdrew, and checks only `uaRO.exe`.

## Decisions (approved)

1. **Which client.** The build puts its own manifest into the client's root as `uaro-client.json` (the shared format 1:
   `client`, `version`, `files`; it does not list itself). No file (the old client, any 3.0.0 install): «uaRO».
2. **Names.** The desktop shortcut is the client's name («uaRO», «uaRO HD»); the Apps entry is «uaRO Graphics» for
   the old client and «uaRO Graphics (<client>)» for another («uaRO Graphics (uaRO HD)»). A copy never overwrites a
   shortcut or an Apps entry of another copy that is still there; a removal deletes only its own entry.
3. **Patch lists per client.** The site keeps a client per file (uaRO, uaRO HD); `GET /api/public/client-patch?client=
   <name>` answers that client's list (no `client`: «uaRO», what 3.0.0 asks). The app asks for its own client and
   refuses a list of another.
4. **The player's own files.** `Palettes.grf` and `dinput8.ini` (the game's root, any case) are put only when missing
   or still as the build or the last patch left them; otherwise they stay as the player made them.
5. **Withdrawn files.** A file of the last put list that the new list no longer has is deleted only when the patch
   made it (nothing was at its place before), it is unchanged since, and the build does not list it.
6. **Closed first.** Before putting or deleting: `uaRO.exe`, `uaro-graphics\uaro-render64.exe` and
   `uaro-graphics\uaro-studio.exe` of THIS folder are not running (by full path - the other client may be open).
7. **Still never patched:** DATA.INI, uaRO Graphics' files, the player's folders, programs - and now `uaro-client.json`
   (site and app). A new GRF in DATA.INI means a new full client, not a patch.

## My calls (marked for the user)

- A second copy of the same client: its shortcut «uaRO (2)» (up to (9)); its Apps entry is not written while the
  first copy holds the name and is still there (a stale entry - its folder gone - is taken over).
- An unpacked client never installed (the full client): the app's start makes its shortcut once (as the installer
  does) and keeps its Apps entry right at every start (also renaming an entry 3.0.0 wrote under the old name).
- The patch record (`uaro-graphics\client-patch.txt`) keeps the client and, per file, whether the patch made it; it is
  written also when nothing had to be put (no hashing of the same list at every start).
- Release: 3.0.1-beta.1 on «Бета»; 3.0.1 on «выпускай». The HD client is published only with 3.0.1 or later inside
  (3.0.0 would name an HD install «uaRO» at its first update) and with `uaro-client.json`.

## Not in this

- Asking the player about a changed `Palettes.grf` (kept silently, logged by nothing yet).
- Full-client version upgrades by manifest (download a new build's changed files): patches stay loose files from the
  admin; DATA.INI is never changed.
