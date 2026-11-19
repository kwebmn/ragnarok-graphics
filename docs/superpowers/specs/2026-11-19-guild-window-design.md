# The guild window («Гільдія», Alt+G) rebuilt - design

## Intent

A tester asked for the guild skills' pictures in «Гільдія» → «Навички» (the old game window had them, ours shows only
«name · Рів. N»). The owner asked for the whole panel thought through («продумай панель гильдии», «надо сделать
както посерьезней»), saw the concept (https://claude.ai/artifact/S84bYG99wHsnvieWQQrqd7, v2: server facts, pictures
of every tab in the game's look) and answered «делай», «все делай» - every stage, betas without stopping.

## Facts it rests on (uAthena code, conf and DB on kozak, read 19.11)

- 15 guild skills; Guild's Glory (10005) max 0 = off. The server sends (0x0162) only skills whose needs are met; «up»
  only to the master (level < max, points not checked). Points: one a guild level.
- Auras (Leadership STR, Glorious Wounds VIT, Cold Heart AGI, Sharp Gaze DEX): +1 a level, guildmates within 2 cells
  of the walking master, never the master (guild_aura 31). The game's text says «+5 in 5 cells» - wrong here.
- Battle Orders (+5 STR/INT/DEX 60 s), Regeneration (HP x2 / HP+SP x2 / x3, 60 s), Restoration (90 % HP/SP, 10 s cast),
  Urgent Call (5 s cast): only where map_flag_gvg (guild_vs1-5 always, castles in WoE; emergency_call 11); one shared
  5-minute block for all four (guild_block_skill); answered by 0x011a (skill, target, source).
- 0x01b6: tax always 0; territory = msg 300+N («Two Castles», «Total Domination»). 0x014c: alliances (opposition 0 ally,
  1 enemy, guild id, name 24), sent with the Info tab's request. 0x0154: members (account, character, hair, colour,
  sex, class, level, exp, online, position, memo 50, name 24 - 104 bytes), sent with the Members tab's request.
- Rights: member 0x57, master 0xD7 (0x80 = the notice's tab). 0x016f (the notice) reaches every member at login.
- Guild Extension: 16 + 6 a level, 76 at 10. 46 guilds, 12 with 21+ members, the biggest 68.
- The game keeps the guild skills as a std::list at the session + 0x2E4 (count + 0x2E8), a node's record at +8 (id
  +4, inf +8, level +0xC, sp +0x10, up +0x14, range +0x18); its window's «use» = the game mode's 0x7E (the record, the
  level), «+» = the game mode's 0x4A (the id).

## Decided

The owner: build the concept, all of it. The concept's open questions, decided as proposed there (rulings, to be
listed in the reply): the wide window (variant B); Guild's Glory hidden; our own server-true descriptions, the game's
English text dropped; «Використати» and the drag onto the hotbar made; allies and enemies on «Інформація»; the notice
shown to every member on «Інформація»; «Учасники» with contribution, search, sort and the side card, by default the
ones in the game first, then by position, then by level; «Наступна облога» not made (the schedule is empty, nothing to
show); the work in stages, a beta each.

## The window

One RML document (uikit/guild_live, class `gdw`), 760 rem-px wide at scale 1 (63.33rem), the same size on every tab:
the head (the emblem 28, the guild's name in gold, «рівень N · у грі X з Y», «Alt+G», ✕), six tabs underlined as
«Навички» (a gold count on «Навички» when the master has points, the members' count on «Учасники»), the body (the list
on the left, a 268 px side panel on the right), the foot (points or the role's line on the left, the mouse's hints or
the controller's buttons on the right). Tabs follow the game's own guild windows as now (0x45 switches, the game asks
the server); a tab the game does not allow is dimmed.

- «Навички»: four groups (Замок / Розвиток гільдії / Аури майстра - 2 клітинки довкола, поки майстер іде / На війні
  гільдій - активні, спільна перезарядка 5 хв); a row: the game's icon (gd_<aegis>.bmp 24x24), the game's name, the
  kind and one line, dots, «N/M», «+» (master, points, needs met, level < max). Locked skills (needs unmet - not sent)
  from our table, dimmed, «Потрібно: …». The side panel: the chosen skill - icon 48, name, kind, about, «Вам зараз»
  (auras, for a member), levels, where it works, the cooldown («ще M:SS» after one of the four), needs with ✓, buttons
  «Використати» and «Підняти до N» (master). Double click uses, drag goes to the hotbar.
- «Інформація»: the emblem 48, the name, «Рівень N · X з Y учасників · Z у грі», the exp bar, master (dot), average
  level, castles (from the server's words), your tax (your position's, when known), allies and enemies (chips), the
  notice (gold box), «Вам від гільдії» (the auras and the war skills the guild has); the side: who is in the game,
  «Усі учасники →».
- «Учасники»: search by name, sort (in the game first / level / position / contribution), columns name, class, level,
  position, contribution (% of all); the side: the chosen member's card (class, level, in the game, position and its
  rights, contribution and rank, «Написати», «Запросити в групу», «У друзі», «Посада ▾», «Вигнати…»).
- «Посади»: the positions with people (the rest folded), rights chips, tax, people; the side: the chosen one and
  «Змінити» (the game's own editor) - later our own editor.
- «Вигнані»: name and reason; the side: the chosen one's account and reason.
- «Оголошення» (master): the subject and body fields with counters (60/120 bytes) as now; the side: how all see it,
  «Зберегти», «Скасувати».
- No guild: Alt+G shows how to join and how to make one (Emperium, `/guild Назва`).

Pad: LB/RB tabs, D-pad the focus, A uses (skills) or opens the member's actions, Y raises (master), X onto the hotbar,
B back/close.

## Not in it

No server changes (uAthena untouched). No new settings: it is the same [interface] guild window.
