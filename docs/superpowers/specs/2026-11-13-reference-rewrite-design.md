# «Довідник» on the new interface - design

The owner's concept doc https://claude.ai/artifact/6sd7hxvHC1yGpnnA9KCMgk («Довідник на новом интерфейсе —
концепция», tab 1 the concept, tab 2 «Разбор: удобство и вид»), every point below confirmed by the owner on
2026-11-13 (answers in chat). Start: «начинай делать в инлайн режиме, без остановки».

## Intent

«Довідник» becomes an ordinary window of the new interface (a live RmlUi document like «Налаштування» or the shop):
movable, sized by its corner, text laid out by the library (no clipping, no overlap at any text size), real scroll
and a real search field. The data and the look the owner approved in October stay; the defects found in the review
are fixed and the chosen new things are added. The new window replaces the old one whole, with all its cards.

## Decisions (all the owner's)

Window and keys
- Moves by its header and ground; its bottom right corner sizes it; default size as now (1270x850 at 1080p,
  smaller on a small screen). A narrow window: the rail shrinks to icons, the card's blocks stack in one column.
- Opens on «Головна» (Ctrl+F, the icon row's button) - «начудил - стартовая позиция»; a card opens directly from the
  item menu «У довіднику» (in «Речі»), the target card, a monster in the minimap's list, a «[name]» in the chat.
- Ctrl+F stays its key (in the chat and in «Налаштування» it searches only where words are typed).
- While it is open, the hotbar's F-keys and the Alt window keys work in the game when the caret is not in the search
  field.
- Header: «‹ Назад» (Alt+←, Backspace on an empty field, the mouse's back button), «Головна», the page's name and
  its way, the search field (on every page; caret, selection, paste), the star «Обране», ✕.
- Rail: «Головна», Монстри, Предмети, Локації, Квести, NPC і послуги, Боси, Улюбленці, Крафт, Навички (new),
  Карта світу, Стихії (was «Таблиці»: it holds only the elements' table).

Search
- Results as one types, by kinds, as now; English names and numbers (the owner's rule: no typo search, no
  translated names).
- Typed in a Cyrillic layout (uk/ru): searched also by the same keys in Latin; a line over the results says what
  was searched («Набрано в українській розкладці — шукаю poring», a link to search as typed).
- ↑ ↓ pick a row, Enter opens, Esc first clears the field, then closes the window.
- A group shows up to 100 rows; a row of the groups on top jumps to each.
- One search field everywhere: on a map's page the header's field, the group «на цій карті» first.

Cards
- All they show now, plus: an item's description from the game (the same files the bag's card reads), under the
  name beside the icon; a boss's own timers (the HUD «Боси» panel's) first, the site's players' timers below;
  Shift+click on a name puts «[Name]» into the chat line; what the player has - «у сумці N · у візку N» (the storage
  as last seen), in «Входить у крафт» what is there and what is missing, a quest's needed things «2 з 5»; monsters'
  levels coloured by the player's level (fitting, too strong, too weak).
- One row per monster: several entries of the same thing merge into one chance 1-(1-a)(1-b)... with how it was made
  («7,73 % · 8 × 1 %», «18,12 % · 11 % + 8 %»); the same per thing in a monster's drops.
- Lists show whole; «ще N ›» only past 100 rows (no card has such a list: Elunium's 88 droppers the longest; only
  sections and the search). A list longer than 20 rows goes full width in two or three columns.
- Wide window: blocks in two columns, the next block into the shorter column (no holes); narrow: one column.
- Gold only for what can be pressed and what is chosen; numbers white.

Maps
- One map for «⤢» (the minimap's big map) and the reference: the same picture (the game's, the x4 one or «Схема»),
  the same layers, buttons and settings; the reference opens it on any map; a location on «Карта світу» opens it.
- All layers on at first (NPC, warps, monster zones, «Ви тут» and the party).
- NPC dots: Kafra, merchants, quests - their colour and a letter (K, $, !); our server's NPCs and guides - their
  colour; the rest grey. Names and cells on hover; a 24-pixel click area round a dot.
- Warps as rings; labels only at the map's exits, the rest on hover; Kafra's and ferrymen's prices.
- Zoom up to x2 on the plain picture, x4 on the x4 picture and «Схема».
- Beside the map: NPCs by kinds, whole lists, the kinds on top to jump; a row's hover lights its dot, a click opens
  the NPC's card.
- NPC card: name, kind, place; a merchant's goods with prices, Kafra's teleports with prices, a ferryman's
  destinations and prices; «Вести сюди» (the way and the footprints to the NPC); its picture when its sprite is
  found; its minimap opens the map with this NPC in the middle. No first words, no «Поруч», no service NPCs
  (names with «#», empty sprites -1/111/139).
- NPC kinds by the script's folder (the site's pack); «NPC і послуги» uses the same kinds and colours and searches
  NPCs by name on all maps.

Other
- «Карта світу» inside the same window with its zoom, drag, route and dungeons.
- «Навички» (new): skills by jobs from the site's data.
- Pad: as in other windows - the gold frame on rows, A opens, B back, LB/RB sections; Y the search with the
  on-screen keyboard; X «Вести сюди» on a card; on the map a cross as on «⤢»; hover tips shown for the row the pad
  picked.
- The site: the walkable-cells file built by itself with the data pack; NPC kinds in the pack; the owner is told
  before the site deploy.

## Not changing

The data pack and its download, the pages' content builders (core refpages), routes and «вести», the world map's
picture, «Обране» and «Нещодавні» (their files), the site's live data.

## Constraints

- Client only: no uAthena/server/NPC script changes. The site changes are staged and deployed on the owner's OK.
- Betas only (3.0.18-beta.N, one per stage), notes uk/ru/en; nothing released without «выпускай».
- Strings uk/ru/en; UI names quoted from strings.cpp; game parameters (elements, races, sizes) in English.
- RML well formed and checked on the host (RmlUi loads it without errors); the theme's window scope test green.
- Every new switch shows in the app; host tests green; 32/64-bit builds without warnings; lines <= 120 columns.

## Estimate

32-43 h: 1 window and all pages (8-10), 2 world map (2-3), 3 maps (6-8), 4 pad and cleanup (3-4), 5 the cards'
new things (5-7), 6 «Навички» (3-4), 7 search and chat (3-4), 8 the site (2-3).
