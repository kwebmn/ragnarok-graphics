# Autopilot: a fresh install that works for everyone at once — design

2026-10-30. The owner: «нужно продумать вариант при котором мы ставим програму и она максимально безпроблемно работает
сразу у всех ... так чтобы он запустил и все автоматически настроилось»; my proposal in chat, the owner: «делай».

## Why

The site's log since the uaRO HD client came out (2026-10-23..30): 11 players' computers, 6 of them gone after one
session of 1-22 minutes, none of those sent a report; half the cards weak (Radeon HD 6570, FirePro W5130M, Radeon 780M,
GTX 1050 Ti); every player at 1080p or below with the 60-frame limit and VSync; nobody changes settings. One player
(Argentina) opened the launcher 17 times in four hours, no session reaching ten minutes. What the code does now:

- The settings app's first start (`settings::autoSetup`, by the card's memory) writes an explicit effects set
  (`weak`/`medium`/`strong`) - so the driver's card judge (`card_speed::Judge`), which moves the `auto` set only,
  never runs for those players. The judge itself only tries weak -> medium.
- `keep_fps` is 45 below the strong class and 0 for it: a GTX 1070 or RX 580 with 8 GB gets the strong set and no
  safety net; with VSync at 60, 45-59 frames judder.
- The safe mode counts only starts that ended before their first picture; a crash at the login or a map's loading
  repeats at every start.
- An installed client copied with another computer's `uaro-graphics.ini` (`[utility] machine` of the packer's) gets a
  question (`kAskMachine`) instead of its setup.
- The statistics and the automatic report are asked in two message boxes before the first game.
- We cannot see how a new player's first start went.

## Goals

1. **The set by the card's measured speed, both ways.** The automatic set moves over the whole ladder - «Без
   ефектів» (0), «Низькі» (1), «Середні» (2), «Високі» (3) - to the highest one the card holds at the 60-frame target
   with room to spare, judged on the maps; the verdict kept per card (judged again on a new card or screen).
2. **keep_fps 60 for every class** (the super-light mode's own 0 stays): the in-game net for heavy scenes.
3. **The safe mode also after a bad end in the first 5 minutes after the first picture**; a new version tries the
   normal mode again.
4. **A copied install set up silently**: another computer's `machine` and no driver log in this folder.
5. **No questions before the first game**: the statistics' and the report's questions wait for a game played here.
6. **The first start's outcome with the anonymous install ping**; the site keeps it, the admin summary shows it.

## Non-goals

No benchmark screen at the first start; DLSS, MSAA, the render scale and the AI textures stay as the class gives
them; a set the player picked by hand and the custom set stay theirs; no new graphics features.

## Anti-goals

The set flapping between two levels; a town slow for the processor's part lowering the set (only the card holding the
game back counts - `effect_costs::slowForCard`); a player's own choice overridden silently; anything personal sent.

## Decided

- The owner: the target is a smooth 60 frames a second (fewer effects on weak machines), the proposal as a whole.
- Mine (said to the owner, flip on a word): the first start's outcome goes with the unconditional anonymous ping, the
  app's ping tip saying so (Goal 6) - the players who leave after one session never answer the statistics' question.

## How (the builder's choices are marked "builder")

### 1. The card judge's ladder (`core/card_speed`)

- Levels 0..3 as `effect_groups::setLevel`. The start: the record for this adapter and size, else the memory level
  (`effect_groups::cardLevel`, 1..3).
- 5-second windows on a map with the game in front (as now). Up: the card's own work at most half a frame at the target
  (`hasRoom`) for 12 windows in a row, the level under 3 and not settled -> one level up on trial. Trial: 36 windows (3 minutes); 2 windows in a row slow for the card -> back down, that level failed
  (settled); 36 windows -> kept (not settled while under 3: the next game may try higher). Down while not on trial: 3
  windows in a row slow for the card at a level above 0 -> one level down, settled.
- The record (`uaro-graphics\card-level.txt`): `v2 <level> <settled 0|1> <adapter>`; the old `<level> <adapter>`
  (levels 1-2) still read - as the start level, not settled. No screen size in it: the render target is known only
  after the first picture, while the set is needed before; a new card or screen is the settings app's changed machine
  (`[utility] machine` holds the adapter and the screen), and then the app deletes the record - judged again. The
  driver writes it at Keep, Back and Lower; the settings app reads it for «Авто: {1}».
- The driver (`Context::judgeCard`): every class with the `auto` set (not only level 1); a level changed in a window
  resets keep_fps's slow count that window (no double reaction). On-screen notes for up on trial, kept, back down,
  lowered - in the three languages.

### 2. The automatic setup and the migration (`core/settings/auto_setup`, recommended)

- `autoSetup`: `effects.preset = auto` for Old, Low, Middle, High and IntegratedStrong; IntegratedWeak keeps `off` with
  the super-light mode, NoDriver `off`. `frames.keep_fps = 60` for every class but IntegratedWeak (0).
- The migration, once (`[utility] autopilot = 1`), in the driver at the game's start and the app when it opens (as
  `worldScaleReset`): an explicit set equal to the memory level's (1 weak, 2 medium, 3 strong) becomes `auto`;
  `keep_fps` 0 or 45 becomes 60. Custom and `off` stay.
- The app: «Авто: {1}» names level 0 «Без ефектів» (FxPresetOff's words); «Гра» shows, with the `auto` set, «Ефекти під
  цю відеокарту: «{set}» · ціль 60 кадрів» and a button to the Effects page (builder: the host's page switch).

### 3. The safe mode (`core/safe_mode`)

- `judge(previousLog)`: Fine as now unless a bad end (`report::reasonOfLog`) came within 5 minutes after the first
  picture's line - then FailedEarly; both kinds count in `failedInRow`. Times from the log's `HH:MM:SS` stamps (past
  midnight: a day added); a "not closed" end is the log's last line.
- The note (`kSafeModeNote`) carries the version that wrote it (its first line `version <V>`); a start of another
  version deletes it and the counter
  (the normal mode tried again). The app's SafeModeAuto words: «останні запуски не дійшли до картинки або закрилися з
  помилкою в перші хвилини».

### 4. The app's first start (`app/model`, `main_window`)

- `firstStart` gets "a driver log here": another computer's `machine` and no log -> Apply (silently), with a log ->
  Ask (as now).
- `kAskStats`: asked only once a game was played here (the driver's log exists); else nothing this start.

### 5. The first start's outcome (driver, app, site)

- The driver writes `uaro-graphics\cache\first-start.txt` once per install (not there yet): version, class (the memory
  level, the API, the safe mode), and after the first 12 windows on maps (a minute): reached a map 1, those windows'
  median frames, the set level then; at
  the next start: how the first game ended (`reasonOfLog` of the previous log, "" fine) and its minutes.
- The ping (`core/install_ping`, `app/update_run`) adds `first` (that file's fields) until the site took it
  (`first-start.sent`).
- The site (`GraphicsPingController`, migration): columns on `graphics_installs` - first_map, first_fps, first_level,
  first_end, first_minutes, first_class; the admin summary's installs block: how many reached a map, ended badly, by
  class. Staged in the sandbox with a deploy script the owner runs.

## Phases (betas)

1. 3.0.11-beta.58 - the ladder, the record, the driver, the automatic setup, the migration, the app's words.
2. 3.0.11-beta.59 - the safe mode.
3. 3.0.11-beta.60 - the app's first start.
4. 3.0.11-beta.61 - the first start's outcome (+ the site part staged).

## Left to the builder

The texts' exact words (uk first, quoted from strings.cpp's column), the log lines, the tests' cases, the «Гра» line's
place among the cards.
