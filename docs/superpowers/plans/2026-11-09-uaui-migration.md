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

## Stage 3 — runtime for parts (betas 11–12)

- [ ] T1 `win/uaui.hpp` part API: `uaui::Doc` (load RML from memory, its model, show/hide with the fade, place
      in px, box, top-most), data events → C++ (`BindEventCallback`), a doc's z among the driver's windows
      (window_stack order), blocker from the doc under the cursor, keys to a focused text field.
- [ ] T2 `<img src="pic:…">` for the driver's pictures that change (portraits, minimap, sprites): a texture by the
      picture's address and generation, uploaded again when the generation changes.
- [ ] T3 theme = today's look: window frame, title, ×, tabs, slots, bars, tips, buttons — measured from the old
      painters (core/window_panel, core/char_block, core/item_window).
- [ ] T4 speed: reuse the frame's vertices when RmlUi rendered the same geometry; one draw per texture run.

## Stage 4 — HUD (one beta each)

- [ ] character block (win/char_block + core/char_block)
- [ ] target card (win/target_card)
- [ ] party panel, mercenary/homunculus blocks
- [ ] buffs column and its further lines
- [ ] hotbar
- [ ] experience bars, experience gain, item toast
- [ ] hunt panel, MVP panel, tracker
- [ ] minimap frame and its list (the map picture as `pic:`)

## Stage 5 — windows

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

- [ ] a second and third theme (classic RO, contrast), the pick in the app and Alt+D, a theme through `updates`
- [ ] the old painters removed; `[interface] uaui` default on (owner's word)

## Ledger

(one line per finished step: date, beta, what, commit)
