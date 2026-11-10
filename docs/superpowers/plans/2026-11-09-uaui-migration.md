# uaUI: all of the new interface on RmlUi — plan and ledger

> Owner 2026-11-09: «давай наверное делай все окна в инлайн режиме. реализовывай полностью всю концепцию. без
> остановки.» Spec: Claude Doc «Концепция: единая библиотека интерфейса uaRO Graphics»
> (https://claude.ai/artifact/2r7LUxsfJWmFfpAA4dja8N), variant A (RmlUi + our components). Inline, a beta per step
> (`$S/ship-beta.sh 3.0.18-beta.N`), gate each time. Stages 1–2 done: beta.7 (spike), beta.9 (theme, template,
> showcase), beta.10 (Blocker::Uaui).

## Rules

- A part keeps its logic (reading the game, commands, pad, packets, layout memory); only its painting and hit-testing
  move to a uaUI document with a data model. The old painter stays until every part is over, then goes (stage 6).
- `[interface] uaui` (off by default, «Нова бібліотека інтерфейсу») draws the converted parts by uaUI; off = the old
  pictures. Flip the default only on the owner's word.
- The built-in theme repeats today's look (core/ui_style, core/window_panel colours, Plex, sizes in concept px = dp).
- What changes in a fight (HP, cast, cooldowns) changes at once; motion only for showing, hovering, tabs, dragging.
- Each uaUI document is a blocker (Blocker::Uaui rect = the top uaUI box under the cursor).
- Texts stay in core/settings/strings.cpp (uk/ru/en); documents get them through the model, not typed in RML.

## Owner 2026-11-10: the ruling below REVERSED - «надо вообщето Переписывать 73 части в разметку RmlUi вручную»

beta.16 turns the recording off (R-ZK9ZK9: the game gone right after «Довідник» opened recorded; likely the bridge
over-filled by the meshes - not proved, no dump). Every part is rewritten in RML + its data model by hand, one a beta,
its old picture kept behind [interface] uaui = 0. Stages 4-5 below are open again.

## (reversed) Ruling (2026-11-09, beta.12): the parts' painters recorded, not rewritten

58 painters draw through panel::roundRect / circle, uiicons::draw, a picture's put/blit, and drawWords (each word's
GDI coverage cached). With `Picture::scene` set those record primitives (core/ui_scene) instead of rasterizing; uaUI
draws a part's scene inside its RmlUi document by the custom element `<ua-scene>` - rects and circles as meshes, icons
and pictures as textures, words from their own GDI coverage in a words atlas (pixel for pixel today's). RmlUi keeps the
documents (fade, scale, drag, blocker, z, theme); the content's colours themed by a palette in the theme file.
Rewriting 73 parts in RML by hand: months; recording: the same look at once, the CPU's painting and the whole
picture's upload gone. Cost if wrong: per-pixel glows (panel::over loops) land in a residual layer under the shapes.

## Stage 3 — runtime for parts (betas 11–12)

- [x] T1 `win/uaui.hpp` part API: `uaui::Doc` (load RML from memory, its model, show/hide with the fade, place
      in px, box, top-most), data events → C++ (`BindEventCallback`), a doc's z among the driver's windows
      (window_stack order), blocker from the doc under the cursor, keys to a focused text field.
- [x] T2 `<img src="pic:…">` for the driver's pictures that change (portraits, minimap, sprites): a texture by the
      picture's address and generation, uploaded again when the generation changes.
- [x] T3 theme = today's look: window frame, title, ×, tabs, slots, bars, tips, buttons — measured from the old
      painters (core/window_panel, core/char_block, core/item_window).
- [x] T4 speed: reuse the frame's vertices when RmlUi rendered the same geometry; one draw per texture run.

## Stage 4 — HUD (by hand in RML, one beta each)

- [x] character block (win/char_block + core/char_block) — beta.17 by hand: core charblock::model, RML kDoc, <ua-ring>
- [x] target card (win/target_card) — beta.18 by hand
- [ ] party panel (beta.19 by hand), mercenary/homunculus blocks
- [x] buffs column and its further lines — beta.20 by hand (core layout boxes in px)
- [x] hotbar — beta.21 by hand (passive doc: its own input; core hotbar::view)
- [ ] experience bars, experience gain, item toast
- [ ] hunt panel, MVP panel, tracker
- [ ] minimap frame and its list (the map picture as `pic:`)

## Stage 5 — windows (by hand in RML)

- [ ] «Речі», storage, cart, trade
- [ ] «Спорядження», «Характеристики», «Навички»
- [ ] shop, vending, own shop
- [ ] NPC talk, ask / message / pick / count cards, item and skill cards
- [ ] chat, whispers, chat room, popup menu, emotions, commands
- [ ] login, servers, character select and make
- [ ] pet, homunculus, mercenary, options, friends, guild, whisper setup, macros, tip of the day, menu, OSK
- [ ] «Довідник»
- [ ] pad tiles / assign / hints, HUD edit

## Stage 6 — themes and cleanup

- [x] a second and third theme (classic RO, contrast), the pick in the app and Alt+D (beta.13); a theme through
      `updates` not done (the palette file is the player's)
- [ ] the old painters removed; `[interface] uaui` default on (owner's word)

## Ledger

(one line per finished step: date, beta, what, commit)
- 2026-11-09 beta.11: T1 runtime (begin/end, Doc, pic:, blocker), showcase part
- 2026-11-09 beta.12: ruling - parts recorded (core/ui_scene) and drawn by uaui::drawScene; all blank()+drawPart parts on the card with [interface] uaui
- 2026-11-09 beta.13: themes (core/ui_theme: standard/classic/contrast, [interface] theme, ui\palette.txt), windows grow+fade in 120 ms, log of pictures not recorded
- 2026-11-09 beta.14: manual pictures -> blank (answer/info cards, HUD layout, «Довідник» (full repaint when recorded), char block warning/flare, buff overlay, exp strip/gain/tip, exp gain, pad carry, enter veil); nested recorded pictures move their prims; minimap still painted (map blits)
- 2026-11-09 beta.15: minimap + big map recorded (map pixels residual under marks; big map's picture an image over its card)
- 2026-11-10 beta.16: recording off (owner: rewrite by hand; R-ZK9ZK9 crash)
- 2026-11-10 beta.17: character block by hand in RML (full, compact bars, rings, icon; tips; warning; flare; buttons); <ua-ring> element; theme file adds to the built-in theme
- 2026-11-10 beta.18: target card by hand in RML (round picture, name, level, element chip in its colour, HP bar, book, pad hints, details on :hover, click opens «Довідник»)
- 2026-11-10 beta.19: party panel by hand in RML (rows, faces, crown, states, HP bar, cast line, way arrow rotated, buffs with times, compact cells, fold/view); click actions shared (toggleFold/toggleView/rowClicked); uaui::stamp()
- 2026-11-10 beta.20: buffs column by hand in RML (core layout's boxes in px, sweep as <ua-ring sector square>, times/names/«ще N» with font-effect outline, names toggle, tip); font engine: outline effect (halo glyphs); ua-ring square sector
- 2026-11-10 beta.21: hotbar by hand in RML (core hotbar::view: every part placed and said; passive uaui::Doc - drag/wheel/keys stay the part's), grey icons/blocked/pad badges/battle icon made once
- 2026-11-10 beta.22: uaui::mouse takes a release only after its own press (owner: windows did not drag)
