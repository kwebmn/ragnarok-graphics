# «Довідник» on the New Interface Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** «Довідник» as a live window of the new interface (RmlUi) with every card, then the world map, the maps'
NPCs, the pad, the cards' new things, «Навички», the search's layout fix and chat links, the site's pack - a beta
per stage.

**Architecture:** the pages stay core `refpages` (rows of 25 styles, links). A new `uikit/reference_live` turns a
page into markup - the chrome (header, rail, footer) bound to a model, the body a generated RML string (blocks in
columns, every clickable element `ctl` with `data-ln="<i>"` into a links vector) - host-tested with RmlUi
(`tests/rml_host`). `win/reference_window` becomes a `uilive::Window`: one click listener maps `data-ln` to
`refpages::Link`, the search an RmlUi `<input>` whose special keys (↑ ↓ Enter Esc PgUp PgDn Alt+←) the window takes
first, the world map an `<img>` of the painted sheet. The old hand layout (`core/reference_window`) goes in stage 4,
its world painter kept.

**Tech Stack:** C++17, RmlUi 6.3 (third_party), Win32 ddraw.dll 32/64-bit, host tests (`build-host/uagfx_tests`),
the ship gate (`gship.sh`).

**Spec:** `docs/superpowers/specs/2026-11-13-reference-rewrite-design.md` (the owner's concept doc
6sd7hxvHC1yGpnnA9KCMgk, both tabs, all answers of 2026-11-13).

## Global Constraints

- Client only; site changes staged, deployed only on the owner's OK. Betas only, notes uk/ru/en per beta.
- Strings uk/ru/en in `core/settings/strings.cpp`; UI names quoted from its uk column; game terms in English.
- Markup: RmlUi loads it on the host with no warning in `rmlhost::log()`; the theme's rules for the window under
  the body class `rfw` only (no `lw` generic classes inside the body: `.lw .row/.chip/.list` would hit it).
- Gold only for what is pressed and what is chosen; numbers white.
- Lists whole; «ще N ›» only past 100 rows (`refpages::kFew = 100`, `kFewMore = 0`).
- New lines <= 120 columns; no warnings in 32/64-bit; host suite green before each ship.

## Review Focus

1. A page change while scrolled far down: the new page starts at its top; «Назад» brings the old scroll back.
2. Typing fast while icons load a few a frame: no lost letters, the caret stays in the field.
3. The window pulled small (narrow) and big again: the rail's names come back, columns rebuilt, nothing clipped.
4. Esc with text typed clears it; a second Esc closes; F1-F9 drink and cast while the caret is outside the field.
5. A press dragged on the world map is not a click; a click without a drag opens the map's card.

---

### Task 1: data - one row a monster, whole lists, «Стихії» (stage 1)

**Files:** Modify `src/core/reference_index.hpp/.cpp` (Drop, Source: `parts`; merge on read), `src/core/
reference_pages.hpp/.cpp` (kFew 100, kFewMore 0, search groups 100, the parts note), `src/core/settings/strings.cpp`
(RefSecTables «Стихії», its hint); tests `tests/test_reference_index.cpp`, `tests/test_reference_pages.cpp`.

**Interfaces:** `struct Drop { int item, rate, kind; std::vector<int> parts; }`, `struct Source { int mob, rate,
kind; std::vector<int> parts; }` - entries of one thing (and kind) of one monster merged at read: `rate` =
round(10000 (1 - prod(1 - p/10000))), `parts` the merged rates (empty when one); `std::string
refpages::chanceParts(const std::vector<int>& parts, settings::Lang)` - «8 × 1 %», «11 % + 8 %» ("" for < 2).

- [ ] Tests first: a mob with Apple 8 x 100 and 2 x 8000 -> one drop 773 / 9600 with parts; droppedBy one row a mob;
  chanceParts strings; a section of 60 rows shown whole, 130 -> 100 + «ще 30 ›»; search group of 23 whole.
- [ ] Watch them fail; implement; host suite green; commit.

### Task 2: uikit reference - the chrome and the body's markup (stage 1)

**Files:** Create `src/uikit/reference_live.hpp/.cpp`, `tests/test_uikit_reference.cpp`; CMakeLists (sources,
test). Theme rules `.rfw ...` in `src/core/ui_theme_css.cpp`.

**Interfaces:** namespace `uagfx::uikit::reference`: `struct RailV { Rml::String name, icon; int count = 0; bool on
= false; }`; `struct Model { Rml::String title, back, query, find, keys, pack, live, ground, star, note, body; int
liveState = 0; bool home = false, narrow = false; std::vector<RailV> rail; std::vector<Rml::String> crumbs; }`;
`const char* rml()`; `void bind(Rml::DataModelConstructor&, Model&)` (events are the window's: ref_back, ref_home,
ref_star, ref_close, ref_rail, ref_chip); `struct Look { bool narrow = false; std::function<std::string(const
std::string& key)> picture; }`; `std::string body(const refpages::Page&, const Look&, std::vector<refpages::Link>&
links)` - every row but the rail; each clickable `class="... ctl" data-ln="<i>"` with `links[i]`; text escaped;
`marks` as `<span class="mk">`; blocks: the hero (Title, Sub, Portrait) on top, then sections in two columns (the
next into the shorter by rows), a section of > 20 rows full width split in 2 (narrow 1) columns, narrow one column.

- [ ] Tests first (rmlhost): the chrome loads with no RmlUi warning; a monster-like page's body loads, has as many
  `data-ln` as links, a Result's mark is a `.mk` span, «<b>&» escaped; two sections side by side in a wide look, one
  column narrow; a 30-row section full width with two inner columns; the World row is an element `#wmap`.
- [ ] Implement; host suite green (scope test too); commit.

### Task 3: win - the live window (stage 1)

**Files:** Rewrite `src/win/reference_window.cpp` (keep its header's API), hook `referencewin::fieldKey` in
`src/win/window_switch.cpp` before `uaui::key`, `src/win/window_stack.cpp` (sizeKey «reference»).

- [ ] uilive::Window «reference», body class `rfw`; place: centred, 1270x850 dp at 1080p (shrunk to 92 % x 90 % of
  the screen), times `windowstack::sizeOf(Win::Reference)` on the box (not a zoom); narrow below 900 dp wide.
- [ ] Model from the state (title, «Назад до …», crumbs, star, rail, footer chips, keys); body markup on a page or
  a picture change (pictures a few a frame, the body made again at most every 150 ms); scroll: 0 on a new page,
  kept on «Назад», the picked row scrolled into view; the picked row's class `sel` set directly.
- [ ] Clicks: a listener on `#body` maps `data-ln` to `go()`; the chrome's events; the mouse's back button; the
  wheel scrolls (RmlUi), over `#wmap` it zooms the world map.
- [ ] Keys: the field focused on open; `fieldKey` takes ↑ ↓ Enter Esc PgUp PgDn Alt+← while the field has the caret;
  `key` (field not focused): the same, letters and Ctrl+V into the field, F-keys and Alt keys to the game.
- [ ] World map: `#wmap` gets the painted sheet (refwin::paintWorldPicture) at its box; drag, wheel zoom, hover tip,
  a click without a drag opens the map's card; the dungeon chain's maps as before.
- [ ] Builds 32/64 clean; log lines «reference: …» for open, page, pictures; commit.

### Task 4: ship stage 1 (beta)

- [ ] Notes uk/ru/en (what changed, what to check: drag, corner, narrow, typing, Esc, F-keys, every card, the world
  map); `gship.sh`; ledger line.

### Stage 2 - world map polish (beta)
The bar's tabs and tools, the route panel, the dungeon chain as markup over the picture (not painted), the tip as a
card; a location's click opens its map card. Tests: the World page's markup has the bar, the panel, the chain.

### Stage 3 - maps: one map with «⤢» (beta)
A shared map painter for any map (core minimap): the game's picture, x4 or «Схема»; NPC dots by kind (K $ ! letters;
colours for ours and guides; the rest grey), warps, exit labels, spawn zones, «Ви тут», the party; zoom limits.
The big map «⤢» and the location card use it; the card's list by kinds with the jump row; NPC card page (Go::Npc):
goods, teleports, ferries, «Вести сюди», picture, its minimap -> the map centred. «NPC і послуги» by the same kinds.
Kinds: the pack's (stage 8) else the name's guess. Tests: painter marks per kind, exits labelled only, NPC card rows.

### Stage 4 - pad and cleanup (beta)
Pad: rows' gold frame through uilive nav, A open, B back, LB/RB sections, Y the field with the on-screen keyboard,
X «Вести сюди», tips for the picked row. The old layout (`core/reference_window` but the world painter, moved to
`core/reference_world_paint`) and its tests removed. Tests: the pad's targets are the body's `ctl` boxes.

### Stage 5 - the cards' new things (beta)
Item description under the name (info_card's reader); own boss timers first (mvp panel's list); Shift+click a name
-> «[Name]» in the chat line; «У довіднику» in the item menu («Речі») and the target card; the player's things (bag,
cart, storage last seen), craft have/missing, quest «2 з 5»; monsters' levels coloured; NPC minimap -> map.

### Stage 6 - «Навички» (beta)
Skills by jobs from the site's data (the watcher fetches it like quests) - jobs list, a job's skills with levels,
SP, element, description. Tests: the section and a job page from a sample file.

### Stage 7 - search and chat (beta)
Cyrillic typed -> the same keys in Latin (login_hints' table) searched too, the line over the results, «шукати як
набрано»; «[Name]» in the chat (the new chat) a link to the card for players with uaRO Graphics.

### Stage 8 - the site (staged)
The walkable-cells file built with the pack; NPC kinds (script folder) in the pack; staged with a deploy script;
the owner told before deploy.
