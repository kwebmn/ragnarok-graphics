# uaRO.exe: registry settings (for the settings utility)

Source: Ghidra decomp `/root/BornRok/winEXE/decomp/uaRO_decomp.c` ("L" = decomp line) plus checks on the
binary `/root/BornRok/winEXE/uaRO.exe` (objdump, raw .data reads). Build: hexed "2007-05-07a kRO Sakexe".
**[?]** marks inferences that are not fully proven.

## 0. Where and how the client accesses the registry

* **Root:** always `HKLM` (0x80000002). The path comes from `PTR_DAT_006bd38c`, set in FUN_004f26f0 (L203756/L203814).
  It is `Software\Gravity Soft\Ragnarok`. Only servicetype `china` (3) uses `...\RoClient` instead. uaRO's
  sclientinfo.xml uses `korea` (servicetype 0); service ids are parsed at L203895–204150.
  The exe is 32-bit, so on x64 the real key is `HKLM\SOFTWARE\WOW6432Node\Gravity Soft\Ragnarok`.
  A non-elevated, unmanifested client is UAC-virtualized. Its writes go to
  `HKCU\Software\Classes\VirtualStore\MACHINE\SOFTWARE\WOW6432Node\Gravity Soft\Ragnarok`.
  Reads check the VirtualStore first and fall back to HKLM per value (standard Windows behaviour, not from the decomp).
  The settings tool itself will not be virtualized, so it must open the VirtualStore path explicitly and use the 32-bit view.
* **Reads:** `RegOpenKeyExA(..., KEY_QUERY_VALUE)` → `RegQueryValueExA` → close. The client never checks the value
  type. It reuses one `cbData` variable for many DWORD reads (set to 4 once, e.g. L458359/L416759).
  - A non-DWORD value of more than 4 bytes gives ERROR_MORE_DATA, which enlarges `cbData` for later reads.
    A following REG_SZ value could then overflow a 4-byte global.
  - Rule: write every numeric value as **REG_DWORD (4 bytes)**.
  - Floats (camera, minimap zoom) are REG_DWORD values holding raw IEEE-754 bits. Example: -45.0f = 0xC2340000.
* **Writes:** `RegCreateKeyExA(HKLM, ..., KEY_ALL_ACCESS)` + `RegSetValueExA`. All 24 IAT references to RegSetValueExA
  were mapped to their functions (section 3). There are no calls through thunks.
* **Missing main key:** FUN_00654f60 returns 0 when `RegOpenKeyExA` fails (L458352–458356). WinMain then runs
  `Setup.exe`, waits for it and retries in a loop (L458886–458937). If Setup.exe is missing, this becomes an endless
  CreateProcess/Wait loop. The key must always exist.
* **When things are read:** the main key at start (L458886). The session init FUN_0061dd60 (L458978) reads Option, FOG and
  ShortCutList. UIRectInfo is read after D3D init (L459052). Per-character data is read at character select / map entry.
  No registry value is re-read while the game runs, so every change needs a client restart.
* **Window class and title:** both are `"2007-05-07a kRO Sakexe"` (`PTR_DAT_006bd388`, string at file offset 0x3b0).
  Use it, or the process name, to detect a running client. The korea build creates no mutex (`Global\Surface` mutex only for service ≠ 0/3, L458957–458970).

## 1. Main key values read at startup — FUN_00654f60 @0x654f60 (L458339–458478)

"Default" means the value used when the value is missing. Globals without an explicit default are BSS, so they are 0.

| Value | Type | Default if missing | Validation / normalisation | What it does (evidence) |
|---|---|---|---|---|
| ISFULLSCREENMODE | DWORD | 0 (BSS) | any non-zero = fullscreen. Only for service 0xF (vietnam) does a missing value send you to Setup (L458361) | Selects D3D init flags 7 or 6, the window style and the cooperative level (section 2). L458369, L459015 |
| WIDTH / HEIGHT | DWORD | **0** (BSS) | none | Fullscreen: SetDisplayMode size. Windowed: client-area size. 0 breaks start-up, so these are required (section 2). L458372–458378 |
| BITPERPIXEL | DWORD | **0** (BSS) | none | Only used as `ddpfPixelFormat.dwRGBBitCount` for SetDisplayMode in **fullscreen** (L459026, L3095). Ignored in windowed mode, where the desktop format is used. Use 16 or 32. The pixel code takes channel masks from the primary surface (FUN_00404570 L3125–3160), so 32 should work [?] |
| DEVICECNT | DWORD | 0 | — | **Never used by the client.** The global 0x773190 is only referenced in the loader. Probably Setup.exe's combo-box index of the device [?] |
| MODECNT | DWORD | 0 | — | **Never used by the client** (0x773194). Probably Setup's display-mode list index, not a count (the player has 20 with 1920×1080) [?] |
| ISVOODOO | DWORD | 0 | copied to 0x7735ec (L458451) | Used only in the ground renderer FUN_004de9f0 (L187383). When non-zero, ground triangles outside the device guard band are skipped (a 3dfx workaround). Guard band: device caps, or ±640/±480 around the screen (L3596–3607). Use **0** |
| SHOWTIPSATSTARTUP | DWORD | 1 (initialised .data) | anything other than 0/1 → 1 (L458436) | "Tip of the day" window opens once per session on the first map entry (L287399) |
| ISLIGHTMAP | DWORD | 1 (explicit, L458398–458400) | flag 0x7710b9 = (value == 1); other code tests `!= 0` / `== 1`, so use 0/1 only | Ground lightmaps (baked shadows and light). `/lightmap` toggles it for the session only (L300053); **never saved** |
| TRILINEARFILTER | DWORD | 0 (explicit, L458405) | — | If non-zero **and** the device reports LINEARMIPLINEAR filtering (caps bit 0x20), textures get mipmaps (DDSD_MIPMAPCOUNT, DDSCAPS_MIPMAP\|COMPLEX; L3568, FUN_004059d0 L4049). Saved by the client at exit |
| SPRITEMODE | DWORD | **2** (explicit, L458410) | 0 → half width + half height; 1 → half height only; 2 and any other value → full (L458411–458421). The Detail window clamps it to 0..2 (L98578) | Sprite (.spr) decode resolution (FUN_00439xxx, L48153–48205). **2 = high, 0 = low.** Saved at exit |
| TEXTUREMODE | DWORD | **2** (explicit, L458425) | 0 → textures 1/4 size; 1 → 1/2; 2 and other values → full (divisor 0x6e4bd8, L458426–458435) | The texture manager creates textures at w/div × h/div (FUN_0040c4f0, L9872). **2 = high, 0 = low.** Saved at exit |
| DEVICENAME | REG_SZ ≤ 40 B (buffer 0x28) | "" | — | Read into 0x773158, **never used** (informational only, written by Setup) |
| GUIDDEVICE | REG_BINARY 16 | all zeros | none | rclsid for `IDirect3D7::CreateDevice` (FUN_00404450). **Must be a real device IID.** Missing or zero → "Cannot init d3d OR grf file has problem." and exit (L459036). With IID_IDirect3DHALDevice {84e63de0-…} or IID_IDirect3DTnLHalDevice {f5049e78-…}, textures go to video memory (0x4000), otherwise system memory (0x800) (FUN_00404260, L2808–2859) |
| GUIDDRIVER | REG_BINARY 16 | zeros | none | The pointer to these bytes is passed to `DirectDrawCreateEx`. Zeros = primary display; this works with dgVoodoo on the players' PCs |
| SOUNDMODE | DWORD | **0** (BSS) | — | **0 = no audio at all**: Miles is not started and BGM is also off, because 0x6b7b54 = SOUNDMODE (L458475). **1 = 2D**: digital driver + 2D samples, volume falls with distance, no panning (FUN_00421af0). **2 = 3D**: Miles 3D provider with positional samples (FUN_00421c20). If audio init fails, WinMain shows "Audio System Init Failed" and **exits** (L459009–459013). 0 is the fallback for PCs without sound |
| SPEAKERTYPE | DWORD | 0 | — | Only in mode 2: `AIL_set_3D_speaker_type(provider, value)`, if the provider supports it (L27226–27229). By MSS 6 headers: 0 = 2 speakers, 1 = headphones, 2 = surround, 3 = 4 speakers (4 = 5.1, 5 = 7.1) [? from memory of mss.h] |
| DIGITALRATETYPE | DWORD | 0 | 0 → **22050 Hz**, 1 → **11025 Hz**, 2+ → **8000 Hz** (L27126–27134) | Output rate for `AIL_open_digital_driver` |
| DIGITALBITSTYPE | DWORD | 0 | 0 → **16-bit**, any other value → 8-bit (L27135–27140) | Output bits for `AIL_open_digital_driver` |
| NUMSAMPLETYPE | DWORD | **2** (explicit, L458467) | 0 → 48 voices, 1 → 32, 2+ → 16 (L27108–27116) | How many sample handles are allocated (2D or 3D), i.e. the maximum number of simultaneous sound effects |
| PROVIDERNAME | REG_SZ ≤ 1024 B | "" | — | **Effectively ignored.** In mode 2 the buffer is overwritten with the literal `"Miles Fast 2D Positional Audio"` before `AIL_enumerate_3D_providers` (L27166–27183); modes 0/1 never use it. Any value is harmless (Setup writes "No Provider"). If that provider cannot be opened, 3D mode plays no effects at all, because the fallback 2D handles are never allocated |

Notes on the sound driver call:
* It is `AIL_open_digital_driver(rate, bits, SOUNDMODE==1 ? 1 : 0, 1)` (L27123–27142).
* Whether the third argument means mono/stereo cannot be confirmed without mss32.dll [?].
* `AIL_set_preference(15, 0)` uses a global that is not loaded from the registry.
* The imports show this is MSS 6.x (3D-provider API).

### Other values in the main key

| Value | Type | Read / written | Meaning |
|---|---|---|---|
| FOG | DWORD | Read in FUN_0061cac0 (L416745), default 1, normalised 0/1. Forced to 1 if the `Option` key is missing (L416990). Written at exit (L417213) | Per-map fog from fogParameterTable (FUN_00567410). `/fog` toggles it (L299495) |
| CHECKSAVE | DWORD | Login window read FUN_00456f80 (L76679), value ≠1 → 0. Written by FUN_00457100 (L76824) | "Remember ID" check box |
| ID | REG_SZ (written with 25 bytes) | Read with cb = 0x19, written L76823. An empty, zero-filled ID is written when CHECKSAVE = 0 | Saved login ID (max 24 chars) |
| BITWISE | DWORD | Read and ignored (L76684); always written as 1 | legacy |
| PASSWORD | REG_SZ | Read (L76707) and **discarded**; the password box is cleared. Never written | legacy; a tool may delete it |
| NUMACCOUNT | DWORD | Read and written by the list window FUN_004563c0 (msg 0x50 with the value name; L344623/L345638). Default = servicetype (0) | Last selected entry (index) in the clientinfo `<connection>` list |
| NUMSERVER | DWORD | Same mechanism (L344957) | Last selected char-server index |
| CURSLOT | DWORD | Read FUN_0045a4c0 (L79306): page = v/3, slot = v%3. The page is reset to 0 if ≥ page count. The local variable is uninitialised when the value is missing. Written FUN_0045ae20 (L79811) | Last selected character slot (page×3 + slot) |
| BINKMODE | DWORD | Read FUN_0050b740 at start (L459078). Written only after `openning.bik` actually played. A key press sets it to 1 ('R' sets 0; L224841) | 1 = skip the intro movie. A no-op if no openning.bik exists |
| RunCount | DWORD | servicetype china only (L203787–203798) | n/a for uaRO |
| RAGPATH | REG_SZ | Written only when servicetype = vietnam (L286540) | n/a for uaRO |

## 2. WIDTH / HEIGHT / ISFULLSCREENMODE / BITPERPIXEL in detail

Window creation, FUN_00654b60 (L458162):
* The window client size starts as the .data defaults 640×480 (0x6e4bd0/0x6e4bd4).
* It is overwritten with WIDTH/HEIGHT **only when ISFULLSCREENMODE == 0** (L458376–458378).
* Then `AdjustWindowRect(style 0x02CA0000)` = WS_CLIPCHILDREN|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX: no resize border, no maximize box (L458203).
* `CreateWindowExA` at x = (SM_CXSCREEN−W)/2, y = (SM_CYSCREEN−H)/2 (L458206–458213).
  - Centering uses the client size.
  - There is **no clamping to the screen or work area**: a 1920×1080 window on a 1080p desktop gets its bottom cut off.
  - WndProc FUN_006541e0 has no WM_GETMINMAXINFO / WM_SIZE handling.

D3D init FUN_00403f30 (L2717) with flags `6 | (fullscreen ? 1 : 0)` (L459015):

* **Fullscreen**
  - `SetWindowLongA(GWL_STYLE, WS_POPUP)` + `ShowWindow(SW_MAXIMIZE)` (L2731).
  - Cooperative level `0xC13` = DDSCL_FULLSCREEN|ALLOWREBOOT|EXCLUSIVE|MULTITHREADED|FPUSETUP (FUN_00404260 L2871–2878).
  - `IDirectDraw7::SetDisplayMode(WIDTH, HEIGHT, BITPERPIXEL, 0 Hz, flags)`. 320×200×8 gets DDSDM_STANDARDVGAMODE (FUN_00404570 L3081–3097).
  - On failure: "Cannot init d3d OR grf file has problem." and exit.
  - Then a flip chain with one back buffer.
* **Windowed**
  - Cooperative level `0xC08` = DDSCL_NORMAL|MULTITHREADED|FPUSETUP.
  - Primary surface + clipper + an off-screen back buffer **sized to GetClientRect** (L3038–3075).
  - So the render size is the real client area. BITPERPIXEL is ignored (desktop format).
* In-game resolution globals: 0x6ebe1c / 0x6ebe20 (device +4 / +8) and renderer +0x24 / +0x28.
* **No validation** against a mode list, no minimum and no maximum anywhere. No compares with 640/800/1024 were found.
  The UI assumes at least 640×480, and its window validators fall back to defaults when windows don't fit (section 5).
* No runtime mode switch: no Alt+Enter, and the re-init FUN_006553a0 has no callers. In fullscreen, WM_ACTIVATE minimizes or
  maximizes, and WM_SYSCOMMAND move/minimize are blocked (L457976).
* The client **never writes** WIDTH, HEIGHT, ISFULLSCREENMODE, BITPERPIXEL, DEVICE*, GUID*, ISVOODOO, ISLIGHTMAP or any
  SOUND value. Only RegQueryValueExA references exist (section 1 lines), and no RegSetValueExA call site uses these names.
  These values can therefore be edited while the game runs (they take effect next start) and the game will not overwrite them.

## 3. Everything the client writes, and when

| When | Function (addr, L) | Key / values |
|---|---|---|
| **Clean exit** (main loop returned, e.g. window closed or Alt+F4; not on crash or kill) | WinMain L459086 FUN_004d09b0 @0x4d09b0 (L176816) | `UIRectInfo`: all values, dumped from g_windowMgr 0x727ed8 |
| same, right after | WinMain L459087 **FUN_0061d890 @0x61d890** (L417176) | Main key: **SPRITEMODE, TEXTUREMODE, TRILINEARFILTER, FOG** (L417207–417213). `Option`: all 22 values + SKINNAME (REG_SZ, always 0x80 bytes with garbage after the NUL) + NUMEXNAME / EXNAME%d (REG_SZ 0x40) + NUMEXAID / EXAID%d (L417272–417349). NUMEXNAME and NUMEXAID are capped at 20. **EXNAME/EXAID are never read back** |
| **Every time the game mode ends**: each map change (packet 0x0091 → FUN_004f6c40(1,map) L317669), logout, char-select return, quit | CGameMode::OnExit FUN_00559b60 (vtable 0x69f394; L286287) | `ShortcutItem\<srv>\SITEM…`/`S_SKILL_USE_LEVEL…` (FUN_004d07b0, writes L176802–176807). `SkillUseLevelInfo\…` (FUN_004d1b00, writes L177285–177301). `NameBaloon\<srv>\POS` (L286468). It also copies the current camera latitude/distance into the in-memory indoor or outdoor globals; those are written to the registry at exit |
| Login window: Connect, toggling the Save check box, Exit button | FUN_00457100 (L76734; callers L76885/76897/76900) | ID (25 B), CHECKSAVE, BITWISE=1 |
| Server-list OK (clientinfo connection, then char-server list) | FUN_004563c0 msg 6/0x76 (L76109) | NUMACCOUNT / NUMSERVER |
| Clicking a character slot | FUN_0045ae20 (L79807) | CURSLOT |
| Toggling the check box in the Tip-of-the-day window | FUN_0047fa80 (L109665) | SHOWTIPSATSTARTUP |
| Alt+M macro window gets message 0 (Enter / default action) [?] | FUN_004534c0 → FUN_00453a10 (L73421) | `ShortCutList\` "0".."9" (REG_SZ). The in-memory list is updated too |
| Book window "bookmark" button | FUN_0048d220 (L120907) | **Deletes** `BookMark\<srv>\<char>` and recreates it with one DWORD |
| After the intro movie played | FUN_0050b740 (L224975) | BINKMODE |
| Dead code (no callers) | FUN_00455d90 (**the RegCreateKeyExA at L75793**) / FUN_00455d00, FUN_0045a5a0, FUN_0047fa10, FUN_00559eb0, FUN_00559f80, FUN_0050b6d0 | Generic DWORD saver / loader and duplicates. Never executed |
| Dead code: nProtect GameGuard | FUN_0065a320 / 0065ba30 / 0065c4c0 / 0065c6b0 | HKCU `SOFTWARE\INCAInternet\nProtectGameGuard\…`, and **deletes values in AppCompatFlags\Layers**. The init FUN_0050e160 has no callers and FUN_0050d980 returns 0, so this is inert |

Consequences for the settings tool:
* Everything in `Option`, `UIRectInfo` and SPRITEMODE / TEXTUREMODE / TRILINEARFILTER / FOG is loaded once and written back
  from memory at exit. Edits made while the client runs are overwritten.
* ShortcutItem, SkillUseLevelInfo and NameBaloon are overwritten on every map change.
* With several clients open, the last one to exit wins.
* So: refuse to write while a client is running.

## 4. `Option` subkey — load FUN_0061cac0 @0x61cac0 (L416633), save FUN_0061d890

If the whole `Option` key is missing (L416988–416998), the client uses FOG=1, isEffectOn=1, bgmIsPaused=0, isSoundOn=1,
volumes 100 and both AI flags 0. All other fields keep the CSession constructor values (L418993–419028).
All values are DWORD unless noted.

| Value | Default if missing | Normalised? | Meaning / command (msgstringtable text) |
|---|---|---|---|
| isEffectOn | 1 | 0/1 | `/effect` (case 8, L298299): "Effects On/Off" |
| bgmIsPaused | 0 | 0/1 | `/bgm` (case 7, L298283): "BGM On/Off". The Option window "bgm" box shows the inverse |
| isSoundOn | 1 | 0/1 | `/sound` (case 0xb, L298471): "Sound Effects On/Off". Option window "sound" box |
| streamVolume | 100 | **raw kept** (L416791). Playback clamps to 0..127 (FUN_004218d0) | **BGM** (mp3 stream) volume: `/bv <0-127>` (case 0xd, L298497) and the Option window BGM slider (step 10, 0..127, L99462–99487). Saved from the raw global, so an out-of-range value persists |
| soundVolume | 100 | clamped 0..127 (FUN_00421920/60) | **Sound-effect** volume (2D and 3D samples): `/v <0-127>` (case 0xc, L298480) and the second slider. The clamped value is saved (L417281) |
| m_monsterSnapOn_Skill | 1 | 0/1 | `/skillsnap` (0x2d, L299348): "Skill Snap" (cursor snaps to monsters while aiming skills). Option window "skill" |
| m_monsterSnapOn_NoSkill | 0 | 0/1 (copied to session+0xf10) | `/snap` (0x2e, L299358): "Attack Snap". Option window "attack" |
| m_isItemSnap | 0 | 0/1 | `/itemsnap` (0x2f, L299368): "Item Snap". Option window "item" |
| m_isOpaque | 0 | 0/1 | Option-window check box "opaque" (L99230–99235, L99414). **No other reader found**; it appears to have no effect in this build [?] |
| m_isShowWhisperWnd | 1 | 0/1 | Friend/whisper options window: "Open 1:1 Chat between Strangers" (msgstr 361). Whisper handler FUN_004dbe00 L184100 |
| m_isShowWhisperWnd_Friend | 1 | **not normalised** | "Open 1:1 Chat between Friends" (msgstr 359). **Load bug:** if non-zero it forces m_isShowWhisperWnd = 1 (asm 0x61ce72–0x61ce77). "Strangers off + friends on" cannot persist |
| m_isPlayWhisperOpenSound | 1 | 0/1 | "Alarm when you receive a 1:1 Chat" (msgstr 362). If `BGM\Msg_NewMsg.wav` (path `..\..\BGM\Msg_NewMsg.wav`) exists it plays regardless of this flag. Otherwise `버튼소리.wav` plays only when the flag is 1 (L184134–184142) |
| m_bMakeMissEffect | 1 | 0/1 | `/miss` (0x35, L299423): show the "Miss" effect |
| g_isFixedCamera | 0 | 0/1 | `/camera` (0x36, L299434). 1 = the camera focus snaps to the character every frame; 0 = smooth (lagging) follow (FUN_004faca0 L211220–211236). iRO text "Camera Zooming On/Off" |
| g_outdoorViewLatitude | -45.0 | load: < -89 → **-65**, > -1 → **-25** (asm 0x61d057–0x61d096) | Outdoor camera tilt (float bits). On map entry it is clamped again to **[-65, -25]** (FUN_0055a0b0 L287044–287049, asm 0x55a376–0x55a3ac) |
| g_outdoorViewDistance | 300.0 | clamped to [230 (0x6d66b0), **2040** (0x6d66b4)] | Outdoor zoom. 2040 looks like a uaRO zoom patch; the stock value is lower [?] |
| g_indoorViewLatitude | -45.0 | clamped to [-55, -35] (L416891–416899) | Tilt on maps listed in indoorRswTable.txt |
| g_indoorViewDistance | 300.0 | clamped to [230, 300] | Indoor zoom |
| m_bAutoOpenDetailWindowIfLowMemory | 1 | not normalised | Auto-opens the "Arrange Detail" window when physical RAM < 163 000 000 bytes and SPRITEMODE = TEXTUREMODE = 2 (L179006–179025; RAM check L419028). The Detail-window check box stores `1 - checked` (L98774). Irrelevant on modern PCs |
| SKINNAME | REG_SZ ≤ 128 B, default `"<Basic Skin>"` | — | Loaded into skin-manager string 0x727dec (asm 0x61d143). FUN_0043b500 (L50575, FindFirstFileA L50638) scans `.\skin\*.*`: each **sub-folder of `<client>\skin\`** is a skin (UI image prefix `..\..\skin\<name>\`), and `.zip` files there are extracted to `_tmpskin_` [?]. A name that matches the folder is selected. `"<Basic Skin>"` or any non-matching name = no skin (built-in GRF interface). The shipped client has `skin\default\basic_interface\*.bmp`. Saved from FUN_0043c170 (L417297) |
| onHoUserAI | 0 | 0/1 | `/hoai` (0xa3, L300508): homunculus AI `./AI/USER_AI/AI.lua` (1) instead of `./AI/AI.lua` (0) (L351764) |
| onMerUserAI | 0 | 0/1 | `/merai` (0xad, L300569): mercenary `./AI/USER_AI/AI_M.lua` vs `./AI/AI_M.lua` |
| NUMEXNAME, EXNAME%d, NUMEXAID, EXAID%d | — | — | **Write-only** at exit (whisper-block lists). Never loaded |

Session-only toggles are **not** in the registry; they reset to constructor defaults each launch (L418993–419016):
`/noshift`=0, `/noctrl`=0, `/aura`=1, `/window`=1, `/skillfail`=1, `/loginout`=1, `/shopping`=1, `/q1`=`/q2`=0,
`/notalkmsg`, `/notalkmsg2`, `/notrade`, `/battlemode`, `/showname`, `/lightmap`. A settings tool cannot preset them.
The slash-command id table is at L428990–429900.

## 5. `UIRectInfo` (load FUN_004cf2b0 L175893, save FUN_004d09b0 L176816; struct = g_windowMgr 0x727ed8 + offset)

* **Names use dots**, e.g. `ITEMWNDINFO.X` (Ghidra shows `_`). The full list of names with offsets (154 found in the loader) is in the appendix below. The groups are:
  - M_CHATWNDX / Y / HEIGHT / SHOW / STATUS, m_minimapZoomWnd, M_MINIMAPARGB, M_ISDRAWCOMPASS, M_ISDRAGALL.
  - `<WND>.X/.Y/.W/.H` for: SAYDIALOG, CHOOSE, BASICINFO, ITEM(+ORGHEIGHT, CURTAB, CURRADIO1-3, SHOW),
    STATUS(+ORGHEIGHT, SHOW), EQUIP(+ORGHEIGHT, SHOW), OPTION(+ORGHEIGHT, SHOW), SHORTENITEM(+SHOW), SHORTCUT(+SHOW), CHATROOM,
    ITEMSTORE, ITEMSHOP, ITEMSELL, ITEMPURCHASE, MESSENGERGROUP(+SHOW, RADIO), EXCHANGEACCEPT, SKILLLIST(+SHOW),
    MERCHANTITEM(+SHOW), MERCHANTMIRRORITEM, MERCHANTSHOPMAKE, MERCHANTITEMSHOP, MERCHANTITEMMYSHOP, MERCHANTITEMPURCHASE,
    ITEMCOLLECTION, COMBINEDCARDITEMCOLLECTION, ITEMPARAMCHANGEDISPLAY, PARTYSETTING, DETAILLEVEL, WHISPER, FRIENDOPTION,
    ITEMCOMPOSITION, ITEMIDENTIFY (all `…WNDINFO`).
* **No clamping at load.** Only these explicit defaults apply when a value is missing (L176060–176370):
  - M_ISDRAGALL → 1 (normalised 0/1).
  - M_ISDRAWCOMPASS → 0.
  - M_MINIMAPARGB → 0xAAFFFFFF; any value other than 0xFFFFFFFF becomes 0xAAFFFFFF.
  - The *.SHOW values of STATUS / EQUIP / OPTION / SHORTENITEM / SHORTCUT / MESSENGERGROUP / SKILLLIST / MERCHANTITEM → 0.
  - MESSENGERGROUPWNDINFO.RADIO → 1.
  - Everything else keeps the constructor value (FUN_004cc2f0 L173180–173186): chat X=0, Y=0, HEIGHT=98, SHOW=1,
    STATUS=1, minimap zoom 4.0f, ARGB 0xAAFFFFFF, compass 1. Window rects not set there are 0 (BSS).
* If the **whole key is missing**, FUN_004c8a10 (L169612) applies one default layout (640×480-oriented, all widths 280):

  | Window | X, Y, W, H |
  |---|---|
  | BASICINFO | 0,0,280,120 |
  | ITEM | 0,120,280,120 (tab 0) |
  | STATUS / OPTION | 185,300,280,120 |
  | EQUIP | 185,300,280,147 |
  | SHORTENITEM | 185,300,140,30 |
  | SHORTCUT | 185,300,280,34 |
  | SAYDIALOG | 200,100,280,180 |
  | CHOOSE | 200,300,280,120 |
  | CHATROOM | 500,180,280,120 |
  | ITEMSTORE | 360,200,280,432 |
  | ITEMSHOP | 0,120,280,312 |
  | ITEMSELL / ITEMPURCHASE | 280,280,280,152 |
  | MESSENGERGROUP | 300,200,140,155 |
  | EXCHANGEACCEPT | 200,200,280,120 |
  | SKILLLIST | 360,200,280,240 |
  | MERCHANTITEM | 200,120,280,120 |

* **Validation when a window is created** (message 0x22 hands the window its record):
  - Each window class has a pair `IsValid` / `SetDefault` at 0x4c7980–0x4c89f0; about 33 pairs, each called from one window's
    handler (e.g. FUN_00485650 L114391, FUN_00470f90 L97348, FUN_004730a0 L98842).
  - `IsValid` requires the expected size (exact, or ≥ 280×120 for resizable windows) and `0 ≤ X ≤ screenW−W`, `0 ≤ Y ≤ screenH−H`.
    screenW/H are the current renderer size (DAT_006ee3d8+0x24/+0x28). Some windows also check a tab in 0..6 or radio 0/1.
  - **If it fails, the window is reset to its hard-coded default (not clamped).**
  - Chat window: if off-screen → X=0, Y=screenH−h, i.e. bottom-left (L83328–83336).
  - So windows lost after a resolution decrease come back at default positions the next time they open. A tool can force a
    reset of one window by setting its `.X` to 0xFFFFFFFF (-1), or of all windows by deleting the key.
* Meaning of the extra values:
  - M_ISDRAGALL: storage/cart "Toggle Item Amount" box (msgstr 0x127). 1 = move whole stacks without the amount prompt (L150511).
  - M_MINIMAPARGB: minimap opacity, 0xFFFFFFFF opaque ↔ 0xAAFFFFFF translucent; toggled by the minimap button (L93964).
  - m_minimapZoomWnd: float in [1.0, 4.0] via the zoom buttons (L115974–115982). **Not validated at load**, and 0 gives a
    divide-by-zero at L288814, so write 1.0–4.0 only.
  - M_CHATWNDSTATUS: chat size step, 0..15 (14 px per step).
  - M_CHATWNDHEIGHT: stored chat height in px.
  - M_CHATWNDSHOW and M_ISDRAWCOMPASS: no reader found (inert).
  - `*.SHOW`: the window was open at exit and is reopened.

## 6. Other subkeys worth exposing

* **`ShortCutList\`** (stored with a trailing backslash): REG_SZ values `"0"`…`"9"` = **Alt+1 … Alt+9, Alt+0** chat macros
  (key handler L182625–182810 → FUN_004dc390).
  - Loaded at start by FUN_006480f0 (L448333) with cb = 0x32, so at most **49 chars + NUL**.
  - An **empty value keeps the built-in default**: msgstringtable 544–554 = `/! /? /ho /lv /swt /ic /an /ag /$ /...`.
  - Text starting with `/` runs as a command or emote; anything else is sent as chat. A good thing to expose.
* **`ShortcutItem\<char-server name>\`**: names `SITEM%d%d%d` / `S_SKILL_USE_LEVEL%d%d%d` =
  (hotbar slot 0–26, **servicetype** (0), **character-select slot** DAT_00771944 = CURSLOT-style index). It is **not**
  the character id (L346402).
  - SITEM: REG_SZ 32 B — item name, `#…` for a skill, or "nothing".
  - Server name: from the server list; if it contains "사크라이" (Sakray) that literal is used.
  - Loaded at character select (L346403); written on each game-mode exit.
* **`SkillUseLevelInfo`** (no server subkey): value `<SKILL_NAME><charslot>` (format `%s%d`) = DWORD level selected in the
  skill window. 0 or missing = default (L177145–177196).
* **`NameBaloon\<server>\POS`**: DWORD per server. Values other than 0/1 → 0 on load (L286967). It picks a placement
  variant for the actor name label (FUN_00567690: cases 0–3; 0/1 are horizontally centred with different vertical offsets).
  It is toggled by internal UI messages (0x5c toggles 0↔1, 0x59 cycles). Which of 0/1 is above or below is not determined [?].
  Low value for a tool.
* **`BookMark\<server>\<char name>`**: one DWORD named after the book = last page (default 1). Low value.
* Main-key items a tool could reasonably expose:
  - CHECKSAVE / ID (remember login).
  - SHOWTIPSATSTARTUP.
  - BINKMODE (only matters if openning.bik exists).
  - Deleting a legacy PASSWORD value.

## 7. Practical guidance for the settings utility

1. **Minimum the client needs to start:** the key exists; WIDTH, HEIGHT ≠ 0; a valid GUIDDEVICE (IID_IDirect3DHALDevice
   bytes); BITPERPIXEL 16 or 32 for fullscreen; SOUNDMODE (0/1/2). Everything else has a safe default.
2. **Safe ranges:**
   - SPRITEMODE / TEXTUREMODE 0..2 (2 = best); TRILINEARFILTER, ISLIGHTMAP, ISVOODOO(=0), FOG 0/1.
   - DIGITALRATETYPE 0..2 (22050 / 11025 / 8000 Hz); DIGITALBITSTYPE 0/1 (16 / 8 bit); NUMSAMPLETYPE 0..2 (48 / 32 / 16 voices).
   - Volumes 0..127; camera floats within the clamps above; minimap zoom 1.0–4.0.
3. **Never write while uaRO.exe runs.** Values the client never writes (display, sound, Setup values, ISLIGHTMAP) could be
   written while it runs, but they only apply at the next start.
4. **Write to the same view the game reads:** the VirtualStore copy for a non-elevated client, using the 32-bit registry
   view. Values the game already wrote there shadow HKLM.
5. **Windowed mode:** choose WIDTH/HEIGHT smaller than the desktop work area. The window is centred, not clamped, and has
   no resize border. Fullscreen only accepts real display modes (or whatever dgVoodoo emulates).

Unverified items [?]:
* Semantics that only Setup.exe or mss32.dll could confirm: DEVICECNT/MODECNT as indices, SPEAKERTYPE enum names, and the
  mono/stereo meaning of the driver's channel argument. Neither file is in `/root/BornRok/winEXE`, and they were not available for
  this research.

## Appendix: UIRectInfo value names and their offsets in g_windowMgr (0x727ed8)

| Value | Offset |
|---|---|
| `m_minimapZoomWnd` | 0x20 |
| `M_ISDRAGALL` | 0x2c |
| `M_ISDRAWCOMPASS` | 0x28 |
| `M_MINIMAPARGB` | 0x24 |
| `M_CHATWNDSTATUS` | 0x1c |
| `M_CHATWNDHEIGHT` | 0xc |
| `M_CHATWNDX` | 0x4 |
| `M_CHATWNDY` | 0x8 |
| `M_CHATWNDSHOW` | 0x10 |
| `SAYDIALOGWNDINFO.X` | 0x350 |
| `SAYDIALOGWNDINFO.Y` | 0x354 |
| `SAYDIALOGWNDINFO.W` | 0x358 |
| `SAYDIALOGWNDINFO.H` | 0x35c |
| `CHOOSEWNDINFO.X` | 0x360 |
| `CHOOSEWNDINFO.Y` | 0x364 |
| `CHOOSEWNDINFO.W` | 0x368 |
| `CHOOSEWNDINFO.H` | 0x36c |
| `BASICINFOWNDINFO.X` | 0x2ac |
| `BASICINFOWNDINFO.Y` | 0x2b0 |
| `BASICINFOWNDINFO.W` | 0x2b4 |
| `BASICINFOWNDINFO.H` | 0x2b8 |
| `ITEMWNDINFO.X` | 0x2bc |
| `ITEMWNDINFO.Y` | 0x2c0 |
| `ITEMWNDINFO.W` | 0x2c4 |
| `ITEMWNDINFO.H` | 0x2c8 |
| `ITEMWNDINFO.ORGHEIGHT` | 0x2cc |
| `ITEMWNDINFO.CURTAB` | 0x2d0 |
| `ITEMWNDINFO.CURRADIO1` | 0x2d4 |
| `ITEMWNDINFO.CURRADIO2` | 0x2d8 |
| `ITEMWNDINFO.CURRADIO3` | 0x2dc |
| `ITEMWNDINFO.SHOW` | 0x2e0 |
| `STATUSWNDINFO.X` | 0x2e4 |
| `STATUSWNDINFO.Y` | 0x2e8 |
| `STATUSWNDINFO.W` | 0x2ec |
| `STATUSWNDINFO.H` | 0x2f0 |
| `STATUSWNDINFO.ORGHEIGHT` | 0x2f4 |
| `STATUSWNDINFO.SHOW` | 0x2f8 |
| `EQUIPWNDINFO.X` | 0x2fc |
| `EQUIPWNDINFO.Y` | 0x300 |
| `EQUIPWNDINFO.W` | 0x304 |
| `EQUIPWNDINFO.H` | 0x308 |
| `EQUIPWNDINFO.ORGHEIGHT` | 0x30c |
| `EQUIPWNDINFO.SHOW` | 0x310 |
| `OPTIONWNDINFO.X` | 0x314 |
| `OPTIONWNDINFO.Y` | 0x318 |
| `OPTIONWNDINFO.W` | 0x31c |
| `OPTIONWNDINFO.H` | 0x320 |
| `OPTIONWNDINFO.ORGHEIGHT` | 0x324 |
| `OPTIONWNDINFO.SHOW` | 0x328 |
| `SHORTENITEMWNDINFO.X` | 0x32c |
| `SHORTENITEMWNDINFO.Y` | 0x330 |
| `SHORTENITEMWNDINFO.W` | 0x334 |
| `SHORTENITEMWNDINFO.H` | 0x338 |
| `SHORTENITEMWNDINFO.SHOW` | 0x33c |
| `SHORTCUTWNDINFO.X` | 0x3d8 |
| `SHORTCUTWNDINFO.Y` | 0x3dc |
| `SHORTCUTWNDINFO.W` | 0x3e0 |
| `SHORTCUTWNDINFO.H` | 0x3e4 |
| `SHORTCUTWNDINFO.SHOW` | 0x3e8 |
| `CHATROOMWNDINFO.X` | 0x370 |
| `CHATROOMWNDINFO.Y` | 0x374 |
| `CHATROOMWNDINFO.W` | 0x378 |
| `CHATROOMWNDINFO.H` | 0x37c |
| `ITEMSTOREWNDINFO.X` | 0x380 |
| `ITEMSTOREWNDINFO.Y` | 0x384 |
| `ITEMSTOREWNDINFO.W` | 0x388 |
| `ITEMSTOREWNDINFO.H` | 0x38c |
| `ITEMSHOPWNDINFO.X` | 0x390 |
| `ITEMSHOPWNDINFO.Y` | 0x394 |
| `ITEMSHOPWNDINFO.W` | 0x398 |
| `ITEMSHOPWNDINFO.H` | 0x39c |
| `ITEMSELLWNDINFO.X` | 0x3a0 |
| `ITEMSELLWNDINFO.Y` | 0x3a4 |
| `ITEMSELLWNDINFO.W` | 0x3a8 |
| `ITEMSELLWNDINFO.H` | 0x3ac |
| `ITEMPURCHASEWNDINFO.X` | 0x3b0 |
| `ITEMPURCHASEWNDINFO.Y` | 0x3b4 |
| `ITEMPURCHASEWNDINFO.W` | 0x3b8 |
| `ITEMPURCHASEWNDINFO.H` | 0x3bc |
| `MESSENGERGROUPWNDINFO.X` | 0x3c0 |
| `MESSENGERGROUPWNDINFO.Y` | 0x3c4 |
| `MESSENGERGROUPWNDINFO.W` | 0x3c8 |
| `MESSENGERGROUPWNDINFO.H` | 0x3cc |
| `MESSENGERGROUPWNDINFO.SHOW` | 0x3d0 |
| `MESSENGERGROUPWNDINFO.RADIO` | 0x3d4 |
| `EXCHANGEACCEPTWNDINFO.X` | 0x3ec |
| `EXCHANGEACCEPTWNDINFO.Y` | 0x3f0 |
| `EXCHANGEACCEPTWNDINFO.W` | 0x3f4 |
| `EXCHANGEACCEPTWNDINFO.H` | 0x3f8 |
| `SKILLLISTWNDINFO.X` | 0x3fc |
| `SKILLLISTWNDINFO.Y` | 0x400 |
| `SKILLLISTWNDINFO.W` | 0x404 |
| `SKILLLISTWNDINFO.H` | 0x408 |
| `SKILLLISTWNDINFO.SHOW` | 0x40c |
| `MERCHANTITEMWNDINFO.X` | 0x410 |
| `MERCHANTITEMWNDINFO.Y` | 0x414 |
| `MERCHANTITEMWNDINFO.W` | 0x418 |
| `MERCHANTITEMWNDINFO.H` | 0x41c |
| `MERCHANTITEMWNDINFO.SHOW` | 0x420 |
| `MERCHANTMIRRORITEMWNDINFO.X` | 0x424 |
| `MERCHANTMIRRORITEMWNDINFO.Y` | 0x428 |
| `MERCHANTMIRRORITEMWNDINFO.W` | 0x42c |
| `MERCHANTMIRRORITEMWNDINFO.H` | 0x430 |
| `MERCHANTSHOPMAKEWNDINFO.X` | 0x434 |
| `MERCHANTSHOPMAKEWNDINFO.Y` | 0x438 |
| `MERCHANTSHOPMAKEWNDINFO.W` | 0x43c |
| `MERCHANTSHOPMAKEWNDINFO.H` | 0x440 |
| `MERCHANTITEMSHOPWNDINFO.X` | 0x444 |
| `MERCHANTITEMSHOPWNDINFO.Y` | 0x448 |
| `MERCHANTITEMSHOPWNDINFO.W` | 0x44c |
| `MERCHANTITEMSHOPWNDINFO.H` | 0x450 |
| `MERCHANTITEMMYSHOPWNDINFO.X` | 0x454 |
| `MERCHANTITEMMYSHOPWNDINFO.Y` | 0x458 |
| `MERCHANTITEMMYSHOPWNDINFO.W` | 0x45c |
| `MERCHANTITEMMYSHOPWNDINFO.H` | 0x460 |
| `MERCHANTITEMPURCHASEWNDINFO.X` | 0x464 |
| `MERCHANTITEMPURCHASEWNDINFO.Y` | 0x468 |
| `MERCHANTITEMPURCHASEWNDINFO.W` | 0x46c |
| `MERCHANTITEMPURCHASEWNDINFO.H` | 0x470 |
| `ITEMCOLLECTIONWNDINFO.X` | 0x474 |
| `ITEMCOLLECTIONWNDINFO.Y` | 0x478 |
| `ITEMCOLLECTIONWNDINFO.W` | 0x47c |
| `ITEMCOLLECTIONWNDINFO.H` | 0x480 |
| `COMBINEDCARDITEMCOLLECTIONWNDINFO.X` | 0x484 |
| `COMBINEDCARDITEMCOLLECTIONWNDINFO.Y` | 0x488 |
| `COMBINEDCARDITEMCOLLECTIONWNDINFO.W` | 0x48c |
| `COMBINEDCARDITEMCOLLECTIONWNDINFO.H` | 0x490 |
| `ITEMPARAMCHANGEDISPLAYWNDINFO.X` | 0x494 |
| `ITEMPARAMCHANGEDISPLAYWNDINFO.Y` | 0x498 |
| `ITEMPARAMCHANGEDISPLAYWNDINFO.W` | 0x49c |
| `ITEMPARAMCHANGEDISPLAYWNDINFO.H` | 0x4a0 |
| `PARTYSETTINGWNDINFO.X` | 0x4a4 |
| `PARTYSETTINGWNDINFO.Y` | 0x4a8 |
| `PARTYSETTINGWNDINFO.W` | 0x4ac |
| `PARTYSETTINGWNDINFO.H` | 0x4b0 |
| `DETAILLEVELWNDINFO.X` | 0x4b4 |
| `DETAILLEVELWNDINFO.Y` | 0x4b8 |
| `DETAILLEVELWNDINFO.W` | 0x4bc |
| `DETAILLEVELWNDINFO.H` | 0x4c0 |
| `WHISPERWNDINFO.X` | 0x4d8 |
| `WHISPERWNDINFO.Y` | 0x4dc |
| `WHISPERWNDINFO.W` | 0x4e0 |
| `WHISPERWNDINFO.H` | 0x4e4 |
| `FRIENDOPTIONWNDINFO.X` | 0x4fc |
| `FRIENDOPTIONWNDINFO.Y` | 0x500 |
| `FRIENDOPTIONWNDINFO.W` | 0x504 |
| `FRIENDOPTIONWNDINFO.H` | 0x508 |
| `ITEMCOMPOSITIONWNDINFO.X` | 0x50c |
| `ITEMCOMPOSITIONWNDINFO.Y` | 0x510 |
| `ITEMCOMPOSITIONWNDINFO.W` | 0x514 |
| `ITEMCOMPOSITIONWNDINFO.H` | 0x518 |
| `ITEMIDENTIFYWNDINFO.X` | 0x51c |
| `ITEMIDENTIFYWNDINFO.Y` | 0x520 |
| `ITEMIDENTIFYWNDINFO.W` | 0x524 |
| `ITEMIDENTIFYWNDINFO.H` | 0x528 |
