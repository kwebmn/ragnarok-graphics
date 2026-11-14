# uaRO Graphics 3.0.19-beta.5 — what's new · 2026-11-14

- **Assumptio without the square.** Under Assumptio the game draws the character's body and head once more — slightly larger, pulsing, as light. That pass now takes the game's own picture instead of the frame's HD copy, so there should be no light square around the character any more. If the square stays, send a report while Assumptio is on you: the log now has a line about this pass.

# uaRO Graphics 3.0.19-beta.4 — what's new · 2026-11-14

- **«Reference»: your quest step — from the server.** A quest's card has a «Your progress» block: the step you are on, what to do now and whom to go to (the «Next: …» line opens the NPC with «Lead me here»). The site reads your character's quest variable on the server, so «the server didn't give the variable» shows too. The first quest is Kiel Hyre; the program must be signed in to the site with the account your character is on. Refreshed once a minute while «Reference» is open.
- **The site's new quest kinds** have their own names in «Reference»: «Quest skills» and «Story quests».

# uaRO Graphics 3.0.19-beta.3 — what's new · 2026-11-14

- **«Reference» shows the same as the site:** the site now works out which monsters are in the game (on maps and the ones scripts call — the Emperium, the castle guardians, quest MVPs) and which things one can get (from boxes, albums and NPCs too). Once the site updates its data pack, «Reference» follows its word; with an older pack — as before.

# uaRO Graphics 3.0.19-beta.2 — what's new · 2026-11-14

- **«Reference»: Chonchon, Steel Chonchon, Verit, Atroce and others in the game's own sprites.** The game names their files the Korean way (chocho, vorit, atros), not as the server does, so «Reference» did not find them. Now it asks the game for the name first. The Emperium and the treasure chests are 3D models in the game — they keep the site's pictures.

# uaRO Graphics 3.0.19-beta.1 — what's new · 2026-11-14

- **«Reference»: pictures of every monster.** The client has no sprites of Chonchon, Steel Chonchon, Verit, Atroce, Beelzebub, Satan Morroc, Sword Master, Bow Master, the treasure chests and the castle guardians — their pictures from the site are now built into uaRO Graphics. 38 more monsters (the Bio Lab MVPs, Egnigem Cenia, skeletons, kobolds) now find the game's own sprites. The same in the target card and the «Pets» section.
- **«Guild» no longer falls apart** when pulled bigger by its corner: it grows with its size, three tabs a row. «Information» opens with the guild's emblem and name. The same fix in «Status» (Alt+A) and on the character select screen.
- **Warp Portal and Teleport:** the list names the maps as the game's old window did (for example «Robot Factory level 2»), the map's code small under the name.
- **The old client no longer crashes** on a clothes dye its archives lack («CPaletteRes :: Cannot find File : palette\…»): such clothes are drawn in their own colours, as the game itself does in the world.

# uaRO Graphics 3.0.18 — what's new · 2026-11-13

The game's whole interface on the new library: windows move and resize, the text sharp at any size. A new «Reference», a game launcher, playing from the keyboard, more of the controller, sharp damage numbers and crashes fixed.

## The new interface

- **Every window and panel** is drawn by the new interface library: drag them, resize most by the bottom-right corner, the text stays sharp at any size. The login, server and character screens too; the server list shows how many players are online.
- **«Settings» in the game** (Alt+O, the icon in the character block, the Esc menu) in place of the game's options, with sections and search. ✕ on our panels; the character block (Alt+V) has a second row of our windows' icons.
- **«Equipment»** as the game's original window, with the real character in its clothes and dye. Sets «1 2 3» - put a set of things on with one click.
- **Items:** names show their slots «[3]» and the cards inside (bag, storage, cart, trade, shops); element things in their element's colour. Several things at once in the storage and cart (Ctrl+click, Shift+click); the count window works out how many fit by weight.
- **«Friends / Party»:** each person's face, class and level; a menu under the row; add a friend by name.
- **Chat:** a magnifier to search the lines, Ctrl+V to paste, «[Name]» of an item or monster in a player's line opens it in the «Reference».
- No more dimming of the world under windows.

## «Reference» (Ctrl+F)

- **A window of the new interface:** sections at the left, search (in the Cyrillic layout too), back, the controller.
- **Monsters** by bands of 10 levels, **items** by types - with figures in columns; **cards** with facts: HP, experience, element, what drops and its chance, where to find, where to buy, what it is made of.
- **Locations** with their own map and NPCs by kinds; **«Skills»** - jobs and skill cards; **«Elements»** - the damage tables; the **world map** with a route.
- Shift+click on a name in the game, or «In the reference ›» in an item card, opens its card.

## The first screen - a launcher

- **The game's art over the window and one big button:** «PLAY», «UPDATE», «+ ONE MORE WINDOW» while the game runs. «Check» beside it and the «Release | Beta» switch.
- **«How the game looks»** right on the first screen, with «Auto for your graphics card»; the card checked without starting the game.
- **«Changelog»** with every version; a «Create an account» button.

## Keyboard

- **Walk mode** (the ` key left of 1): W A S D and the arrows lead the character, Tab the next target.
- **Windows by keys:** arrows, Enter, Esc in the window opened last; choosing and making a character by the arrows.
- **Menu (Esc):** «Keys» - every key on one card, «Change account»; Ctrl+Home brings the camera back.

## Controller

- Skill wheels, chat from the controller (channel, reply to a whisper, history), hotbar profiles, our windows on the Start tiles.
- The «Reference», the map and NPC talk from the controller; the attack button with no target picks up the loot nearby.

## Picture

- **Sharp damage numbers,** Miss, Guard, Critical and the crit stars; sharp snowflakes and sparkles of skills (Storm Gust and others). «Skills' look» is now «Redrawn, soft» by default.
- When frames run short, skill effects get lighter by themselves.
- **Wings** on the backs of those wearing Angel Blessing, Helm of Darkness, Wings of Diablo and other winged things (seen by players with uaRO Graphics).
- The map loading bar in the new look; grass in the guild castles' yards; torch fire hides behind walls.

## Faster and smaller

- Less work every frame (chat, windows, finding characters and monsters); a smaller update: ddraw.dll 20.5 → 16.0 MB, the app 5.6 → 4.0 MB.

## Fixed

- **Crashes:** right after entering a map (cps.dll - two threads read the game's archives), entering a guild castle, in the sound on a map change or teleport, when the server says a thing is worn at 4+ places.
- **«Negative»** and washed-out ground after some skills (Berserk), **white ground**, monsters left standing off their place, logging in after a lost connection.
- **Hangs:** when the game stands 15 seconds or more the log writes where it stands, and «The game froze» is a report of its own.

# uaRO Graphics 3.0.18-beta.256 — what's new · 2026-11-13

- «Reference» → «Elements»: the columns' names no longer run into each other. The four tables stood two to a row and a column had no room for its name. Each table now takes its own row; two to a row only in a wide window, where the names fit whole. In a narrow window the column names are cut to three letters (Neu, Wat, Ear…).
- What to check: «Reference» → «Elements» at different window sizes - the names over the columns do not touch.

# uaRO Graphics 3.0.18-beta.255 — what's new · 2026-11-13

- «What drops» shows the chance again, «Where to find» the monsters' count on a map, «Where to buy» the price. 3.0.18-beta.253 limited a row's right-hand words by a per cent of its width and the game's layout gave them no width at all. The words are whole again; a layout test checks it.
- What to check: any monster's card - the chance at the right of each thing; an item's card - the prices in «Where to buy».

# uaRO Graphics 3.0.18-beta.254 — what's new · 2026-11-13

- The reference's world map no longer shrinks to a strip (R-TMUEN4). A long way (31 moves to the Monastery) had a steps panel taller than the window; the window took that for the map's overflow and trimmed the map again and again. Now the panel is no taller than the map and its steps scroll inside it; the map is trimmed only when it does not fit itself.
- What to check: «Довідник» → MVP → Fallen Bishop Hibram → Monastery (abbey02) → the way: the whole map, the steps at the right scrolled by the wheel.

# uaRO Graphics 3.0.18-beta.253 — what's new · 2026-11-13

- «Довідник» reworked after the owner's review (R-N7GPT5, R-JSWF77, R-U5HAJ6, R-9CD8YA, R-WP985Q). A list's line now: a picture, a name, a line under it and figures at the right (a value over its label), each list's in straight columns.
- Monsters: in groups of 10 levels with jumps on top; race · element · size under the name, Lv (coloured by your level), HP and the maps' count at the right. Cut at 100, the list starts a little below your level; the rest by «all N ›».
- Items: where to get it under the name (who drops it and the chance, craft, shops), the shop's price a labelled column of its own. Cards show their effect. Craft by groups: cooking (the book's level a column), potions, weapons (the weapon's level), metals and stones, converters; the ingredients no longer run over the next column. MVP, pets, locations (players now), shops, Kafra, «dropped by» look the same.
- A monster's card: the picture stays in its frame; a grid of facts beside it - HP, Base EXP, Job EXP, and once the site's pack is updated ATK, DEF, MDEF, HIT, FLEE, STR-LUK, attack range, step, attack pace and its ways (aggressive, picks up items, ...). Zero-chance drops hidden, slots in the things' names: Club [3] and Club [4].
- An item's card: weight, price, sell price, slots and the facts of the game's description (class, attack, defence, weapon level, required level, jobs) as a grid; the description as paragraphs the whole width, the weight not twice. «Made of» shows «needed» and «in the bag» columns (red when short) instead of «є 0».
- The lists' pictures load a little at a time, at most 3 ms a frame - no stutter.
- What to check: «Monsters», «Items» (any type), «Craft», «MVP», the cards of Fabre and Autumn Red Tea. The monsters' full stats appear once the site's pack is deployed.

# uaRO Graphics 3.0.18-beta.252 — what's new · 2026-11-13

- The guild castles' courtyards grow grass, and the lawn no longer shines like a polished floor (R-9UNG8S). Castles counted as indoors by their names: no «Grass» there, and «Reflections on smooth floors» covered the whole courtyard, mirroring the banners in the grass. Now the grass grows in every castle's courtyards (prtg, payg, gefg, aldeg, schg, arug, nguild) but not on the halls' floors: Aldebaran's carpet and Payon's green tiles stay clear. The halls' floors still reflect, grass never does.
- No more sound crashes at a map change or a teleport (R-S9B6T3). The game fades its music in a thread of its own while its main thread opens and closes the music. RAD's Miles takes such calls one at a time; our mss32.dll and the driver's sound hooks did not, and the fade thread hit an XAudio2 voice already destroyed. Now the calls go one at a time.
- What to check: enter a castle by day - grass in the courtyard, none on the halls' carpets and tiles, the halls' floors reflecting. Change maps and teleport a few times, also right after coming back to the game window from another one.

# uaRO Graphics 3.0.18-beta.251 — what's new · 2026-11-13

- Entering a castle no longer throws you out of the game (R-NGSWJM, "can't get into the castle"). The game reads its data.grf archive through 256 MB "windows". In a castle its main thread was reading the guild war 3D models (Emperium, guardians) while the map loading thread switched the archive to another window at the same moment, so the model was unpacked from memory that was already gone. The game's archive reads now go one at a time, with or without the updates folder. The 3.0.15 fix did not close this cause.
- What to check: enter a few castles (prtg_cas01–05, payg_cas01–05, gefg_cas01–05, aldeg_cas01–05) — the game must not close. The log has the line "files: the game's archive reads one at a time" at the start, and in castles lines "files: … waited … ms" may appear — that is the caught case.

# uaRO Graphics 3.0.18-beta.250 — what's new · 2026-11-13

- «Reference» without the clutter: monsters not on any map (event copies, summons) and things no one can get (no monster on a map drops them, no shop sells them, no craft makes them, no quest or pet takes them) no longer show in the lists, the search and the counts. 483 of 1004 monsters stay, about 1 870 of 7 643 things. Their cards still open by a link.
- «Dropped by» on a thing's card lists monsters on maps only.
- Bigger pictures: a monster in the lists 44 instead of 24, a thing and a skill 32, a card's picture up to 150.
- What to check: «Monsters» - only those on maps; searching a thing not in the game finds nothing; the pictures in the lists and cards visibly bigger and sharp.

# uaRO Graphics 3.0.18-beta.249 — what's new · 2026-11-13

- Reports are simpler still: they no longer carry the pictures the texture pack had no better version of (up to 64 small files). The game no longer saves them either, and the old copies (the uaro-graphics\missing folder) are deleted at start.
- What to check: send a report - only your own picture and the logs go.

# uaRO Graphics 3.0.18-beta.248 — what's new · 2026-11-13

- «Reference», «World map»: the map itself is back (R-QP7PQB). Since beta 239 it showed only the players' marks and «You are here» without the locations: the window gave the picture it drew as its own background too, so it was wiped before the drawing.
- Less cut off: the header's way keeps its end (the page's name shows, the early steps become «…»); long lists go in two columns, three only in a very wide window, so names are not cut; the world map's page no longer overflows the window (no stray scroll bar).
- Reports are simpler: no more five extra pictures of the same place with parts of the graphics off (added to hunt Berserk's wash; its cause was found back in beta 229). The report is smaller and the game does not flicker while making it. The old files of those pictures are deleted.
- What to check: «World map» with its locations, zoom and drag; the header on a deep page; «Monsters» - names in two full columns; send a report - only your own picture.

# uaRO Graphics 3.0.18-beta.247 — what's new · 2026-11-13

- «Reference» no longer stutters on long lists (R-ZW43KT). «Monsters» had 1006 rows, and each time a few monster pictures came the whole page was made again - the game fell to 26 frames. Now a long list shows its first 100 rows and «all N ›» (as the reference's other lists do). Pictures load first for the rows in view and take their place without the page being made again.
- The same limit for «Items» by type, «Cards», «Locations», «Crafting», shops and teleports.
- What to check: «Monsters» opens at once and scrolls smoothly; pictures appear where you look; «all N ›» opens the whole list and it scrolls smoothly too.

# uaRO Graphics 3.0.18-beta.246 — what's new · 2026-11-13

- Getting ready for a site update for «Reference»: the app can take a fresh file of the maps' walkable cells (routes and maps) from the site as soon as the site serves it, and then it wins over the package's own file. Until the site serves it nothing changes.
- What to check: nothing new in the game; «Lead me here» routes work as before.

# uaRO Graphics 3.0.18-beta.245 — what's new · 2026-11-13

- «Reference», search: a name typed in a Cyrillic layout («зщкштп») is searched by the same keys in Latin («poring»), and a line over the results says «Typed in the Ukrainian layout - searching poring». Beside it «search as typed ›» searches the typed letters themselves.
- Chat: «[Name]» of a thing or a monster in players' lines (near, party, guild, whisper) is now a link - a click opens its card in the reference. Shift+click on a name in the reference puts such a «[Name]» into the chat line, so players with uaRO Graphics can share cards.
- What to check: Ctrl+F, type «зщкштп» in the Ukrainian layout - the line over the results and Poring; «search as typed ›»; write «selling [Poring Card]» in the chat - the link opens the card.

# uaRO Graphics 3.0.18-beta.244 — what's new · 2026-11-13

- «Reference»: a new section «Skills» (between «Crafting» and «World map»). Jobs are grouped: «Novice», «First jobs», «Second jobs», «Transcendent», «Other jobs», each with how many skills of its own it has.
- A job's page shows only its own skills (not its earlier job's) with a «Lv 10 · SP 8 · Physical» line, and at the bottom a way to the earlier job's skills.
- A skill's card: the most level and SP, kind, element, what it is used on; its description; «SP by level»; «Needs» - which skills at which level come first; «Who learns it» - every job with the skill. Levels, SP and trees come from the server's tables.
- Skill descriptions come from the site in your interface's language; the app fetches them once a day. Until then the game's own description is shown.
- The search finds skills too: a «Skills» group by the name or the English service name (MG_FIREBOLT).
- What to check: «Skills» → Swordsman → Bash → «Who learns it»; Magnum Break - «Needs: Bash Lv 5»; Fire Bolt - SP by level 12…30; search «fire bolt»; the description in your language (after the app ran once with the internet).

# uaRO Graphics 3.0.18-beta.243 — what's new · 2026-11-13

- «Reference», cards: under a thing's name is its description from the game, the same as in the bag's item card. Beside it is what you have: «in the bag 3 · in the cart 1 · in storage 12» (the storage as you last saw it in this game).
- «What it is made of» shows how many of each material your bag holds, and «Goes into» says «have all» or «missing N». A quest's needed things show «2 of 5».
- Monsters' levels in the lists are coloured by your character's level: green fits (within 10 levels), red is stronger, grey is weaker.
- A boss's card: «Your timers» from the «Bosses» panel first, the site's players' timers below. The bosses' list puts your timer first too.
- Shift+click on a name (a monster's, a thing's, an NPC's, a quest's, a map's) or on the card's title puts «[Name]» into the chat line.
- A thing's card (right click on a thing) has «In the reference ›» at the bottom - it opens its card in the reference.
- What to check: Apple's or Jellopy's card - the game's description under the name, the «in the bag …» line; a thing that can be crafted; a quest with things; the monsters' list - the levels' colours; Shift+click on Poring - «[Poring]» in the chat; right click on a thing in «Items» → «In the reference ›».

# uaRO Graphics 3.0.18-beta.242 — what's new · 2026-11-13

- Fixed: in betas 239–241 the game closed with an error the first time in a game «Reference» opened - by Ctrl+F, its button, the target's card or the controller. The window wrote its search words before it was made. Now such a write waits for the window, in every window of the new interface (R-F8FGR4).
- What to check: right after entering the game press Ctrl+F - the reference's «Home» opens and the game does not fall; close it and open it again; open the reference from the target's card (click a monster).

# uaRO Graphics 3.0.18-beta.241 — what's new · 2026-11-13

- «Reference» on the controller works like the other windows. A gold frame walks the card's rows, chips and buttons with the D-pad or the left stick, sideways between the two columns too. A opens, B goes back (on the first page it closes the window). X is «Lead me here» on an NPC's or a location's card, Y is the search with the on-screen keyboard (Start on it opens the first result). LB and RB switch the sections on the left, the right stick scrolls, Start closes the window. The bottom line shows your controller's buttons.
- Maps on the controller: A on a location's map or on the world map puts a cross on it. The left stick moves the cross (at the edge the map follows), the right stick moves the map, LB and RB zoom. A opens what is under the cross: an NPC's card, the next map, a location on the world map. X on a location's map moves the cross to the next NPC or exit, B removes the cross. The name under the cross shows just as under the mouse.
- The mouse or the keyboard take the window back: the frame and the cross go away.
- The reference's old hand layout is gone; only the world map's painter is left.
- What to check: open the reference with a controller button - the frame on the first row; the D-pad every way over a monster's card; A on a row, B back, LB and RB through the sections; Y - type «poring», Start; X on a Kafra's card; on Prontera's card A on the map - the cross, the stick, LB and RB, X, A on a Kafra; the world map - the cross and A on a town; a mouse click - the frame goes away.

# uaRO Graphics 3.0.18-beta.240 — what's new · 2026-11-13

- «Reference», a location's card: it now holds the map itself, like the minimap's big map «⤢». The wheel and the «−», «⤢», «+» buttons zoom, and the map can be dragged. Over the map are the layers «NPC», «Warps», «Monsters», «You are here». «NPC» and «Warps» are the same switches as the minimap's. «Monsters» (where monsters spawn, as red patches) and «You are here» (you and your party members' faces) are the reference's own.
- NPCs on the maps are coloured dots by kind with letters: K for Kafra, $ for merchants and services, ! for quests. Our server's NPCs are purple, guides yellow, the rest grey. Words stay on only by the map's exits; the rest show under the pointer. On your own map you see yourself and your party members' faces. The minimap's big map «⤢» shows the same letters and the same words.
- Beside the map are the location's NPCs by kind, with a row that jumps to each kind. A click on an NPC in the list or on the map opens its card: kind, map and cell, «Lead me here ›» (leads to it, from other maps too), «Show on the map ›», its goods with prices, and «Takes you to» for ferrymen. A click on an exit on the map opens the next map's card. An NPC found by the search opens its card too.
- «NPCs and services»: a «By kind» block on top - how many NPCs of each kind there are on all the maps. A click on a kind lists them all with their map and cell.
- What to check: Ctrl+F, «Locations», Prontera - the wheel over the map, dragging, «−» «⤢» «+»; switch each layer off and on (for «NPC» and «Warps» the minimap should change too); point at a dot - its name; click a Kafra - her card, «Lead me here ›» from another map; click an exit; «NPCs and services» → «By kind» → «Kafra»; the minimap's big map «⤢» - letters in the dots, words only by the exits; in a party - your friends' faces on your own location's map.

# uaRO Graphics 3.0.18-beta.239 — what's new · 2026-11-13

- «Довідник» (the reference) is now an ordinary window of the new interface. Drag it by its header or background and size it by its bottom right corner. A narrow window shows the left sections as icons and stacks the card's blocks. The window lays out its own text, so nothing is clipped or overlaps. A real scroll bar and a real search field: caret, selection, paste.
- Esc clears the search field first and closes the window the second time. While the caret is not in the search field, F1–F9 and the Alt windows work in the game. Ctrl+F always opens «Головна».
- Cards: blocks in two columns, the next block into the shorter one. Lists show in full; «ще N» appears only past 100 rows. A long list (over 20 rows) spans the width in two or three columns. Search results have a row on top that jumps to each group.
- One row per monster: several database entries of the same thing show as one total chance and how it was made. For example, Hugeling on the Apple card: «7,73 % · 8 × 1 %» instead of eight rows of 1 %.
- «Таблиці» is now called «Стихії». Gold marks only what can be pressed and what is chosen; numbers are white.
- The world map lives in the same window: drag, wheel zoom, a click on a location opens its card.
- What to check: Ctrl+F, type a name, ↑ ↓ and Enter; drag the window by its header; pull the corner to make it narrow; Esc twice; open a card and press F1 — the game's bar must act; the Apple card — Hugeling once with «8 × 1 %»; Ctrl+M — the world map: drag, wheel, click a town.

# uaRO Graphics 3.0.18-beta.238 — what's new · 2026-11-13

- The scrollbar in the new windows («Settings», the shop, the storage and others): a press on it was taken for a press on the window's ground, and the window followed the cursor together with its bar, so only the mouse wheel scrolled. Now the bar can be dragged and clicked in every window.
- The shop: the list's head «Item · Price · Weight» lay over the first row, and the first item's icon stuck out of the frame. The cause was the style of the screen's red edge at low HP, which reached any element of the same class name in other windows. Three more such clashes were found and removed: the character block, the target card, the login's password field. A new test checks all the markup so such clashes do not come back.
- NPC talk: in the frame where the game makes its window (a Kafra's first words) the old white window showed for a moment. Now the game's window is hidden before the game draws it.
- To check: scroll «Settings» (Alt+O) by dragging the bar on the right; open a shop - the head is above the list, nothing overlaps; talk to a Kafra for the first time after a start - no old white window.

# uaRO Graphics 3.0.18-beta.237 — what's new · 2026-11-13

- Wings (R-FAFJWS «the character's body blinks»): the wings took the depth of the character's first drawn layer. The game sorts the body and the head by the depth of their tops, and the head now and then comes first - then the wings stood nearer than the body and covered it, switching from frame to frame, so the body blinked. Now the wings always stand a little behind all of the character's layers (facing us) or a little in front of them (facing away).
- To check: restart the game (it updates to .237), put on Angel Blessing (5207), stand, walk, turn every way - the body does not blink, the arms and shoulders are over the wings; if something is wrong, a new report with the «wings: …» lines in its log.

# uaRO Graphics 3.0.18-beta.236 — what's new · 2026-11-13

- Wings (R-4U3RD7): the .235 log showed where they stopped - the driver read the body's animation from the wrong file (.spr, not .act). Now the body's animation is found by its sprite's name, and the wings should show on the back.
- To check: restart the game (it updates to .236), put on Angel Blessing (5207) - big wings on the back; if not, a new report (not a reply to the old one) with the «wings: …» lines in its log.

# uaRO Graphics 3.0.18-beta.235 — what's new · 2026-11-13

- The wings were not drawn (R-TT95RZ: Angel Blessing on, no wings): the game keeps a body sprite's name without «data\», and the driver did not know the body. Now the body is known by its folder however the name starts. The log has a line for each step («wings: the players' render runs…», «… waiting for its faces», «the first wings drawn…» or why not).
- To check: put on Angel Blessing (5207) or another item of the list - big wings on the back; if not, send a report at once, without restarting the game.

# uaRO Graphics 3.0.18-beta.234 — what's new · 2026-11-13

- Wings: the wings' items draw their own headgear on the head again, as always (the owner's call); the big wings on the back come from the driver as before.
- To check: put on an item of the list - its headgear on the head, the big wings on the back.

# uaRO Graphics 3.0.18-beta.233 — what's new · 2026-11-13

- Wings: for players with uaRO Graphics the wings' items (Angel Blessing, Helm of Darkness, Wings of Diablo, Phoenix Crown, Soul Wing, Soul Ring, Valkyrie Feather Band, Emperor's Laurel Crown, Sacred Palma, Emperor Wreath, Flower Crown, Red Deviruchi Hat) no longer draw their own headgear - the big wings on the back alone. The character in «Спорядження» (Alt+Q) wears none of it either. Players without the driver see the usual headgear.
- To check: put on any of these items - nothing on the head, big wings on the back.

# uaRO Graphics 3.0.18-beta.232 — what's new · 2026-11-13

- Wings: a new list of items - only ones the 2007 game knows, so players without the driver get no error boxes and nothing odd (they see the usual headgear). Angel Blessing - angel, Helm of Darkness - fallen angel, Wings of Diablo (and Refined) - demon, Phoenix Crown - fire, Soul Wing and Soul Ring - ice, Valkyrie Feather Band - valkyrie, Emperor's Laurel Crown, Sacred Palma and Emperor Wreath - gold, Flower Crown - sakura, Red Deviruchi Hat - crimson demon. Only the GM hands them out. The .230 items (Wings Of Victory and the others) give no wings any more - do not hand them out.
- To check (GM): @item with the item's number on a test character (5207, 5181, 5044, 5152, 5297, 5256, 5205, 5303, 5227) - no error boxes; the small headgear on the head and big wings on the back, from every side, walking, sitting.

# uaRO Graphics 3.0.18-beta.231 — what's new · 2026-11-13

- The «Cannot find File : sprite\악세사리\…» boxes no longer show when you or someone near wears a headgear the 2007 game does not know (as Wings Of Victory and other items of newer clients): the game read another word in place of its name and looked for a file that is not there. Now it looks for nothing for such a headgear (the log's «wings: … past its … names»).
- For the GM: players without uaRO Graphics still get those boxes - do not hand out the wings' items until their list changes (to items the game knows).

# uaRO Graphics 3.0.18-beta.230 — what's new · 2026-11-13

- Wings on characters (first beta): big wings on the back, drawn by the driver for whoever wears one of nine rare items (the GM hands them out). Wings Of Victory - angel, Flying Evil Wing - fallen angel, Wings of Diablo (and Refined) - demon, Red Wing Hat - fire, Filir's Pinions - ice, Sigrun's Wings - valkyrie, Bijofnil Wings - gold, Fairy Flower - sakura, Majestic Evil Horns - crimson demon. The pictures are the official wings of the uaRO HD client and their recolours. Every player with uaRO Graphics sees them; none in «Пауза».
- To check (GM): put each item on a test character in the classic client and in HD - the game takes it calmly; the wings show from every side, walking, sitting, on a Peco; none while in Hiding. If no wings show, send a report (its log has «wings: ...» lines).

# uaRO Graphics 3.0.18-beta.229 — what's new · 2026-11-12

- Fixed the «negative» and the washed-white ground after some skill effects (R-H2CQCJ, the old Berserk reports on moc_pryd05 and moc_fild11): the game draws some effects in inverted colours and never sets the usual ones back - after such an effect the whole world turned into a negative or the ground washed white. Now the inverted colours stay on the effect itself. The switch «Без негативу після ефектів умінь» in the group «Виправлення гри» (on; in «Пауза» the game as it is).
- To check: the same skills (Assumptio, Berserk and others) - the world and the ground in their usual colours; if a negative or a wash still shows, send a report - its log will have a line about the inverted texture.

# uaRO Graphics 3.0.18-beta.228 — what's new · 2026-11-12

- The character make by the keys: ↑ ↓ go through the rows - the name, the hair, its colour, the three stat pairs (a frame on the row); ← → change the row chosen (in a pair: the point left or right), and on the name's row they move its caret as before. Enter - «Створити», Esc - «Скасувати» (the game's). Under the pairs: the hint «↑ ↓ рядок · ← → змінити».
- To check: on the make screen ↓ - the frame on the hair, ← → change it; on down to the pairs and back up to the name (← → move the name's caret again); type a name, Enter - the character is made.

# uaRO Graphics 3.0.18-beta.227 — what's new · 2026-11-12

- «Спорядження» (Alt+Q): the character's head is over the body again - the body's neck no longer covers the mouth (R-UC9NT7). Every job had it since beta .126: the job's .imf layer order was read the other way round. Now the order is the game world's; in the frames where the game puts the body over the head, the low headgear goes under the body too, as in the game.
- To check: Alt+Q - the face shows whole (novice, rogue, priestess); turn the character to all 8 sides - the head and the headgear in place, as in the game world.

# uaRO Graphics 3.0.18-beta.226 — what's new · 2026-11-12

- The menu (Esc): a new row «Змінити акаунт» on its very top. The game opens the character select and the driver presses its «Назад» at once - you are on the login screen, where another account can be picked. «Вибір персонажа» stays where it was.
- To check: Esc → «Змінити акаунт» - the character select is not shown (or flashes for a moment), the login screen opens; then Esc → «Вибір персонажа» - it stops at the character select as before.

# uaRO Graphics 3.0.18-beta.225 — what's new · 2026-11-12

- The keys' cheat sheet in the game: the menu (Esc) has «Клавіші». The menu card shows every key of uaRO Graphics (the same list as the app's «Головне»): the keys in gold on the left, what they do on the right; «Назад» or Esc bring the menu's buttons back.
- Ctrl+Home (the chat line empty) - the camera eases back to the one the game starts with: not turned, tilted -45, 300 away.
- The sets in «Спорядження» by the controller and the keys: down from «Набої» into the sets' rows (the focus frame on the row). A (Enter) - «Надягти» (an empty set: keep what is worn), X - «Зберегти надіте», Y (the menu key or Shift+F10) - «Очистити»; the button hints at the window's bottom say what each does.
- To check: Esc → «Клавіші» - the list reads, long lines wrap, «Назад» and Esc bring the menu back; Ctrl+Home after turning the camera; in «Спорядження» the arrows down to the sets, Enter puts one on.

# uaRO Graphics 3.0.18-beta.224 — what's new · 2026-11-12

- «Навички» → «Дерево» (R-3ZE2X9: «the rogue's Plagiarism is not shown, the skills go past the list's frame»): the Rogue's tree is wider than the window and its right columns were drawn on past its edge - under the skill's side panel, without names. The tree is now cut at its edge, and the mouse wheel, when there is no room up and down, scrolls it sideways (as Shift did). Plagiarism is in the Rogue tree's sixth column; the client's English names call it «Intimidate» (the real Intimidate being «Snatch»).
- «Речі» (R-5K5NRW: «says success but it is not put on»): after a double click on a thing, the green «вдягнуто» rose at once, before the server answered - even when the server refused (a level or a job short). The word now waits for the answer: green «вдягнуто» when the thing is on, red «не вдягнуто» when the server refused (why - the game's own chat line says).
- To check: a Rogue → «Навички» → «Дерево» - the right skills stay out of the side panel, the wheel scrolls sideways to Plagiarism («Intimidate»); a double click on a thing that cannot be worn - red «не вдягнуто».

# uaRO Graphics 3.0.18-beta.223 — what's new · 2026-11-12

- A game that will not start (R-H42AS4, R-3PETVR: «the process is there, the game never starts»): such reports had no log at all - the game stopped before the graphics driver begins writing. The driver now watches the start from the very beginning: if the game has not come to its window in 25 seconds, it opens the log and writes where the game stands (a few times, by module), its command line, every module in the process (other programs that hook into the game show), the game's windows and a minidump for the report. An error box of the game's hidden behind other windows or off every screen is brought to the front, onto the main screen.
- To check: a normal start - no «start watch» lines in the log (they come only when a start drags on). If the game will not start - wait half a minute and send a report from the app: it carries the log and the minidump.

# uaRO Graphics 3.0.18-beta.222 — what's new · 2026-11-12

- Sharp words at any size - now everywhere (the owner: «делай»; as R-BN9TJA's character block in 3.0.18-beta.221): the target card and the party panel (their size in «Розміщення інтерфейсу») and the windows sized by their corner (NPC talk, messages and questions, character making, whispers, the shop windows, chat rooms, cards - ~20 windows) no longer shrink a finished picture. Each is laid out anew at its size and its letters drawn at their own size - sharp small or big. For it the interface library (RmlUi) got a document's own scale.
- The target card stays in the middle when resized (it slid a little), and its showing/hiding slides smoothly again.
- To check: «Розміщення інтерфейсу» - make the target card and the party panel smaller: the words sharp; pull an NPC talk or a message window by its corner - smaller and bigger, the words sharp, the buttons pressed, the focus ring (keys/pad) in its place; character making made smaller by its corner - the face shrinks with the window.

# uaRO Graphics 3.0.18-beta.221 — what's new · 2026-11-12

- The character block (R-BN9TJA: «shrunk, its words get cut like a picture and lose readability»): its size in «Розміщення інтерфейсу» no longer scales a finished picture - the block is laid out anew at that size and its letters drawn at their own size, sharp small or big. The lengths inside it are in its own units now, the proportions as they were.
- To check: «Розміщення інтерфейсу» (Ctrl+Shift+L) - make the character block smaller and bigger: the words sharp, nothing out of place; Alt+V (full / compact / folded) - all in place; the tips under the cursor - in their places.

# uaRO Graphics 3.0.18-beta.220 — what's new · 2026-11-12

- The sets in «Спорядження» (Alt+Q) made plain (the owner, R-76RF6H: «still did not understand how it works, make it plain»): in place of the numbers «1 2 3», a «Комплекти» block with a row for each set: its number, its things' icons and names (or «порожній») and buttons with words - «Надягти» (or «✓ Надіто» when it is on already), «Зберегти надіте» (keep in this row what is worn now) and «Очистити». The line over them says what came of it: kept (things: N), putting on…, worn, not in the bag - ….
- A set takes off what it has not (the owner: «снимать»): putting it on first takes off what is worn in slots empty in the set (a manteau, arrows…), so the character is dressed just as kept; a two-hander's other half is left alone.
- «Довідник» (R-5773KD, R-FSNPYE): the pink round a location's map picture is gone - the game's magenta is see-through now, as on the minimap, on every map.
- The hotbar (R-BJZH75) no longer jumps aside when the cursor points at something whose name shows over it: the name under the cursor is no window, the bar stays.
- To check: Alt+Q - «Зберегти надіте» in row 1, take something off, press «Надягти»; put on a manteau, press «Надягти» on a set without one - the manteau comes off; «Довідник» → Локації → Prontera - no pink bars; point at an NPC at the screen's bottom - the bar stays.

# uaRO Graphics 3.0.18-beta.219 — what's new · 2026-11-12

- The «1 2 3» sets in «Спорядження» (Alt+Q) redone (the owner: «still did not understand how to use the 1 2 3 buttons, and it feels not functional»): a click on an empty number did nothing, and keeping a set was the right button's alone, with nothing telling it. Now:
  - point at a number - a tip over the row: what the set holds (things with their refine) and what a click does;
  - a click on an empty number keeps what is worn now; a click on a kept set puts it on; one worn already says so;
  - the right button - a menu «Зберегти надіте» / «Очистити»;
  - the line under the numbers says what came of it: kept (things: N), putting on…, worn, not in the bag - …, not all of it could be worn;
  - the number of the set worn now is lit in gold; the light under the cursor works now (it never did).
- To check: put something on, click «1» - «збережено»; take a thing off, click «1» - it is on again; right button on «1» - «Очистити».

# uaRO Graphics 3.0.18-beta.218 — what's new · 2026-11-12

- «Спорядження» (R-RASZHM's picture): the hint «Комплекти: клік — вдягти, права кнопка — запам'ятати вдягнене» was drawn over the «1 2 3» buttons and the row stuck to the window's left edge. The hint had a class name the new interface already uses for its sheets (absolutely positioned); it has its own now and stands after the buttons, the row in from the edge as the ammo's.
- To check: open «Спорядження» (Alt+Q) - the «1 2 3» buttons and the hint side by side, nothing over another.

# uaRO Graphics 3.0.18-beta.217 — what's new · 2026-11-12

- uaRO HD's own things - Familiar Power (#15001), Lycan Fury (#15002), Holy Order Relic (#15003) - have pictures in the client, but their files are named only in the English table (data\english\idnum2itemresnametable.txt), and our windows read data\'s alone. Now the tables of the language folder the game reads its own from (data\english\ for Ukrainian and Russian Windows) fill data\'s: these things show their pictures in «Речі», «Спорядження», the hotbar, the shop, the trade and «Довідник». Entries data\ already has are never changed; the log says how many were added.
- To check: Familiar Power in «Речі» and in «Спорядження» («Низ голови») - with its mask picture.

# uaRO Graphics 3.0.18-beta.216 — what's new · 2026-11-12

- Report R-JPMWSD («moving the cursor, it lands on a letter, not between letters»): the chat line's caret was put by the text's width as GDI measured the whole line at once, while the new interface draws the letters one by one, and in Latin text the two widths came half a letter apart. The chat now measures its words the way the new interface draws them, so the caret stands between the letters (in the message line and in a whisper's name). Once a session the log gives both widths of the line, not its words.
- «Спорядження» logs what each slot holds when the window opens and whenever what is worn changes («equip: worn - …»): reports like R-RASZHM («put a thing on and it went missing») can now be read from the log.
- To check: type a few English words into the chat and move the caret with ← → - it stands between the letters; open «Спорядження», put a thing on and take it off - «equip: worn» lines in the log.

# uaRO Graphics 3.0.18-beta.215 — what's new · 2026-11-12

- Report R-8KPDY9 («the keyboard pulled right all the time»): an arrow was pressed with the walk mode off, then ` turned the mode on while the arrow was still held. The walk mode took the arrow's let go, and the arrows' hook never learned it was let go, so the character walked right on its own for half a minute. Now each frame the held arrows, the walk mode's keys (W A S D, the numpad) and the windows' keys are checked against what Windows holds: a key let go stops the walk whichever part of the driver took its message (the log says so in a line «let go taken by another hook»).
- ` held switches the walk mode once, not back and forth with the auto repeat, and types nothing into the chat.
- Walk mode: two keys one way (W and ↑) - one let go, the character walks on while the other is held.
- To check: hold →, press `, let go of → - the character stops; in the walk mode hold D and click a field of the game's (a count, a search) - the character stops; hold ` for a second - the mode switched once.

# uaRO Graphics 3.0.18-beta.214 — what's new · 2026-11-12

- Report R-CX44S5 («the awakening potion button got stuck»): Space in «Речі» acted as the pad's X and took the item up to put it on the bar, and the keyboard could not finish that. Space is now no window button. When the pad carries a thing to the bar, the keys work too: the arrows pick the place, Enter puts it, Esc cancels, and the hint names the keys.
- The walk mode (`) no longer turns itself back on or off right after the key: the driver's own write of the setting, read back, is no longer taken for a change made in «Налаштування».
- To check: in «Речі» press an arrow, then Space - nothing is taken up; X on an item with the pad, then Esc on the keyboard - the carry is cancelled; ` twice in a row - the mode stays as pressed.

# uaRO Graphics 3.0.18-beta.213 — what's new · 2026-11-12

- The character select's carousel (the owner: «hold left/right and it spins endlessly and cannot be stopped»): each left/right is now a step from the place shown, not from the one already asked for - a held key turns the carousel at the game's pace and stops as soon as it is let go; a place the game does not move to is left after 4 s (and the log says so).

# uaRO Graphics 3.0.18-beta.212 — what's new · 2026-11-12

- Keyboard, fixes after the review: the arrows do not walk the character while the caret is in another of the game's fields (a count, a name) - they move the caret there; a key's action pressed while an NPC dialog or a menu is open no longer fires later, when the window closes.

# uaRO Graphics 3.0.18-beta.211 — what's new · 2026-11-12

- Character select from the keyboard: after an arrow, ←/→ go through the characters, Enter plays, Space makes one, Esc goes back, PgUp / PgDn the pages (as in the windows).
- «Main» lists every key for playing from the keyboard: the arrows, Space, the walk mode, the windows; the hint of «Walk mode (` key)» lists its quick keys.
- The first screen: the arrows move between the buttons by place (←/→ in a row, ↑/↓ between rows), Tab one by one; F5 is «Check» for updates. «Settings»: Ctrl+PgUp and Ctrl+PgDn the page before and after (Ctrl+F the search, as before).

# uaRO Graphics 3.0.18-beta.210 — what's new · 2026-11-12

- Windows from the keyboard: the window opened or clicked last («Items», «Storage», «Cart», «Skills», «Equipment», «Stats», «Trade», the menu and the others) listens to the keys after the first arrow (with the chat line empty), as it does to a gamepad: the arrows move the gold frame, Enter presses (use, wear, move), Esc closes, Space is the X action, PgUp / PgDn the tabs, Shift+F10 or the menu key the card. The window's hints show the keys' names in place of the gamepad's buttons. A click on the ground gives the keys back to the world (the arrows walk again); text in the chat line takes them back too.

# uaRO Graphics 3.0.18-beta.209 — what's new · 2026-11-12

- Quick keys: Space with the chat line empty does what the gamepad's A does - attacks the target, else picks up loot beside, else attacks the nearest monster, else talks to the NPC near. Ctrl+←/→ turns the camera 45°, Ctrl+↑/↓ zooms it in and out. The switch: «Interface» → «Space acts». In the game's battle mode (/bm) Space still opens the chat.
- In the walk mode (`): Tab and Shift+Tab pick the next and the previous target (the nearest first, with the gold ring), Space attacks, F picks up, R talks to an NPC, C sits or stands, Q and E turn the camera, 1-9 use the cells of the bar row shown now (F12 turns the row). Skills from the keys hit the chosen target without the mouse.

# uaRO Graphics 3.0.18-beta.208 — what's new · 2026-11-12

- Walk mode (the owner's idea): the ` key (left of 1, in any layout) switches it on while the chat line is empty. Then the letters do not type into the chat; W A S D, the arrows and the numpad (7 9 1 3 diagonally, 5 stops) walk the character; Enter opens the chat line, Enter again sends it and goes back to walking, Esc clears the line. The chat line says «Walking · Enter: type · `: off». Alt+letters, Ctrl+digits, F1-F9, Insert and Esc work as always. The mode is kept between starts; the switch: «Interface» → «Walk mode (` key)». Quick keys (target, attack, pick up) come in the next beta.

# uaRO Graphics 3.0.18-beta.207 — what's new · 2026-11-12

- Walking with the arrows (testers: «control with the keyboard's up, down, left, right»): while the chat line is empty the arrow keys walk the character - up is away from the camera, two arrows go diagonally; let go and it stops at the nearest cell. The same arrow as the gamepad's shows on the ground, and by a wall the character slides along it. With text in the chat line the arrows move its caret as before; in an NPC menu, a shop and «Reference» they belong to the window. The switch: «Interface» → «Walking with the arrows».

# uaRO Graphics 3.0.18-beta.206 — what's new · 2026-11-12

- While the game runs, the big button stays gold and says what it does: «+ ONE MORE WINDOW», with «The game is running · windows: N» under it (how many game windows are open). It was a dark «GAME RUNNING · + WINDOW» before, more like a label than a button.

# uaRO Graphics 3.0.18-beta.205 — what's new · 2026-11-12

- The first screen has a «Create an account» button at the top: it starts the game with the registration open (in the game, as «Create an account» on the login screen; the site's registration page when the site does not answer). With an update offered, the game starts for the registration without it.

# uaRO Graphics 3.0.18-beta.204 — what's new · 2026-11-12

- «Items», «Storage» and «Cart»: the scroll bar right of the cells can now be dragged with the mouse, and a click on the bar moves there (it only showed where you are in the list before).
- White ground (R-7XY449, «logged in and it went white at once»): an «Own ground» texture that was not read (the game was loading the map) is read again; one still not read, or with no room on the card, gives the game's own ground on that map instead of a white one. The log now names the texture not read and the one drawn white.

# uaRO Graphics 3.0.18-beta.203 — what's new · 2026-11-12

- The first screen shows the logo from the game's own login picture (the wings, the crystal, «UARO», «RAGNAROK ONLINE») in place of the «uaRO» words.
- «Check» is now a part of the big button: ⟳ | «PLAY» | ▾ - one button; while an update comes ⟳ does not click.

# uaRO Graphics 3.0.18-beta.202 — what's new · 2026-11-12

- After an update the program opens the first screen again, and the game starts only by «PLAY» - after «UPDATE» and after an update at the game's start alike.
- «Check» now stands beside «PLAY» («Checking…» while it asks); the «Updates» icon at the bottom is gone.
- Under the button the update channel switch «Release | Beta»: a click changes the channel and checks for updates at once; the version line beside it says what is out.

# uaRO Graphics 3.0.18-beta.201 — what's new · 2026-11-12

- Fixed: after Teleport level 1 (while the character walked) the game left it on its old place - «hanging in the air» over its shadow, not moving, the minimap showing the old position. The server stops the character and moves it at the same moment; the smooth easing to the stop's cell no longer undoes the move.
- On the first screen the «Updates» icon beside «Settings», «Reports», «Site», «Forum» checks for updates in one click («Checking…» while it asks), and the version line under the button says what is out.

# uaRO Graphics 3.0.18-beta.200 — what's new · 2026-11-12

- «Close» (and Esc) in the settings now closes only the settings and goes back to the first screen, not the whole app. The window's cross closes the app.
- The big button when an update is out is «UPDATE»: the app updates and opens the first screen again, the game does not start by itself. Play with «PLAY» after the update, or «Play without the update» at once.

# uaRO Graphics 3.0.18-beta.199 — what's new · 2026-11-12

- When an update is out, the big button says «UPDATE AND PLAY» (the version and its size under it), and right under it, in sight, «Play without the update»: the game starts as it is, the update waits.
- The first start: the app looks at the graphics card at once and puts in the picture and the textures for it - «How do you want to play?» shows «Already chosen for …». The cards only change that choice, «Skip» keeps it.
- Under the logo: just «Ragnarok Online».

# uaRO Graphics 3.0.18-beta.198 — what's new · 2026-11-12

- «Check the graphics card» in the «How the game looks» card no longer starts the game: the app asks Windows about the card itself (its name, its memory and, when the game already ran on it, how it drew it), puts in the picture and the textures for it at once and says what it found: «Checked: … - by its memory». For 5 s «Changed · Undo» takes it back. The «Graphics test» with the game stays in «Service».

# uaRO Graphics 3.0.18-beta.197 — what's new · 2026-11-12

- The first start of a freshly unpacked game: the card picked (or «Skip») is now remembered at once - before, until the game's first start, the cards could come back at every opening of the app.

# uaRO Graphics 3.0.18-beta.196 — what's new · 2026-11-12

- The first start shows the game instead of the settings: four cards with pictures, «How do you want to play?» - «I want everything new», «New picture, the old interface», «A weak computer, but sharp», «As in 2004». Under each: the picture, the interface and the textures it sets; the one for your card is marked «for your graphics card». One click and it is set; «Skip - the app chooses itself» keeps the automatic choice. Change it any time by the look line over «PLAY».

# uaRO Graphics 3.0.18-beta.195 — what's new · 2026-11-12

- «Changelog»: a click on the «What's new» card opens a screen with every version. On the left the versions by day («Today», «Yesterday», dates), marked «beta» / «release» and «new» for those out since you last looked; on the right what changed in the chosen one. At the top the filters «All», «Releases only», «New for me · N».
- A point that names a setting has «Try it» beside it: it opens the settings searched for that name.
- On the «Release» channel the changelog lists the releases only, each with its betas' points. The wheel scrolls the list and the page, ↑ / ↓ walk the versions, Esc or «← To the game» go back.

# uaRO Graphics 3.0.18-beta.194 — what's new · 2026-11-12

- «How the game looks» right on the first screen: a click on the line over «PLAY» opens a card with pictures - «Picture» (Auto, As in the game, Improved, Remaster), «Interface» (New, New screen, the game's windows, Classic), «Textures» (HD, HD light, Original); the current choice outlined in gold, «changed N» when something was changed by hand.
- A pick is written at once; for 5 s «Changed · Undo» takes it back.
- On top of the card: «Auto for your graphics card» - the picture and the textures for your card in one press (a tick when they already are). «Check the graphics card» starts the «Graphics test», «All settings ›» opens «Main». Esc, ✕ or a click outside closes it.

# uaRO Graphics 3.0.18-beta.193 — what's new · 2026-11-12

- A new first screen: the game's launcher. uaRO HD's art over the whole window (Prontera's fountain at dawn, by day, at sunset and in winter; the pictures change slowly while the window is in front and the game is not running), the big «PLAY» button, how the game looks over it, the version under it. On the left «What's new»: the update's card with its first points and the site's news. At the top: the server and the players online, the language.
- «PLAY» says what it will do: «UPDATE · N MB» when a new version is out, «GAME RUNNING · + WINDOW» while the game runs (one more window). The «▾» beside it: «One more game window», «Safe start», «Play without the update». Enter is «PLAY».
- All the settings are behind the «Settings» icon (every page, as before), back with «← To the game». Beside it: «Reports» (with the red count of new answers), «Site», «Forum».
- A click on the version under the button: the channel «Release» / «Beta (testing)», «Check for updates», «Another version…». The safe mode, uaRO Graphics switched off and the way back after two failed starts show as a plate over the icons, with one button.
- The settings' list has no «Play» page any more: «Graphics test», «Start the game at once» and the update channel are in «Service», «★ New features» and «Changed only» on «Main». The window's frame is dark.
- «What's new» no longer opens by itself after an update: the «Updated to …» card tells it (a click shows its notes).

# uaRO Graphics 3.0.18-beta.192 — what's new · 2026-11-12

- «What's new» shows your version's changes again: since 3.0.18-beta.56 the betas' notes had no version headings and the button opened an empty window. Each beta now has its heading and date - the ground for the «Changelog» of the new first screen.

# uaRO Graphics 3.0.18-beta.191 — what's new · 2026-11-12

- The maps' torches: «New flame» now hides behind the walls standing in front of it - the flame, its glow and its light stand at their torch's true place from the map file. The game draws the flame nearer to the camera than its torch stands, so the fire and its red light came over the top of the wall in front of it (moc_pryd05). Torches their makers set a little into a wall are brought out of it so they do not vanish.
- «New report»'s check shots are all five now: the report's window opens once they are taken (in a second) - the game behind the window draws no more.

# uaRO Graphics 3.0.18-beta.190 — what's new · 2026-11-12

- The count window («Into the storage», «Into the cart», «Drop on the ground»…): Enter with its field empty now takes the most, as the «Done» button does - Enter moved nothing before. Typed digits Enter takes as they are.

# uaRO Graphics 3.0.18-beta.189 — what's new · 2026-11-12

- «New report» now adds up to five check shots of the same place to its picture: right after it the driver draws the frame for a moment without «Own ground», without the «Ground and walls» group, without «Light» and «Fire and smoke», without the «Whole screen» group and as in Pause. In one report they show which part makes a wrong look (such as the ground washed pink-white after Berserk). The game blinks for half a second meanwhile.
- The own ground's log tells every minute the game's ground corners' mean colour, the fog and the stages, and a jump of the mean colour at once.

# uaRO Graphics 3.0.18-beta.188 — what's new · 2026-11-12

- As with Wind Walk's icon: the items' and skills' icons, the skills' and items' name tables and the cards' words not read (the game's files busy while a map loaded) are read again a few seconds later instead of staying empty for the rest of the game. With the names found whatever their case (.187) the driver finds the files the game finds - for its own ground and the map's models too.

# uaRO Graphics 3.0.18-beta.187 — what's new · 2026-11-12

- Wind Walk's icon (and the like) shows in the buffs' column again (the tester: «yesterday orange, now grey»). The cause: a buff come while a map loaded found the game's files busy, and the driver read its icon by its own reader of the archives, which compared names with their letters' case - the game asks for "effect\ICON06.tga", the archive holds "icon06.tga". Names are found as the game finds them now, whatever their case (cp949's Korean names as they were), and an icon not read is read again a few seconds later instead of a dark tile for the rest of the game.

# uaRO Graphics 3.0.18-beta.186 — what's new · 2026-11-12

- Lighter skill effects when frames run short: below "Keep the frame rate" with many skills on the screen (a fight, a guild war), the driver turns on the game's own lighter effects - as its /mineffect - for the new effects, and the full ones again when the frames are back or the skills gone for 20 s. Your own /mineffect is left alone. The switch: Frames → "Lighter skill effects when frames run short" (on; works when "Keep the frame rate" is set). When it turns on - a short note on the screen and a "light skills: …" line in the log.
- "Skills' look" is now "Redrawn, soft" by default.
- The chat after an NPC teleport in the battle mode: for the first 5 seconds on a new map, with the chat's field empty and no Enter of yours there, the chat's typing is turned off - the letters are the skills' again (the tester: the chat active after a teleport through an NPC). The log says "chat: the battle mode's typing off N ms into the new map".

# uaRO Graphics 3.0.18-beta.185 — what's new · 2026-11-12

- Damage numbers sharper still on large screens: on 4K the game stretches a digit 12 times, so its copy is now 16 times larger (xBR twice) instead of 4 - sharper edges, smoother curves, the same look. Miss, Guard, Critical!!, Lucky and the stars, which the game stretches less, stay four times larger. The same switch - Graphics → Quality → "Crisp damage numbers".

# uaRO Graphics 3.0.18-beta.184 — what's new · 2026-11-12

- Sharp snowflakes and sparkles for every class's skills now, not only Storm Gust's: the driver finds the skills' textures that are sheets of small dots on black (snow, sparks, glitter) and draws each dot anew at the screen's resolution - in its place, of its size, colour and brightness. Besides Storm Gust's snow, effects 192, 204, 205 and 219's sparkles come out so already. In "Skills' look" → "Improved original", "Redrawn" and "Redrawn, soft"; "Original" stays the game's. At most one new texture is looked at a frame, the sprite cache is left alone. The log says "skills' specks: effect N's sheet …".

# uaRO Graphics 3.0.18-beta.183 — what's new · 2026-11-12

- One item for every class's skills - "Skills' look" at the top of the Effects page (by the time of day, season and weather; in the game Alt+D): "Original" - as the game draws them; "Improved original" - the game's, only sharp (Storm Gust's snowflakes so far, more skills next); "Redrawn" - the classes' new looks; "Redrawn, soft" - the former "Soft look". The separate boxes of the classes, the mages' elements, "Soft look" and "Storm Gust sharp, as the game's" are gone from the page and the console; an older settings file is read: with "Storm Gust sharp, as the game's" on it starts as "Improved original", with "Soft look" as "Redrawn, soft".

# uaRO Graphics 3.0.18-beta.182 — what's new · 2026-11-12

- Storm Gust's sharp snow is now in the mages' new look too, not only in the game's. Its snowflakes there are the same clean ellipses in their places instead of white pixel blots, the new look's colours and manner kept. The box Effects → Skills → "Storm Gust sharp, as the game's" picks between the two looks: on - the game's, sharp; off - the mages' new look (when it is on). The log tells in which look the snow was first drawn.

# uaRO Graphics 3.0.18-beta.181 — what's new · 2026-11-12

- Storm Gust as the game's, only sharp. The game draws its snowflakes from two small 64×64 sheets stretched 6–12 times on a 4K screen, so they were blurred blots. Each snowflake is now drawn anew at the screen's resolution: a sharp ellipse in the same place, of the same size, tilt, colour and brightness. Its cloud, whirl, lightning, ghost and ice stay the game's (or the AI pack's), the colours and light the game's; the mages' new look for Storm Gust steps aside. The switch: Effects → Skills → "Storm Gust sharp, as the game's" (off). The log says "storm gust: the first snow drawn from its copy".

# uaRO Graphics 3.0.18-beta.180 — what's new · 2026-11-12

- The map loading bar is in the new look. It was the last old window of the game seen at every map change (in 57 of 70 reports): a yellow frame, a grey track and the per cent in the game's small font. Now a sharp bar in the interface's style stands at the same place under the loading picture, with the same per cent the game counts. The switch: Interface → "Windows in the new look" → "Map loading bar" (on with the new interface).

# uaRO Graphics 3.0.18-beta.179 — what's new · 2026-11-12

- The NPC menu colours its options with the server's colours as the game's window does, instead of showing codes like "^FF3355" in the text (R-P7RFV2, Reset Girl: "^FF3355Reset Skills", "Reset Both^000000"). A colour begun in one option goes on in the next ones until another, as the game's list draws it.
- A monster the game left standing 2 cells off its place on the server is put back on it (R-WKEDPY: a Minorous stood elsewhere for 8 s). With its target next to it, the server sends a "walk" from a cell to the same cell; the game makes no such walk, so it is now given as a stop on that cell. The log says "its walk of no length, a stop on".

# uaRO Graphics 3.0.18-beta.178 — what's new · 2026-11-11

- Damage numbers, Miss, Guard, Critical!!, Lucky and the crits' stars are sharp now. They are the game's own pictures (`숫자.spr`, `msg.spr`, `bluemsg.spr`), made four times larger by xBR (as the windows' buttons and labels) instead of stretched and blurred. The game places, colours, scales and moves them as before. The switch: Graphics → Quality → "Crisp damage numbers" (on); Pause shows the old look. The log tells when a number is first drawn from its copy, and every 30 s how many times.

# uaRO Graphics 3.0.18-beta.177 — what's new · 2026-11-11

- A text's width is measured through Windows only once for each label at its size: then it comes from memory (R-B8SDEK: measuring text took 13.7 % of the game's thread, the chat's layout 0.65 ms a frame). The stock of fonts for measuring is larger (64), and the one unused longest goes first. The log tells every 30 s how many measures and how many fonts were made.

# uaRO Graphics 3.0.18-beta.176 — what's new · 2026-11-11

- By the profiler's measurement (R-XZGR7R) needless work every frame removed:
  - the interface library no longer visits every element of the hidden windows each frame (about 70 of them) - a hidden document is updated and laid out when it shows;
  - the controller asked Windows which window is under the cursor at each mouse poll - now only when the cursor moved or every 100 ms;
  - the game's picture memory is no longer opened and closed twice every frame while the game does not touch it;
  - the magic effects' looks are found by their number at once, not by walking the table at each draw;
  - small frame fields go into the bridge to the renderer without an extra call.

# uaRO Graphics 3.0.18-beta.175 — what's new · 2026-11-11

- «Речі», storage and the cart, while open, no longer give the interface library all their data every frame (about 60 times a second during a used thing's flash, R-6PYQQG): only what changed. The minimap likewise - its changed parts alone, not everything up to 15 times a second.

# uaRO Graphics 3.0.18-beta.174 — what's new · 2026-11-11

- The game no longer crashes when the server says an item is worn at more than three of the ten equipment places, or only at places this client does not know (R-ZGN2US: a crash entering moc_fild02): the game itself wrote such places past its own array and spoiled its memory. The driver now leaves such an item out and tells it in the log.

# uaRO Graphics 3.0.18-beta.173 — what's new · 2026-11-11

- The interface library does less work for nothing (R-AKUGNX): the friends and party window told it of changes every frame even closed - now a closed one only hides; the picked-up thing's toast and the experience numbers gave all their data every frame (up to 85 times a second) - now only what changed.

# uaRO Graphics 3.0.18-beta.172 — what's new · 2026-11-11

- «Налаштування» and the Alt+D console no longer look for the settings file on the disk every frame (it was 0.11 ms a frame even with the window closed, R-2CKVU6): its place is found once a second.
- A measurement: the log shows the interface's parts that change their data the most - each change gives work to the interface library, the dearest part of a frame (0.7-1.2 ms).

# uaRO Graphics 3.0.18-beta.171 — what's new · 2026-11-11

- A measurement: the chat's time a frame split into its steps (the game's lines, the input field, the mouse over it, the layout, the history's rows, the redraw, the mouse's bounds) - in R-UT4N5V the chat no longer grows but stays at 0.7 ms a frame; now the step that takes it shows.

# uaRO Graphics 3.0.18-beta.170 — what's new · 2026-11-11

- The chat no longer gets dearer while hunting: each kill, picked-up thing or experience line made it wrap and measure its whole history again (up to 200 lines) - in three minutes the chat grew from 0.13 to 1.33 ms a frame (R-CD8KCP). Now the lines' rows are kept and only a new or changed line is measured.
- The closed «Налаштування» window no longer builds its rows every frame (it was 0.14 ms a frame).

# uaRO Graphics 3.0.18-beta.169 — what's new · 2026-11-11

- A measurement: every 30 s the log tells what each part of the driver takes a frame (chat, skill bar, HP bars, windows, the interface), the dearest first. In R-45Z47V the driver's own work grew from 2.1 to 4.7 ms a frame in five minutes of hunting - now the part that grows shows. The game memory reads are told by module again.

# uaRO Graphics 3.0.18-beta.168 — what's new · 2026-11-11

- The characters' and monsters' index (.166) moves on at the start of every frame, even with the game minimised: otherwise a monster that came while the window was minimised was not found until it was restored.

# uaRO Graphics 3.0.18-beta.167 — what's new · 2026-11-11

- The driver's work with the game's memory is in one place: shared functions instead of 60 copies of the read and 43 of the write (each write a call into Windows), the writes into the game's data without a call into Windows too. Likewise one shared function instead of 55 copies of the interface's texts, 49 copies of a window's place over the game and 37 copies of the mouse buttons' check. About 540 lines of code fewer; the same look and behaviour.

# uaRO Graphics 3.0.18-beta.166 — what's new · 2026-11-11

- Characters and monsters are found by their numbers through an index read in one pass a frame (before, each look-up walked the game's whole list: the monsters' move smoothing made up to ~390 reads a frame, R-C8GVNT). The move smoothing also writes a monster's place without a call into Windows.

# uaRO Graphics 3.0.18-beta.165 — what's new · 2026-11-11

- Reading the game's memory without a call into Windows: the processor copies by itself, and an address that is gone is caught by the driver's handler (before, each of ~1000 reads a frame was its own call into Windows - 0.8-1.3 ms a frame by R-MBCDBU). The controller's, the crisp text's and the textures' memory checks are faster the same way. At start the driver checks the new way itself and goes back to the old one if needed (the «game memory: …» line in the log).

# uaRO Graphics 3.0.18-beta.164 — what's new · 2026-11-11

- Entering the game after a disconnect (R-CFNPF6): while the server still holds the last entry and the character's selection waits, the «Вхід у гру…» card no longer goes away after 30 s and shows the time left itself: «The server still holds the last entry · 2:27». Before, only a small line at the top of the screen told it, and the wait looked like endless loading.

# uaRO Graphics 3.0.18-beta.163 — what's new · 2026-11-11

- The panels that run every frame (the skill bar, chat, party, HP bars, buffs, signs in the world, the controller) are built at full speed again: in .162 the interface was slower (R-MBCDBU: 1.9–2.4 ms a frame against 1.3). Only what opens now and then is built compact: storage, shops, trade, equipment, settings, login, reports, «Довідник». ddraw.dll 17.6 MB (was 20.5), the settings app 4.0 MB.
- In the log the game memory reads are grouped by the driver's functions: it shows who reads the most.

# uaRO Graphics 3.0.18-beta.162 — what's new · 2026-11-11

- A smaller update: ddraw.dll 20.5 → 16.0 MB, the settings app 5.6 → 4.0 MB (about 2.3 MB less to download). The code of the windows, settings and reports is now built for size; drawing, textures, the game's hooks, sound and map loading stay at full speed as before. To check: «Тест графіки» or a usual report - the speed is compared with the previous beta.

# uaRO Graphics 3.0.18-beta.161 — what's new · 2026-11-11

- Measurements in the log: the driver's own work per frame, our interface parts apart from the interface library itself, how many times per frame the game's memory is read and what it costs (the controller's too). Needed before speeding up the work with the game; the same look and behaviour.

# uaRO Graphics 3.0.18-beta.160 — what's new · 2026-11-11

- Unused code removed across all programs (game, settings, studio, renderer): about 3,200 lines - over 200 functions no call reached, with their tests. Found by tracing the links of every program; each one built and checked by the tests. ddraw.dll is 72 KB smaller again; the same look and behaviour.

# uaRO Graphics 3.0.18-beta.159 — what's new · 2026-11-11

- Cleanup finished: the rest of the old picture drawing removed - exp bars, the reference's row repaints, the character block's layers and level flare, the login screens (strip, login list, veil, shelf toast), the on-screen keyboard, shop signs over heads, the controller's carry ring, monster and mercenary cards, Start tiles, the low-HP edge, exp over monsters, the layout editor, pickup toasts. Less work every frame; the same look.

# uaRO Graphics 3.0.18-beta.158 — what's new · 2026-11-11

- Cleanup continued: the old picture output (GDI) of 17 panels and windows removed - the character block, buffs, party, target, hotbar, chat, minimap, tracker, exp bars, reference, trade, whispers and others. Nothing changes on screen.

# uaRO Graphics 3.0.18-beta.157 — what's new · 2026-11-11

- The old edition of the new interface removed from the driver: the fallback sheets of 20 windows, the core's old drawing of windows and panels (GDI pictures) - about 7,000 lines fewer. Nothing changes on screen.

# uaRO Graphics 3.0.18-beta.156 — what's new · 2026-11-11

- The "how many" window moving into the bag (from the storage or the cart) counts what the weight lets in: "The weight lets in 142 of 300", an empty field and "All" take that many. Batch moves take no more than fits too.
- Several things picked now also in the cart (taken into the bag) and in the bag: with the storage or the cart open - Ctrl+click / Shift+click / Ctrl+A, "Put the picked (N)" or drag the picked onto the storage or the cart.

# uaRO Graphics 3.0.18-beta.155 — what's new · 2026-11-11

- Storage: several things at once - Ctrl+click picks a thing (a gold edge), Shift+click a run, Ctrl+A all on the tab, Esc lets go. Take the picked into the bag by the footer's "Take the picked (N)", a double click on a picked one, or dragging the picked out of the storage. Stacks go whole (the "how many" window confirms itself).

# uaRO Graphics 3.0.18-beta.154 — what's new · 2026-11-11

- Equipment: sets "1 2 3", as the later clients' Equip Switch - a right click on a number keeps what is worn, a click puts the set on (things found in the bag by number, refine and cards). Each character has its own sets.

# uaRO Graphics 3.0.18-beta.153 — what's new · 2026-11-11

- Server select: beside the name - how many are playing now ("uaRO · 512 playing"), as the login server tells.

# uaRO Graphics 3.0.18-beta.152 — what's new · 2026-11-11

- Almost every window can now be pulled by its bottom right corner: NPC talk, vending, shop opening, my shop, item choice, friends/party, chat rooms, pet, homunculus, mercenary, settings, macros, whisper settings, the command list - 70-150 %, each keeps its own size.

# uaRO Graphics 3.0.18-beta.151 — what's new · 2026-11-11

- Window size by the mouse: Items, Equipment, Stats, Skills, storage, cart, trade, the NPC shop and the guild - take the bottom right corner (a gold corner shows under the pointer) and pull: 70-150 %. Each window keeps its own size (uaro-graphics\window-sizes.txt).

# uaRO Graphics 3.0.18-beta.150 — what's new · 2026-11-11

- Buffs the server keeps on by itself no longer show a timer or warn that they are ending: Maximize Power and Cloaking (while SP lasts), Chase Walk, Relax, Frenzy, Play Dead, Running, Lex Aeterna, Mystical Amplification, Counter Instinct, Signum Crucis, taekwon stances. Their skill database numbers were not durations (Maximize Power's 1-5 s instead of "until switched off").

# uaRO Graphics 3.0.18-beta.149 — what's new · 2026-11-11

- Wind is now lightning yellow; holy near white so they are not mixed up.

# uaRO Graphics 3.0.18-beta.148 — what's new · 2026-11-11

- Equipment: a worn thing's element now as in the later clients - its name itself in the element's colour (no separate "Wind" mark).

# uaRO Graphics 3.0.18-beta.147 — what's new · 2026-11-11

- Fixed the game freezing on entering a map in .145-.146 (R-STGQE9): our interface waited for the game to finish loading the map's models while the game waited for it. Now it never waits - with the game's files busy it reads the archives with its own reader. The .145 crash fix stays.
- Chat room: actions right under the chosen person; right click chooses; twice on a person or "Whisper" - a whisper in the chat.

# uaRO Graphics 3.0.18-beta.146 — what's new · 2026-11-11

- Equipment: a worn thing with an element shows it in colour beside its name - "Wind", "Fire", "Holy"… (element weapons, forged ones, armour with an element card).
- Item card: an element thing's first line is "Property: Wind", as in the later clients.

# uaRO Graphics 3.0.18-beta.145 — what's new · 2026-11-11

- Fixed the game crashing right after entering a map (an error in cps.dll, then a hang): our interface read the game's files at the same time as its map model loader thread. Now it reads under the same lock as the game.
- Slots in names, as in the old client: "Jur [3]" in the bag, storage, cart and trade (shops since .142).
- Tracking: with nothing pinned, its icon shows for 8 s what to pin and how.
- A crash report now keeps the crash's own dump and logs the faulting address.

# uaRO Graphics 3.0.18-beta.144 — what's new · 2026-11-11

- Element things in their colour: weapons with an element (Fire Brand, Ice Falchion…), forged ones with an element, armour with an element card (Pasana, Swordfish…) - a coloured line in the bag/storage/cart cell, the name in the tip and in the shop and item choice lists in the element's colour. Switch: Interface → "Element things in their colour".

# uaRO Graphics 3.0.18-beta.143 — what's new · 2026-11-11

- My shop: each thing shows "sold N", its slots and what is inside, right click - its card; "Earned" and the Sales list (time · how many × what · +zeny, newest first). The list starts anew each time the shop opens. The server does not name the buyer, so neither do we.

# uaRO Graphics 3.0.18-beta.142 — what's new · 2026-11-11

- Forged things: named as the game does - "Very Strong Fire Main Gauche" (star crumbs and element) in the bag, storage, cart, trade, vending; under the name and in the card - "Forged by Name" (the game asks the server for the smith's name).
- Opening a shop: things show their slots "[3]" and what is inside (cards, free slots); right click on a thing - its card; under the list "If all sells: N z".
- Choosing a thing (a card to insert, the magnifier): slots and what is inside under the name, right click (Y on the controller) - the thing's card.

# uaRO Graphics 3.0.18-beta.141 — what's new · 2026-11-11

- Friends / Party renewed: each person with a face, class and level, an HP bar, "near · N cells" or the map's name, "dead".
- Right click on a person - a menu under the row: whisper, expel (the leader), leave the party (one's own row), add to friends, remove from friends, a route to them, copy the name. On hover - quick whisper / expel buttons.
- Twice on a person - whisper. Friends online first, "N / M online" in the header; the party's also shows its name, the experience share and a Settings button.
- Friends: a player's name field + Add - a friend request by name. Empty tabs say what to do.
- Controller: the D-pad walks people, A opens the menu (left/right its items, A does one), B closes the menu, LB/RB the tabs.

# uaRO Graphics 3.0.18-beta.140 — what's new · 2026-11-11

- NPC talk: the answers are two a row again, as meant (each stood on its own line over half the window).
- Friends / Party and the pet's card: the buttons two a row, not a column on the right; the party's list no longer keeps empty room when the party is small. The same for the list in the item choice window.

# uaRO Graphics 3.0.18-beta.139 — what's new · 2026-11-11

- Item card: a card in a slot (left or right mouse button) opens in its own window beside it, the item's card stays. The card's window moves by its title and closes by its ✕, Esc or B on the controller.

# uaRO Graphics 3.0.18-beta.138 — what's new · 2026-11-11

- Another player's shop: press a thing's name to open its item card (description, slots, the cards in it with their art). On the controller - Y; buying is now Start.
- A thing's line shows its slots "[3]" and what is inside: the cards' names and how many slots are free.

# uaRO Graphics 3.0.18-beta.137 — what's new · 2026-11-11

- Cart (Alt+W): the cards in a thing show in its name, as in the bag, storage, trade and vending (Lucky …, Double …, … of Counter). The same in the item pick window.

# uaRO Graphics 3.0.18-beta.136 — what's new · 2026-11-11

- Hotbar profiles from the controller: the Start menu's "Bar profile N" tile switches 1 → 2 → 3 (as Ctrl+Shift+1..3, with the character's own bar on).
- The controller's button badges in windows (shop, skills, equipment, hints) have their letters again: A, B, LB, RT, Start and so on.
- Shop: when Start bought nothing, the log says why.

# uaRO Graphics 3.0.18-beta.135 — what's new · 2026-11-11

- Our panels from the controller: in the Start menu an open panel's tile (Hunt, Boss timers, Tracking, What killed me) gives its buttons - the D-pad or the left stick walks them (a gold ring), A presses, B leaves. The ✕ is among them.

# uaRO Graphics 3.0.18-beta.134 — what's new · 2026-11-11

- Controller in the chat (on-screen keyboard): RT the channel (all, party, guild), R3 reply to the last whisper, the right stick scrolls the history.

# uaRO Graphics 3.0.18-beta.133 — what's new · 2026-11-11

- Controller on the big map: the left stick moves a cross, X the next NPC / warp / Kafra (named beside it), A plots the way there, Y clears it, B closes.

# uaRO Graphics 3.0.18-beta.132 — what's new · 2026-11-11

- Controller: "Skill wheel 1" and "2" - bind them in the controller menu to any button: hold it, pick one of eight cells of the skill bar's second (third) row with the right stick, let go - the skill. "Settings": choosing "Interface" -> "New screen, game windows" no longer turns the settings window itself off.

# uaRO Graphics 3.0.18-beta.131 — what's new · 2026-11-11

- Controller: the attack button with no target picks up loot beside you (within 2 cells), and with no monsters around talks to the NPC near.

# uaRO Graphics 3.0.18-beta.130 — what's new · 2026-11-11

- Controller: the highlight and tips in windows (Stats, Skills, Items, shops and others) now follow the controller's choice, not the hidden mouse cursor. On the skill bar the LB, RB, LT, RT badges are no longer cut.

# uaRO Graphics 3.0.18-beta.129 — what's new · 2026-11-11

- "Equipment": the character now holds its weapon and shield (as in the game's world).

# uaRO Graphics 3.0.18-beta.128 — what's new · 2026-11-11

- The character block folded: a "…" button on its right opens the row of uaRO's windows (hunt, bosses, tracking and so on).

# uaRO Graphics 3.0.18-beta.127 — what's new · 2026-11-11

- The target card: a log warning on a target change removed.

# uaRO Graphics 3.0.18-beta.126 — what's new · 2026-11-11

- "Equipment": the character in its clothes' dye and with the head and body layers in the game's order (a priestess's veil over her hair), as the game draws it.

# uaRO Graphics 3.0.18-beta.125 — what's new · 2026-11-11

- "Settings" -> "Keys": the keys of "Hunt", "Boss timers", "Tracking", "Party", "What killed me" can be changed (Ctrl and a free letter) or removed.

# uaRO Graphics 3.0.18-beta.124 — what's new · 2026-11-11

- Controller: the Start tile now has "Hunt", "Boss timers", "Tracking", "What killed me"; the "Settings" tile opens the new window, walked by its controls.
- "Settings": - and + buttons by every slider.

# uaRO Graphics 3.0.18-beta.123 — what's new · 2026-11-11

- "Party": ✕ is now at the right of its header (it stood over the title).

# uaRO Graphics 3.0.18-beta.122 — what's new · 2026-11-11

- A new "Settings" window (Alt+O, the character block's icon, the menu) in place of the game's options: sections on the left - "Game" (music, sounds, volumes, snapping, opaque windows - all it had), "How the game looks" and every interface section; a search on top. Everything changes at once.

# uaRO Graphics 3.0.18-beta.121 — what's new · 2026-11-11

- The menu (Esc) has new items at the top: "Settings", "Arrange the interface", "Graphics test", "Send a report"; the game's buttons below, as before.

# uaRO Graphics 3.0.18-beta.120 — what's new · 2026-11-11

- The character block (Alt+V) has a second row of icons: "Reference", "Hunt", "Boss timers", "Tracking", "Party", "What killed me", "Settings"; an open window's icon glows gold. Keys: Ctrl+H, Ctrl+B, Ctrl+J, Ctrl+G, Ctrl+K.
- ✕ on "Hunt", "Boss timers", "Party" and the target card: hides the window without switching it off; bring it back with its icon or key. The game remembers hidden windows.
- "What killed me" is a window of its own with the last death's hits (Ctrl+K).

# uaRO Graphics 3.0.18-beta.119 — what's new · 2026-11-11

- "Lock the parts' places" removed: the interface's parts can always be dragged now (hold 0.3 s and move). Anyone who had the lock on has it off now.

# uaRO Graphics 3.0.18-beta.118 — what's new · 2026-11-11

- Cards now show in item names in another's shop and your own, the shop setup, trade, storage and the bag (e.g. "Double Lucky Katana of Counter"), as in "Equipment".

# uaRO Graphics 3.0.18-beta.117 — what's new · 2026-11-11

- "Equipment": the cart / falcon / Peco sign asks first ("Get off the Peco?" - "Yes" / "No"); a stray click takes nothing off.

# uaRO Graphics 3.0.18-beta.116 — what's new · 2026-11-11

- "Equipment": the button for the cart, the falcon and the Peco is now a round sign between the ◀ ▶ arrows; what it does shows in a tip under the cursor.

# uaRO Graphics 3.0.18-beta.115 — what's new · 2026-11-11

- "Equipment": the character fills the middle again (it stayed small at the top); the ◀ ▶ arrows light up under the cursor.

# uaRO Graphics 3.0.18-beta.114 — what's new · 2026-11-11

- "Equipment": the character is now made of the texture pack's improved frames (4x), no more "squares"; a button over the character, as in the game: "Remove the cart", "Release the falcon" or "Get off the Peco" (with your own shop open the game asks to close it first).

# uaRO Graphics 3.0.18-beta.113 — what's new · 2026-11-11

- Equipment: the character is big and centred again (after the window shrank it stayed small at the top); long item names are not cut - they go onto a second line and the slot grows; "Clip of of Counter" is now "Clip of Counter".

# uaRO Graphics 3.0.18-beta.112 — what's new · 2026-11-11

- Things with cards are named as in the game: the card's word before the name ("Saharic Jur", "Double Bloody Katana"), some cards after it ("Clip of Counter") - in the equipment window and the item card.
- Equipment: the ◀ ▶ arrows now turn the character; the window is smaller (640 instead of 880).

# uaRO Graphics 3.0.18-beta.111 — what's new · 2026-11-10

- "Спорядження" (equipment): the middle now shows the real character from the game's sprites - the class body (on the Peco when riding), the head in its hair colour, three headgears; the hunter's falcon on the shoulder and the merchant's cart behind. The ◀ ▶ buttons turn it all eight ways. The weapon and shield in hand are not drawn yet.

# uaRO Graphics 3.0.18-beta.110 — what's new · 2026-11-10

- "Спорядження" (equipment) like the game's original window: left the top and low head, right hand, robe, right accessory; right the middle head, body, left hand, shoes, left accessory; the ammo in the strip below. The place over each item name, the slot dots after it (gold with a card). The character numbers left the window (they are in "Характеристики"); the middle shows the portrait for now - the character from sprites is the next step.
- A card opened from a slot in the item card shows its big illustration over its description, like the original card window.

# uaRO Graphics 3.0.18-beta.109 — what's new · 2026-11-10

- The item card closer to the original: the big item picture on the left, the description on the right, the slots below - a card's picture and name, two a row. A click on a card opens its description in the same card; a click on the title ("‹ item name") goes back. The card texts inside the item text (from beta.108) are gone.

# uaRO Graphics 3.0.18-beta.108 — what's new · 2026-11-10

- The item card: under the item's description, each inserted card's name in gold and its own description from the game's table (as it is, for any card). The card shows up to 20 rows; a longer one scrolls with the wheel.

# uaRO Graphics 3.0.18-beta.107 — what's new · 2026-11-10

- The login card (login, password, ▾ of saved logins, the eye, ticks, "Увійти" / "Вийти", links, the site line) is now laid out by the new interface library. The login and password fields are matched by name; the presses are the same.

# uaRO Graphics 3.0.18-beta.106 — what's new · 2026-11-10

- The login screens in the new interface library: the server select, the character make (face, hair and colour, name, stat pairs) and the character select (buttons below; the cards and carousel keep their places).

# uaRO Graphics 3.0.18-beta.105 — what's new · 2026-11-10

- Opening a shop: Tab moves to the next field (name, then the prices, round; Shift+Tab back), Enter to the next thing's price.
- Chat: a magnifier in the tabs row - a click opens the search through the lines (type the words; Esc or another click closes it).

# uaRO Graphics 3.0.18-beta.104 — what's new · 2026-11-10

- Fixed: the stats and guild windows stretched over the whole screen.
- Opening a shop: the price field is wide and easy to see, "For sale" shows the places used of all, and each thing shows the price for the whole stack and the site market price.

# uaRO Graphics 3.0.18-beta.103 — what's new · 2026-11-10

- Opening your own shop (choosing things and prices) now uses the new look: the "vend_setup" switch was not part of the "Новий інтерфейс" choice, so the game's old window opened. It now follows "Своя лавка" (my shop).

# uaRO Graphics 3.0.18-beta.102 — what's new · 2026-11-10

- The guild card is now laid out by the new interface library: its top and six tabs (two rows of three); the tabs' content keeps its places, and the "+", "Змінити", "Скасувати" buttons and the notice fields became library elements.
- Fixed (since 3.0.18-beta.92): the guild card could not be dragged - it now moves by its empty part.

# uaRO Graphics 3.0.18-beta.101 — what's new · 2026-11-10

- "Характеристики" (stats, Alt+A): the top of the window (title, tabs, free points, ✕) is now laid out by the new interface library; the tables, formulas and planning keep their places, and their buttons became library elements.

# uaRO Graphics 3.0.18-beta.100 — what's new · 2026-11-10

- Fixed (since 3.0.18-beta.94): the items, storage, cart, trade, equipment, skills, homunculus, mercenary and NPC shop windows follow the text size and item window size settings again.

# uaRO Graphics 3.0.18-beta.99 — what's new · 2026-11-10

- The NPC shop is now laid out by the new interface library: the wares, the cart with − amount + and ×, the totals, the buttons and the keys below. Presses land where they are seen.
- Fixed (since 3.0.18-beta.94): the search hint in the items, storage and cart windows sat below its field.

# uaRO Graphics 3.0.18-beta.98 — what's new · 2026-11-10

- The homunculus and mercenary cards are now laid out by the new interface library: bars, stats, "Нагодувати" (feed), skills with the level arrows and "+", "Видалити" / "Звільнити". Presses land where the button is seen.

# uaRO Graphics 3.0.18-beta.97 — what's new · 2026-11-10

- "Навички" (skills) is now laid out by the new interface library: the top buttons, tabs, list rows, the side panel and the footer; the tree stays a diagram in its places. Presses on "+", the arrows and dragging skills land where you see them.
- Fixed (since 3.0.18-beta.94): the footer of the items, storage and cart windows was spread across the whole width and the gold ring of a used thing did not show; the equipment slot names had extra margins.

# uaRO Graphics 3.0.18-beta.96 — what's new · 2026-11-10

- "Спорядження" (equipment) is now laid out by the new interface library: five slots each side, the portrait and numbers (or the controller's list) in the middle, the ammo below. Clicks and dragging things land in the slot you see.

# uaRO Graphics 3.0.18-beta.95 — what's new · 2026-11-10

- "Обмін" (trade) is now laid out by the new interface library: two sides of five cells a row, the zeny row, the warning and the buttons. Clicks and dragging things land where the cell is seen.

# uaRO Graphics 3.0.18-beta.94 — what's new · 2026-11-10

- "Речі" (items), "Склад" (storage) and "Візок" (cart) are now laid out by the new interface library: the tabs, the search, the cells eight a row and the footer. Clicks, dragging things into other windows, the tip and the controller take the cells' places from that layout, so they land where the cell is seen.

# uaRO Graphics 3.0.18-beta.93 — what's new · 2026-11-10

- The item description card and the skill card can now be dragged: press on the title or the text and drag. A new card opens beside the cursor again.

# uaRO Graphics 3.0.18-beta.92 — what's new · 2026-11-10

- Every window of the new interface can now be dragged: press its empty part and pull. Added for all live windows (messages, requests, "Опції", the pet, macros, chat rooms, shops, the NPC talk and others), the guild, the homunculus, the mercenary and each whisper card.

# uaRO Graphics 3.0.18-beta.91 — what's new · 2026-11-10

- Fixed: when dragging an item, a skill or a skill bar cell inside a window ("Речі", "Склад", "Візок", "Спорядження", "Навички"), its icon now follows the cursor there too - the window itself hid it before, it showed only outside it.

# uaRO Graphics 3.0.18-beta.90 — what's new · 2026-11-10

- New interface (uaUI): one's own open shop and its setup are live windows of the new library: the cart and the wares for sale side by side, the sign and prices the game's fields, a click on a cart item puts it up for sale, ✕ takes it down.

# uaRO Graphics 3.0.18-beta.89 — what's new · 2026-11-10

- New interface (uaUI): another player's shop is a live window of the new library: wares with − / +, the sum and your zeny, "Купити" lit only when you can afford it.

# uaRO Graphics 3.0.18-beta.88 — what's new · 2026-11-10

- New interface (uaUI): the NPC talk is a live window of the new library: the NPC's coloured words, the answers in two columns, the "Далі" / "Закрити" button; the text wraps by itself, the window drags by its ground.

# uaRO Graphics 3.0.18-beta.87 — what's new · 2026-11-10

- New interface (uaUI): the whisper setup, macros, making a chat room, the chat room, friends / party and the pick list are now live windows of the new library - laid out by flexbox, their text wraps by itself, the library's own hover and clicks; the gamepad walks them as before.

# uaRO Graphics 3.0.18-beta.86 — what's new · 2026-11-10

- New interface (uaUI): the last style warning in the log removed (an empty ground colour as the shelf appeared).

# uaRO Graphics 3.0.18-beta.85 — what's new · 2026-11-10

- Internal: the old code painting windows as pictures removed (40 painters and their helpers) - nothing looks different, the driver is smaller.

# uaRO Graphics 3.0.18-beta.84 — what's new · 2026-11-10

- Chat: the chat line takes Ctrl+V (paste one line at the end, as much as the field holds), Ctrl+C (copy the line) and Ctrl+X (cut it). The field itself stays the game's - sending, typing in other languages and the battle mode as they were.

# uaRO Graphics 3.0.18-beta.83 — what's new · 2026-11-10

- "Довідник": the world map is no longer repainted on every mouse move over its rows - only when something on it changed (less processor work with "Довідник" open).

# uaRO Graphics 3.0.18-beta.82 — what's new · 2026-11-10

- New interface (uaUI): the game's "Опції" is a live window of the new library: the music and sound volumes are its sliders, the switches ticks.

# uaRO Graphics 3.0.18-beta.81 — what's new · 2026-11-10

- New interface (uaUI): the "Порада дня" tip, the requests (trade, party, guild, friend, a number or text to type, a count) and the pet window are now live windows of the new library - laid out by flexbox, their text wraps by itself, the library's own hover and clicks.

# uaRO Graphics 3.0.18-beta.80 — what's new · 2026-11-10

- Gamepad: with the character dead and the death window closed, the hint line at the bottom says "B: back to the save point".
- New interface (uaUI): the log no longer fills with "Could not get value from data variable words[…]" warnings.

# uaRO Graphics 3.0.18-beta.79 — what's new · 2026-11-10

- Gamepad: B now closes an NPC talk just as Esc does - on a "Далі" page, in a window with no buttons (it needed holding for a second) and in a number field. The hint line shows it.
- Gamepad: with the character dead and the death window closed, B opens it again - to go back to the save point.
- New interface (uaUI): the party panel re-evaluates only the bars while the HP bars glide, not every row.

# uaRO Graphics 3.0.18-beta.78 — what's new · 2026-11-10

- Fixed (since 3.0.18-beta.75): on the login screens the game's "Please wait…", "Failed to Connect to Server." and other login errors were not shown - only the background stayed. The developer's answer and the gamepad hint show before entering the game again too.

# uaRO Graphics 3.0.18-beta.77 — what's new · 2026-11-10

- New interface (uaUI): the red edges at low HP are lighter still - the clear middle of the screen is no longer cut into small triangles. The FPS drop when they appeared (R-6XYRXP) is gone together with 3.0.18-beta.76.

# uaRO Graphics 3.0.18-beta.76 — what's new · 2026-11-10

- New interface (uaUI): the red edges at low HP no longer slow things down at 4K (over 100 thousand vertices a frame, now a few thousand). Style errors in the log at start removed.

# uaRO Graphics 3.0.18-beta.75 — what's new · 2026-11-10

- The new interface is now drawn by the new library (uaUI) alone: the "new interface library" switch under "Інтерфейс" is gone, the old way of painting windows and panels as pictures has been removed.

# uaRO Graphics 3.0.18-beta.74 — what's new · 2026-11-10

- New interface (uaUI): the game's message box (OK, Yes/No, "Please wait…") is now a live window of the new library - its text wraps by itself, the buttons stand in a row under it, the window centres by its own size.

# uaRO Graphics 3.0.18-beta.73 — what's new · 2026-11-10

- New interface (uaUI): the "Емоції" and "Список команд" windows and the player menu (right click) are now live windows of the new library - it lays them out itself (flexbox), its own hover and clicks. Emotes: a click puts it into the chat, a double click sends it, drag by the window's ground. The gamepad walks them as before.

# uaRO Graphics 3.0.18-beta.72 — what's new · 2026-11-10

- The searches in "Речі", "Склад", "Візок", the shop and "Довідник" now take any language (only Latin letters before), Backspace removes a whole letter, Ctrl+A, Ctrl+C, Ctrl+X and Ctrl+V work. While a field types, no key reaches the game.

# uaRO Graphics 3.0.18-beta.71 — what's new · 2026-11-10

- New interface (uaUI): the theme, the palette and parts' markup can now reach every player through the updates: data\uaro-graphics\ui\theme.rcss, palette.txt and <part>.rml in a GRF in the client's updates folder. The player's own files in uaro-graphics\ui stay on top.

# uaRO Graphics 3.0.18-beta.70 — what's new · 2026-11-10

- New interface (uaUI): faster - a window re-evaluates only the parts of it that changed, and still elements are not rebuilt every frame. The log line "uaui: in N s …" now also tells how many elements were rebuilt a frame.

# uaRO Graphics 3.0.18-beta.69 — what's new · 2026-11-10

- New interface (uaUI): the last parts still drawn as pictures are now drawn by the new library - monsters' HP bars, gold dots over NPCs, the red edges at low HP, the "Вхід у гру…" veil, the shelf's "… додано" note, the interface cursor. The gamepad's window tile, "Призначити", the chat wheel, the on-screen keyboard and the "Розміщення інтерфейсу" frames are over the game's own windows again.

# uaRO Graphics 3.0.18-beta.68 — what's new · 2026-11-10

- The new interface library (uaUI) is now on by default: it draws every part of the new interface. It can be turned off under "Інтерфейс".

# uaRO Graphics 3.0.18-beta.67 — what's new · 2026-11-10

- The world no longer dims under windows: the switch "Приглушувати світ під вікнами" is gone, the world behind "Речі", "Характеристики" and "Довідник" stays as it is.

# uaRO Graphics 3.0.18-beta.66 — what's new · 2026-11-10

- Settings: "Приглушувати світ під вікнами" is now off by default in the app too (the game has had it off since 3.0.11-beta.55); "Типово" no longer turns it on.

# uaRO Graphics 3.0.18-beta.65 — what's new · 2026-11-10

- New interface (uaUI): "Скляні вікна" with DLSS - windows were a flat grey instead of the blurred world; the glass now works with DLSS too. Small style errors in the log at start removed.

# uaRO Graphics 3.0.18-beta.64 — what's new · 2026-11-10

- New interface (uaUI): the switch "Скляні вікна (нова бібліотека)" under "Інтерфейс" (off by default). With it windows of the new library show the world behind them softly blurred, like frosted glass, with the window's colour laid over. Direct3D 12 only (with DLSS too); on Direct3D 11 the switch changes nothing.

# uaRO Graphics 3.0.18-beta.63 — what's new · 2026-11-10

- New interface (uaUI): with "Плавний рух (нова бібліотека)" the HP bars in the party panel glide to their new value too.
- New interface (uaUI): the gamepad in the whisper window - the D-pad moves the frame over the input field, "Надіслати" and ×, A presses, B closes (the top whisper window).
- New interface (uaUI): with "Ефекти (нова бібліотека)" radial gradients are drawn too (for your own themes and markup).

# uaRO Graphics 3.0.18-beta.62 — what's new · 2026-11-10

- New interface (uaUI): the target card no longer logs a style error (an empty element frame colour) the moment it appears.

# uaRO Graphics 3.0.18-beta.61 — what's new · 2026-11-10

- New interface (uaUI): the switch "Ефекти (нова бібліотека)" under "Інтерфейс" (off by default). With it windows and panels cast soft shadows, the button chosen by the gamepad and the ready or lit skill bar cells glow, the HP, SP and experience bars and the buttons get a light sheen on top, and round corners inside windows are cut cleanly.

# uaRO Graphics 3.0.18-beta.60 — what's new · 2026-11-10

- New interface (uaUI): the gamepad in windows that did not hear it before - emotes, commands, making a chat room, the chat room, the popup list, macros. The D-pad (or the left stick) moves a gold frame over the buttons and cells, A presses the chosen one just as the mouse does, B closes the window.

# uaRO Graphics 3.0.18-beta.59 — what's new · 2026-11-10

- New interface (uaUI): your own look for any part. The game puts every part's built-in markup into uaro-graphics\ui\builtin\ (with a README.txt). Copy a part's file to uaro-graphics\ui\ and edit the copy - changes show within a second, no restart; delete the copy to get the built-in one back.
- New interface (uaUI): ready text fields, sliders, lists and switches in the game's look; in a field of the new library the keyboard types into the field. See "Вітрина компонентів (для розробників)", tab "Форма".

# uaRO Graphics 3.0.18-beta.58 — what's new · 2026-11-10

- New interface (uaUI): the switch "Плавний рух (нова бібліотека)" under "Інтерфейс" (off by default). With it windows slide in softly, cells and buttons light up smoothly, skill bar cells grow a little under the mouse and press in, the HP and SP bars glide to their new value (in the character block and on the target card), the low HP bar pulses, a new item and experience float in.

# uaRO Graphics 3.0.18-beta.57 — what's new · 2026-11-10

- New interface (uaUI): labels are now measured by the same engine that draws them - plates, buttons and tags fit the letters exactly. A line that still does not fit its field shrinks a little (by a quarter at most) instead of spilling out.

# uaRO Graphics 3.0.18-beta.56 — what's new · 2026-11-10

- New interface (uaUI): on the skill bar and the effects column the letters now grow with the part's size in "Розміщення інтерфейсу" - the arrow count and other labels no longer spill out of their plates.

# uaRO Graphics 3.0.18-beta.55 — what's new

- **Fixed: "Полиця"'s search typed Cyrillic as garbage** ("ïòðîðîìîòï" for "пророк") — letters are read in the game's code page now. The same is fixed in the site sign-in fields and the shelf card's name.
- **Fixed with "Нова бібліотека інтерфейсу (RmlUi)": the character block's tip in the icon view** — its dark plate now spans the whole text.

# uaRO Graphics 3.0.18-beta.54 — what's new

- **Themes work in "Нова бібліотека інтерфейсу (RmlUi)":** the interface theme (standard; classic — warm brown and cream tones, less round corners; contrast — darker grounds, brighter words, thicker edges) and your own `uaro-graphics\ui\palette.txt` now change all of the new library's windows and panels. It switches on the fly.

# uaRO Graphics 3.0.18-beta.53 — what's new

- **With "Нова бібліотека інтерфейсу (RmlUi)": the target card's frame in "Розміщення інтерфейсу" and a click on its edge fit the card exactly.**
- **The target's HP per cent and "⚔ N" over the character now use the new library** (over the world, under the game's windows, as before).
- The log: the new library's repeated warnings are written once.

# uaRO Graphics 3.0.18-beta.52 — what's new

- **Fixed with "Нова бібліотека інтерфейсу (RmlUi)": the icons on the minimap's buttons now sit in the buttons' middles.** The buttons' edge was drawn outside them, shifting the button against its icon — the edge is inside again, as it was. The same alignment is fixed in the new library's other windows, cells and panels.

# uaRO Graphics 3.0.18-beta.51 — what's new

- **An enlarged minimap: its buttons (+, −, layers) and the marks on the map now grow more than the map itself** — at 160 % they are twice their usual size, not just 1.6 times.

# uaRO Graphics 3.0.18-beta.50 — what's new

- **Fixed with "Нова бібліотека інтерфейсу (RmlUi)": the party's and the character block's frame in "Розміщення інтерфейсу" now fits them exactly**, and a click on their edge no longer goes to the game world.

# uaRO Graphics 3.0.18-beta.49 — what's new

- **Fixed with "Нова бібліотека інтерфейсу (RmlUi)": in "Розміщення інтерфейсу" the gold frame of the party, the character block and the target card now fits the part itself** — it was narrower before, taken from the old size.

# uaRO Graphics 3.0.18-beta.48 — what's new

- **Fixed with "Нова бібліотека інтерфейсу (RmlUi)": letters went missing in "Довідник" and other windows** on big screens (4K) — the letters' store ran out of room. It is twice as big now, and when it fills up the letters are laid out again.

# uaRO Graphics 3.0.18-beta.47 — what's new

- **Fixed with "Нова бібліотека інтерфейсу (RmlUi)": the chat, the windows ("Речі", "Довідник" and others), the experience strip, the experience labels and the hotbar are drawn again.** Their data did not reach the markup, and the hotbar's markup had an error.

# uaRO Graphics 3.0.18-beta.46 — what's new

- **Fixed with "Нова бібліотека інтерфейсу (RmlUi)": the character block, the party and other parts drag and click again** — transparent screen-wide parts (the experience strip, the layout editor) took the mouse.
- **Fixed: icons in the new library's windows** (a markup error kept them away).
- To find why the chat and the hotbar are not seen, the log now records every shown part of the new library — please send one more report.

# uaRO Graphics 3.0.18-beta.45 — what's new

- **The "▾" lists of saved logins on the login screen and in "Продовжити" now use "Нова бібліотека інтерфейсу (RmlUi)"** — over the card, the row under the mouse lit.

# uaRO Graphics 3.0.18-beta.44 — what's new

- **The top one of several new library windows has its gold edge again**, as the other windows do.
- **The login screen's bottom strip (server status, the site's news, language and view) now uses "Нова бібліотека інтерфейсу (RmlUi)".**

# uaRO Graphics 3.0.18-beta.43 — what's new

- **The new library's windows have their shadow again** — as the other windows do.
- **The new library's windows overlap as before:** the last opened or clicked one on top.

# uaRO Graphics 3.0.18-beta.42 — what's new

- **The big map now uses "Нова бібліотека інтерфейсу (RmlUi)":** the card, its header, the cross, the layer buttons and "Схема" are drawn by the new library; the map with its marks stays its own picture. Clicks, the wheel and dragging work as before.

# uaRO Graphics 3.0.18-beta.41 — what's new

- **The accounts "Полиця" and "Персонажі" now use "Нова бібліотека інтерфейсу (RmlUi)":** cards with class portraits gliding in their row, search, arrows, buttons, the site links and the card menu are drawn by the new library.

# uaRO Graphics 3.0.18-beta.40 — what's new

- **The "Продовжити" and site sign-in cards now use "Нова бібліотека інтерфейсу (RmlUi)":** the class portrait, buttons, fields, sign-in ways and ticks are drawn by the new library.
- **More reliable:** a part of the new library that stopped showing (for example the login card after entering the game) now always leaves the screen.

# uaRO Graphics 3.0.18-beta.39 — what's new

- **Carrying items with the controller (the gold frame, the item and the hint) and the mercenary's block under the party panel now use "Нова бібліотека інтерфейсу (RmlUi)".**

# uaRO Graphics 3.0.18-beta.38 — what's new

- **The controller's NPC plate, item names on the ground, shop and chat room signs over players and the cast bar now use "Нова бібліотека інтерфейсу (RmlUi)".** The ones over the world still lie under the game's own windows.

# uaRO Graphics 3.0.18-beta.37 — what's new

- **The game's messages, the party settings and the monster card under the cursor now use "Нова бібліотека інтерфейсу (RmlUi)".** Clicks and keys work as before.

# uaRO Graphics 3.0.18-beta.36 — what's new

- **"Довідник" now uses "Нова бібліотека інтерфейсу (RmlUi)":** the header with search and buttons, the section rail, cards, blocks, tiles, chips, search results with highlighted matches, route steps, the legend, buttons, scrolling and tips are drawn by the new library; the world map stays its own picture. Clicks, the wheel and keys work as before.

# uaRO Graphics 3.0.18-beta.35 — what's new

- **The controller tiles, button assignment, the controller hint line and "Розміщення інтерфейсу" now use "Нова бібліотека інтерфейсу (RmlUi)":** tiles with icons, rows with buttons, the interface parts' frames with labels, the guides while dragging, the resize grip and the bar with "Скинути" and "Готово" are drawn by the new library. Controls work as before.

# uaRO Graphics 3.0.18-beta.34 — what's new

- **The pet, homunculus, mercenary, game "Налаштування", friends and party, guild, macros, tip of the day, menu (and the death screen) windows and the on-screen keyboard now use "Нова бібліотека інтерфейсу (RmlUi)":** bars, switches, volume sliders, lists, buttons and fields are drawn by the new library. Clicks, keys and the controller work as before.

# uaRO Graphics 3.0.18-beta.33 — what's new

- **Login, server select, character select and make now use "Нова бібліотека інтерфейсу (RmlUi)":** the login fields with their errors, ticks, buttons, character cards with HP/SP bars, the character make with stat pairs are drawn by the new library. The new library now also runs before entering the game world (for these windows only). Clicks, keys and the controller work as before.

# uaRO Graphics 3.0.18-beta.32 — what's new

- **The chat, the chat wheel, whisper windows, the chat room and its making, the popup menu, emotions, the command list and the whisper settings now use "Нова бібліотека інтерфейсу (RmlUi)":** tabs with counters, the input with its channel and caret, filters, the name menu, scrolling, the wheel's ring with sectors are drawn by the new library. Keys, clicks, dragging and resizing the chat work as before.

# uaRO Graphics 3.0.18-beta.31 — what's new

- **NPC talk, requests ("Скільки?", invitations), the item choice, the support answer card, item and skill cards now use "Нова бібліотека інтерфейсу (RmlUi)":** the windows, numbered menu options, buttons, fields, lists with icons and scrolling, card slots are drawn by the new library. Clicks, keys and the controller work as before.

# uaRO Graphics 3.0.18-beta.30 — what's new

- **The NPC shop, "Лавка", opening your shop and your own shop now use "Нова бібліотека інтерфейсу (RmlUi)":** tabs, search, the ware list and the cart with − / amount / +, the "Купити" or "Продати" button, keys and controller buttons; the shop rows with "+" and "−", the sums, the price fields are drawn by the new library. Clicks, the wheel, keys and the controller work as before.

# uaRO Graphics 3.0.18-beta.29 — what's new

- **"Спорядження", "Характеристики" and "Навички" now use "Нова бібліотека інтерфейсу (RmlUi)":** gear slots with icons and card dots, the portrait, the controller's pick list; the stats table, "Планування" and "Довідка"; the skills list with levels and "+", "План", and "Дерево" with its lines, nodes and scrolling are drawn by the new library. Dragging, clicks, the wheel and keys work as before.

# uaRO Graphics 3.0.18-beta.28 — what's new

- **"Обмін" (trade) now uses "Нова бібліотека інтерфейсу (RmlUi)":** both sides with their cells, the green "Готово" mark, the partner's changes after your "Готово" in red, the zeny field, the warning, the buttons and the tip are drawn by the new library. Dragging items, clicks and keys work as before.

# uaRO Graphics 3.0.18-beta.27 — what's new

- **"Речі", "Склад" and the cart now use "Нова бібліотека інтерфейсу (RmlUi)":** the window glass, tabs, search, cells with icons, counts, refine and "нове", the scroll, weight, use flashes and the tip are drawn by the new library. Dragging windows and items, clicks, the wheel and keys work as before.
- **The new library's windows are drawn over the panels,** the chat and the hotbar — where the other windows are.
- **Fixed:** with the new library on, everything it draws was drawn twice a frame — semi-transparent parts looked denser than intended.

# uaRO Graphics 3.0.18-beta.26 — what's new

- **The minimap now uses "Нова бібліотека інтерфейсу (RmlUi)":** the frame with gold corners, the +/−/big map buttons, the layer buttons and "Схема", the map name bar, the search and ping plates with the graph, the route bar and the monster list are drawn by the new library; the map with its marks is as before. Clicks, the wheel and Ctrl+Tab work as before. The big map is still the old one. Without this switch nothing changes.

# uaRO Graphics 3.0.18-beta.25 — what's new

- **The hunt, MVP and tracker panels now use "Нова бібліотека інтерфейсу (RmlUi)":** their cards, lines, hover highlights and buttons are drawn by the new library; clicks work as before. Without this switch nothing changes.

# uaRO Graphics 3.0.18-beta.24 — what's new

- **Experience over monsters and the "+N ..." item note now use "Нова бібліотека інтерфейсу (RmlUi)":** the gold labels rising over a killed monster and the pill about an item that came into the bag are drawn by the new library. Without this switch nothing changes.

# uaRO Graphics 3.0.18-beta.23 — what's new

- **The experience bars at the bottom of the screen now use "Нова бібліотека інтерфейсу (RmlUi)":** the Base and Job bars with 10 % ticks, the last gain's label and the hover tip are drawn by the new library. Without this switch nothing changes.

# uaRO Graphics 3.0.18-beta.22 — what's new

- **Windows drag again with the new interface library on:** the new library took the mouse button's release for itself, and dragging other windows broke off.

# uaRO Graphics 3.0.18-beta.21 — what's new

- **The hotbar is on the new library:** with it on, the bar is built anew - cells with all their states (chosen, short of SP or an item, not allowed here, combo, ready), levels and counts, wall and trap timers, the delay after a skill, controller buttons, battle mode, the ammo and supplies pills, tips. Dragging, the level wheel and the keys work as before.

# uaRO Graphics 3.0.18-beta.20 — what's new

- **The buffs column is on the new library:** with it on, the column is built anew - icons, their times outlined in dark, the spent time darkened, names, the "more" count, the names button and the tip; debuffs in a red frame. What fits and where the rest go stays as before.

# uaRO Graphics 3.0.18-beta.19 — what's new

- **The party panel is on the new library:** with it on, the panel is built anew - faces, the leader's crown, class or cast, HP per cent or the map, the HP bar, the cast line, the arrow to the member, buffs with their times, red and gold rows, the compact view, folding. A click on a member does the same as before.

# uaRO Graphics 3.0.18-beta.18 — what's new

- **The target card is on the new library:** with it on, the card is built anew - the monster's picture, name, level, element in its colour, HP bar, the book and the controller buttons; the details open under the cursor and a click opens its card in the in-game reference. The look is the same.

# uaRO Graphics 3.0.18-beta.17 — what's new

- **The character block is the first part rewritten on the new library:** with the new interface library on it is built anew - full, compact, rings and the icon, tips for every part, the low HP warning, the gold edge on a new level, the window buttons. The look and the words are the same; turn the switch off for the old block.
- **Your own theme file `uaro-graphics\ui\theme.rcss` now adds to the built-in theme** instead of replacing it: write only what you want to change.

# uaRO Graphics 3.0.18-beta.16 — what's new

- **Fixed the game closing with the new interface library on:** it could vanish when the in-game reference was opened. The interface parts are drawn as before again; they will move to the new library one by one in the next betas.

# uaRO Graphics 3.0.18-beta.15 — what's new

- **The minimap and the big map are on the new library too:** with it on, their marks, frames and labels are drawn by the graphics card, and the map itself stays the same picture under them.

# uaRO Graphics 3.0.18-beta.14 — what's new

- **More of the new interface on the graphics card:** with the new interface library on, the in-game reference, the answer and info cards, the layout editor, the low HP warning, the experience strip and gain, and the controller hints are drawn this way too.

# uaRO Graphics 3.0.18-beta.13 — what's new

- **Interface themes:** a new choice in the Interface section - standard (as now), classic RO (the old game's warm browns and cream, nearly square corners) and contrast (darker grounds, brighter words, sharper frames). Works with the new interface library. Your own colours go into `uaro-graphics\ui\palette.txt`; the game picks it up within a second.
- **Windows and cards appear smoothly:** they grow from 96 % and fade in over 120 ms (off together with the rest of the interface animation).

# uaRO Graphics 3.0.18-beta.12 — what's new

- **The new interface is drawn by the graphics card:** with the new interface library switch on, the character block, panels, windows and cards are no longer repainted by the processor as a whole picture on every change. Their frames, bars, icons and words are drawn on the card, the words in the same font as now. The look should stay the same; if something looks different, please send a report.

# uaRO Graphics 3.0.18-beta.11 — what's new

- **The new interface library is ready to take parts:** its switch in the Interface section is now the new interface library switch. The parts moved to it are drawn by it, the rest stay as they are; the first moved parts come in the next betas.
- **The components showcase has its own switch** for developers. The test card is gone: the real character block replaces it.

# uaRO Graphics 3.0.18-beta.10 — what's new

- **The wheel over the new library's windows no longer zooms the world:** over the card and the "uaUI · компоненти" window it only scrolls the list and the camera stays. A click on the window no longer goes to the world either.

# uaRO Graphics 3.0.18-beta.9 — what's new

- **The new interface library, stage 2:** with the RmlUi test card switch on, a "uaUI · компоненти" window shows next to the card. It has tabs, the bag's items with their icons (open the items window so they come in), a list that scrolls with the wheel, buttons, switches and a bar. The window drags by its title and hides with its cross.
- **The look is one theme file:** every colour, size and motion of these windows comes from `uaro-graphics\ui\theme.rcss`. Put the file there and the game picks it up within a second, no restart; without it the built-in theme applies.

# uaRO Graphics 3.0.18-beta.8 — what's new

- **The RmlUi test card no longer jumps left** when grabbed with the mouse: it now moves from where it stands.
- **The card's numbers are grouped** like the rest of the interface: "5 157 / 15 137".

# uaRO Graphics 3.0.18-beta.7 — what's new

- **A check of the new interface library (RmlUi):** the Interface section has a new switch for a test card of the new library, off by default. When on, it shows a character card with HP and SP at the top in the middle: it fades in, lights up under the cursor and can be dragged with the mouse. Every 10 s the log records what it costs. The rest of the interface does not change.

# uaRO Graphics 3.0.18-beta.6 — what's new

- **The log tells the Taekwon stances:** a blue frame over the head in Roundhouse Stance. The log now records how the game draws these effects and how the graphics lays them. This is for finding the cause; the picture does not change.

# uaRO Graphics 3.0.18-beta.5 — what's new

- **The log tells the walls' light:** in dungeons (the pyramid, the sphinx) the walls come out brighter than in the game itself. The log now records the light the game holds for the map and all corners of the first wall that differs. This is for finding the cause; the picture does not change.

# uaRO Graphics 3.0.18-beta.4 — what's new

- **No more old "Please wait..." showing through:** when the game showed a message and a "Please wait..." at once (it happens on a first login to an account), the wait stayed in its old look. Now it is hidden then too.
- **The log names the game's old windows** left on screen without the new look - a line for each kind of window, to find the rest of such places.

# uaRO Graphics 3.0.18-beta.3 — what's new

- **"The game froze" - a report of its own:** when the game stood still for 15 seconds or more and had to be closed, the automatic report is now called so, not "ended abnormally"; in the reports list - "The game froze".

# uaRO Graphics 3.0.18-beta.2 — what's new

- **The hang watch:** when the game stands still for 15 seconds or more, the log tells at once where it is stuck and a memory snapshot of the game is saved beside it. If the game then had to be closed, the app sends a report with them by itself - a hang can now be read, not guessed.

# uaRO Graphics 3.0.18-beta.1 — what's new

- **The cart look in the new style:** the Change Cart window is now a pick card - the carts your level has, chosen with the mouse, the keys or the controller, OK changes the cart.
- **The log names the game's old windows** not redrawn in the new interface yet - one line a window, the first time it shows. The reports will say which of them players meet - those are redrawn next.

# uaRO Graphics 3.0.17 — what's new

A simple start: a welcome screen, three choices instead of hundreds of boxes, a pick by your graphics card, quick settings in the game, the new features in one list and going back to the previous version. And many fixes.

## A simple start

- **The welcome screen "Game":** play, another game window, report a problem, the news, a short summary of the game's look.
- **The first start:** four cards "How do you want to play?" - one button sets the picture, the interface and the textures. After an update - a plate with what's new.
- **Three choices** - picture, interface, textures - on the main page and in Alt+D, marked "changed N" when something was changed by hand.
- **A pick by your graphics card** - without starting the game, by your card's level.
- **Alt+D:** tabs (quick, new, picture, interface, textures, all); the quick one as tiles, four a row, Insert pins a row; at the foot - "what it gives" for the chosen row. On the first map - a hint in the chat.
- **More settings** folded away; **the new features** and **only the changed ones** - one click on the game page.
- **Going back:** the service page, another version (the last 5 releases and betas); after two failed starts the app offers the previous one itself.
- **A hung game** with no window no longer holds the update - a button closes it.

## Fixed

- **uaRO HD and the old uaRO:** uaRO HD's screen no longer breaks the old client ("Cannot init d3d") - uaRO HD keeps its screen in its own file, and every start of uaRO HD gives the old client its setup.exe screen back.
- **Battle mode:** an Enter in an NPC's, a Kafra's or a warp menu's window no longer turns chat typing on - letters no longer slip into the chat instead of skills.
- **The character's silhouette behind walls** no longer lies over the character itself and over effects (Assumptio, Two-Hand Quicken).
- **The white "dome" round monsters** from the hit flash is gone; the hit and death flashes come on only by hand.
- **Cyrillic on an English Windows:** the tip of the day, the chat and names without garbage; uaRO HD's tips in the interface's language.
- **Guild and party marks** on the minimap no longer doubled.
- **The controller** in the shop, homunculus and mercenary cards.

# uaRO Graphics 3.0.17-beta.27 — what's new

- **Alt+D, the quick tab - tiles:** the often-used switches are now tiles, four a row, the ones on lit green. Left/right choose a tile, Space or Enter turns it on or off, Insert pins it; with the mouse - click a tile.
- **"What it gives":** at the console's foot, one sentence about the chosen row or tile, so there is no guessing what you turn on.

# uaRO Graphics 3.0.17-beta.26 — what's new

- **The tip of the day in uaRO HD in the interface's language:** uaRO HD's tips file has Windows line ends, and the game counted 69 tips instead of 68 - so our translated tips never came on. uaRO HD now shows them in the app's language, the counter "of 68".

# uaRO Graphics 3.0.17-beta.25 — what's new

- **Cyrillic on an English (or any non-Cyrillic) Windows:** the tip of the day, the chat, names and the game's other texts no longer turn into garbage - uaRO's texts are now always read as Windows-1251 unless the settings set another code page.

# uaRO Graphics 3.0.17-beta.24 — what's new

- **No silhouette on the character itself:** the light outline on the head (under Two-Hand Quicken and in other poses) was the body's silhouette, "hidden" by the character's own head. The character's parts no longer hide one another; the silhouette shows only behind a wall or a house.

# uaRO Graphics 3.0.17-beta.23 — what's new

- **No silhouette from a buff's trail:** under Two-Hand Quicken and similar buffs the game draws see-through copies of the character, and while walking they turned into a blue silhouette. The silhouette now takes the character alone.

# uaRO Graphics 3.0.17-beta.22 — what's new

- **No white "dome" round monsters:** the flash of a hit and of a death no longer blurs through the halo into a bright dome round the monster - only a short flash on the monster itself stays.
- The hit and death flashes no longer come on with the effects' sets - only when turned on by hand (the effects page, the characters and monsters group).

# uaRO Graphics 3.0.17-beta.21 — what's new

- **The character's silhouette behind walls no longer lies over effects:** Assumptio and other effects round the character came out as a blue silhouette right on it, though nothing hid it. Now the silhouette is only the character itself, when a wall or a house hides it.

# uaRO Graphics 3.0.17-beta.20 — what's new

- **Battle mode: letters no longer slip into the chat.** The game turned chat typing on with every Enter — even one pressed in an NPC's, a Kafra's, a warp menu's or a message's window; then skills and items did not fire and the letters went into the chat. Now an Enter in another window leaves typing as it was, and a new map with an empty chat line gives the letters back to the skills.

# uaRO Graphics 3.0.17-beta.19 — what's new

- **A hung game no longer holds the update:** when the game runs without a window (it hung while starting), the service page offers a button to close the hung game - the app closes it (and this folder's setup.exe), and the update goes on.
- **White ground (R-7XY449):** the log now writes what the ground's colour is made of on each map - the next report with white ground shows the cause.

# uaRO Graphics 3.0.17-beta.18 — what's new

- **uaRO HD no longer breaks the old uaRO:** uaRO HD's screen (size, full screen) now lives in its own file, and the driver gives it to the game itself. The shared registry place stays the old client's: every start of uaRO HD's app gives the old client its setup.exe screen back ("Cannot init d3d" should not show again). The login, bars and the rest of the game's settings stay as they were. When the old client has uaRO Graphics of its own, its screen is left alone.

# uaRO Graphics 3.0.17-beta.17 — what's new

- **Fixed: uaRO HD starts again.** uaRO HD's own registry place (betas 13–16) is gone: the game and the app work with the shared one again, as before 3.0.17-beta.13. When those betas took uaRO HD's screen out of it, the app brings it back from their copy.

# uaRO Graphics 3.0.17-beta.16 — what's new

- **Fixed: after 3.0.17-beta.13 uaRO HD did not start (the game hung with no window).** Moving the settings to their own registry place no longer runs while the driver loads but at the game's first registry call. When the game cannot read its own place, it stays on the shared one, as before.

# uaRO Graphics 3.0.17-beta.15 — what's new

- **Fixed: after 3.0.17-beta.13 uaRO HD could fail to start.** The game moves to its own registry place only when it can read it itself and a screen is there; otherwise it stays on the shared one, as before. The choice is written to the log (game key: lines), and the settings app works with the same place as the game.

# uaRO Graphics 3.0.17-beta.14 — what's new

- **uaRO HD repairs the old uaRO at every start:** when a foreign screen shows up again in the shared registry place and keeps the old client from starting ("Cannot init d3d"), uaRO HD removes it and gives the old client its setup.exe screen back — every time, not only the first. When the old client has uaRO Graphics of its own, its screen is left alone: any screen works there.

# uaRO Graphics 3.0.17-beta.13 — what's new

- **uaRO HD and the old uaRO no longer get in each other's way:** both clients kept the game's settings in one place of the Windows registry, and the screen chosen for uaRO HD (32 bits, its size) broke the old client — "Cannot init d3d OR grf file has problem". uaRO HD now has a place of its own: the first start moves everything there (login, bars, windows, bookmarks), and the old client gets the screen from its setup.exe back. The old client loses nothing.

# uaRO Graphics 3.0.17-beta.12 — what's new

- **Alt+D → «New» tab:** the switches that came since you last looked in the console — turn them on and see them in the game at once. «Quick» shows «★ New: N». The first start marks nothing; the list clears once seen.

# uaRO Graphics 3.0.17-beta.11 — what's new

- **A hint in the game:** once, 5 seconds into the first map, the chat shows «uaRO Graphics: Alt+D - quick settings (the picture, the interface, the textures)». It does not come again.

# uaRO Graphics 3.0.17-beta.10 — what's new

- **First start:** «Play» asks «How do you want to play?» with four buttons: «I want everything new», «New picture, the old interface», «A weak computer, but sharp», «As in 2004». One press sets the picture, the interface and the textures (the new picture fitted to your card).
- **After an update** «Updated to …» shows once, with «What's new» and «★ New features».

# uaRO Graphics 3.0.17-beta.9 — what's new

- **«Choose for my card»** on «Play», in «How the game looks». The app looks at your video card and sets «Picture» and «Textures» at once: after the first game by how the card actually drew it, before that by its memory. The interface stays as it is.
- The old scene record (`uaro-graphics\scene-record.bin`, over 1 GB) is removed by itself when «Record a scene» is off.

# uaRO Graphics 3.0.17-beta.8 — what's new

- **For developers: «Record a scene»** now logs what the record is made of — textures, pictures the game draws itself, vertices — before the first map and after it.

# uaRO Graphics 3.0.17-beta.7 — what's new

- **For developers: «Record a scene»** ([debug] record_scene, «Developer» page). Records the game's picture from the start to 15 s after the first map into `uaro-graphics\scene-record.bin` and logs its size. The first step of the video card check without the game; turn it off after recording.

# uaRO Graphics 3.0.17-beta.6 — what's new

- **«★ New features (N)»** on «Play»: everything the update brought in one list. Switch each row right there; the button under it opens its page.
- **«Changed only»**: everything set otherwise than recommended — handy to find something turned off once and forgotten. The dot beside a row puts the recommended value back.

# uaRO Graphics 3.0.17-beta.5 — what's new

- **Every page of the app shows in any mode**, «Main» second, right after «Play». In the simple mode each page shows the main settings first and the rest behind «N more settings ›». Nothing is hidden for good.

# uaRO Graphics 3.0.17-beta.4 — what's new

- **Alt+D in the game has tabs now:** «Quick», «Picture», «Interface», «Textures», «All». ← → on the top row switch them. **«Quick»** opens first: the three «How the game looks» choices, what is switched most (night, weather, grass, torch shadows, new effects, 60 fps, frame counter, ping, monster HP) and your pinned rows. **Insert** on any row pins it ★ to «Quick» (again: takes it off). «All» is the full list as before; typing letters searches everything.

# uaRO Graphics 3.0.17-beta.3 — what's new

- **«Play» became the welcome screen.** A big «Play» at the top, «One more game window» and «Report a problem» beside it. Below: the update (and «← Back» when needed), «How the game looks» with its three choices and «Change ›», the graphics test. News and the server on the right.

# uaRO Graphics 3.0.17-beta.2 — what's new

- **«How the game looks»** now opens the «Main» page: three separate choices instead of hundreds of switches. **Picture:** Auto, As in the game, Improved, Remaster. **Interface:** New, New screen with the game's windows, Classic. **Textures:** HD, HD light, Original. Mix them any way, e.g. the new picture with the classic interface. If you changed something by hand, the choice shows «changed N».

# uaRO Graphics 3.0.17-beta.1 — what's new

- **Going back to the previous version is easy now.** On «Play», next to the update, a «← Back to 3.0.16» button. If the game did not start twice after an update, the app offers to go back itself.
- **«Service» → «Another version»:** your channel's last 5 versions, each installed with one button. The settings stay.

# uaRO Graphics 3.0.16 — what's new

Torch shadows, new hit and status effects, the shop opening card, the homunculus' and mercenary's skills in their cards, and many fixes. Everything new that changes the look is off by default — turn it on in the uaRO app.

## Fixed

- **Doubled letters** in input fields (NPC questions, counts, search) in battle mode after Enter.
- **Your own pet after logging in** was sometimes treated as a plain monster with no right-click menu until the first teleport.
- **The pet's accessory** is worn again by a double click in «Items»; the pet menu in Ukrainian and Russian.
- **Minimap:** the ama_dun01 maze shows again; a guild mate in your party no longer flips between a triangle and a circle.
- **Ctrl+F** opens the reference again when the chat field is empty.
- **False «crash» reports** when the game was closed and started again at once.
- **Fire Wall** is the game's soft fire again, brighter.

## New effects (off — turn on in «Effects»)

- **«Torch and lamp shadows»** — walls, pillars, trees, heroes and monsters cast shadows from fire; **«Light through leaves»** — green light through tree crowns. Direct3D 12 and 11.
- **«Effects behind the hero»** — skill effects no longer hide behind the character.
- **New hits, items, statuses, travel and monster effects** in the same crisp colours as the new skill effects; **«Soft look»** for all new effects.
- **«Sharp hero pixels»** — sprites close up without blur.
- **«Death flash»**, **«Body tint»** (Fury, Steel Body), **«Hallucination wave»**.
- **Monk spheres and gunslinger coins** — glowing orbs with a halo.

## Interface

- **«Shop opening card»** — name, cart and prices in one card, the pad too.
- **Homunculus and mercenary skills** in their cards: use level, click to use, drag to the hotbar.
- **On the hotbar:** stones and spheres a skill lacks; Safety Wall, Pneuma, Sanctuary and trap timers (and a ring on the ground); the monk's combo window.
- **In the character block:** broken gear, pet and homunculus hunger.
- **Shop and chat room signs** in the new look, **the cell under the mouse**.
- **«Stats» → map monsters:** hits by monster size and the Ghost element.
- **«New report» from the game:** you choose its kind.

# uaRO Graphics 3.0.16-beta.39 — what's new

- **The pad on the shop opening card.** D-pad ← → — the cart or the sale, ↑ ↓ — a row (gold edge); A puts a thing up, X takes it back, LB / RB — the price a step down / up, Y — «Open the shop», B — «Cancel». The buttons' hint is at the card's bottom.
- **The pad on the homunculus' and the mercenary's skills.** ↑ ↓ over the skill rows, ← → the use level, A uses, X raises a homunculus skill.

# uaRO Graphics 3.0.16-beta.38 — what's new

- **Fixed: a guild mate in your party on the minimap.** It no longer flips between the guild's pink triangle and the party's face circle: a party member shows only as the circle.

# uaRO Graphics 3.0.16-beta.37 — what's new

- **New: "Hallucination wave"** (Effects → Screen, off at first). While your character is under Hallucination, the picture of the world sways in slow waves, as in newer clients; the interface stays still. Our client shows nothing but the icon on its own.

# uaRO Graphics 3.0.16-beta.36 — what's new

- **Fixed: after entering the game your own pet was sometimes taken for an ordinary monster** (no right-click menu) until a teleport. The game now recognises it by the server's next message about it (hunger, accessory), usually within seconds.

# uaRO Graphics 3.0.16-beta.35 — what's new

- **"Shadows from torches and lamps", "Light through the leaves" and "Effects behind the character" now work on Direct3D 11 too** (older graphics cards). They are switched on in the same place, in "Effects", off at first.

# uaRO Graphics 3.0.16-beta.34 — what's new

- **Fixed: the game did not start with "cannot init d3d"** on betas 30–33 (an error in the torch shadows' shader).

# uaRO Graphics 3.0.16-beta.33 — what's new

- **New: "Opening a shop card"** (Interface → Windows, off at first). Opening your shop (Vending) as one card in the new look: the shop's name, the cart's things (a click puts one up for sale), the things for sale with a price field and a cross to take one back, and the "Open the shop" button. The game itself checks the prices, as before.

# uaRO Graphics 3.0.16-beta.32 — what's new

- **A closer comparison with the monsters.** In "Stats" → "Planning" → "Map monsters" and on a monster's card, your blow now counts the monster's size (a dagger is weaker on large ones, an axe on small ones and so on) and element: a plain blow on ghosts gets through only in part or not at all. The number of blows to win is closer to the real one.

# uaRO Graphics 3.0.16-beta.31 — what's new

- **The homunculus' and the mercenary's skills in their cards.** They now do everything the game's old skills window did: the level a skill is used at is chosen with ‹ ›, a click uses the skill at that level, and a skill can be dragged onto the hotbar. The mercenary's skills can be used and dragged too.

# uaRO Graphics 3.0.16-beta.30 — what's new

- **New: "Light through the leaves"** (Effects → under "Torch shadows", off at first). A tree's leaves between a torch or a lamp and the ground no longer cast a black shadow: a part of the light comes through in green spots. Direct3D 12 only.

# uaRO Graphics 3.0.16-beta.29 — what's new

- **New: "Sharp character pixels"** (Effects → Characters, off at first). With the camera close, the sprites of characters, monsters and NPCs are drawn as clean squares of their pixels with a soft edge - no blur, no flicker. Far off everything looks as before. Works on Direct3D 12 and 11.

# uaRO Graphics 3.0.16-beta.28 — what's new

- **Check: the pet after entering the game.** The log records the server's pet packets to find out why, after entering, the pet looks like an ordinary monster (no accessory, no menu) until the first teleport. Nothing changes in how the game looks.

# uaRO Graphics 3.0.16-beta.27 — what's new

- **New: "Soft look" for the new skill effects** (Effects → under "Mages' new effects", off at first). Instead of crisp dense layers - the game's own glow and see-through light, tinted a little in the element's colours and brighter. It applies to every class's new effects: Safety Wall, Sanctuary, Pneuma, Magnus and the rest look lighter. The Monk's spheres and Fire Wall keep their own look.

# uaRO Graphics 3.0.16-beta.26 — what's new

- **"New report": you choose what it is about.** Opened from the game (Ctrl+Shift+F10), "Graphics" is no longer chosen in advance — pick what it is about or leave it empty. "Other" is now "Interface and other": windows, items, NPCs.

# uaRO Graphics 3.0.16-beta.25 — what's new

- **New: "Effects behind the character"** (Effects → Characters, off at first; Direct3D 12 only). The game draws characters so that effects after them lie over the whole body — even the far half of a ring round the hero. Now what is behind a character hides behind its figure, and what is in front is drawn over as before.

# uaRO Graphics 3.0.16-beta.24 — what's new

- **Fire Wall is brighter.** The game's soft flames and their flicker stay, but each layer gives about three times the light and a little warmer — the fire shows by day on the grass too.

# uaRO Graphics 3.0.16-beta.23 — what's new

- **Fire Wall has the game's soft fire again.** Our mage look repainted its flames in crisp colours and the three columns came out as flat red slabs. Fire Wall keeps the game's own fire with our glow now.

# uaRO Graphics 3.0.16-beta.22 — what's new

- **"Time rings on the ground": the countdown on the hotbar too.** While your Safety Wall, Pneuma, Sanctuary or trap stands, its icon shows a gold bar along the top running down and the seconds in the middle ("32"; with two walls "32 ×2", the one ending first). The countdown is seen even when you stand on the wall and its ring is hidden under you.

# uaRO Graphics 3.0.16-beta.21 — what's new

- **New: "Time rings on the ground"** (Interface, off at first). Your own Safety Wall, Pneuma, Sanctuary and traps get a ring on the ground in the skill's colour. Its bright part runs down as their time goes (by the skill's level, as the server counts it) and pulses in the last 3 seconds. A broken wall or a sprung trap takes the ring away at once.

# uaRO Graphics 3.0.16-beta.20 — what's new

- **No double shadow from a torch.** With "Shadows from torches and lamps" on, "Characters' shadows" no longer draw their simple shadow from the same fires — one real shadow stays. The sun's shadow and the round spot under the feet stay as they were.

# uaRO Graphics 3.0.16-beta.19 — what's new

- **"Shadows from torches and lamps": no dark patch under braziers.** A brazier's bowl and legs shadowed the ground under its own fire — a dark diamond and a star of rays. A fire's own stand casts no shadow from it now.

# uaRO Graphics 3.0.16-beta.18 — what's new

- **"Shadows from torches and lamps": characters too.** Heroes, monsters and NPCs near a torch or a lamp cast a real shadow of their silhouette from it — on the floor and the walls, away from the light.

# uaRO Graphics 3.0.16-beta.17 — what's new

- **A pet's accessory is worn again on a double click in the items window**, as in the game's own window. The new window did nothing with it before.
- **The pet's right-click menu in Ukrainian and Russian** when the program is in those languages.

# uaRO Graphics 3.0.16-beta.16 — what's new

- **New: "Shadows from torches and lamps"** (Effects → "Fire and smoke", off at first; Direct3D 12 only). Walls, columns, houses and hills stop the light of the nearest torches and lamps: what stands behind them stays in shadow, with soft edges. Up to the 8 brightest lights on screen have shadows. The "Shadow strength" slider sets how much light a shadow takes away. For now walls, models and the ground cast shadows; characters come later.

# uaRO Graphics 3.0.16-beta.15 — what's new

- **No more false "crash" reports.** Closing the game and starting it again at once could make the program read the new game's log and send an automatic crash report although the game had closed normally. It now reads the log of the game that closed.

# uaRO Graphics 3.0.16-beta.14 — what's new

- **"Afterimages when fast" removed** — the switch is gone. "Body tints" stays.

# uaRO Graphics 3.0.16-beta.13 — what's new

Six new switches, all off (turn them on in the uaRO program):

- **"What a skill lacks"** (Interface): a skill on the bar short of its gem or spheres is dimmed with a red plate - ×0 or 3/5, its tip says what.
- **"Combo window"** (Interface): the Monk's skill that chains now is outlined in gold with its time shrinking; Asura glows blue when it can go (5 spheres and Fury).
- **"Broken gear"** (Interface): when a weapon or armour breaks - a sound and a red "Broken: ..." in the character block.
- **"Pet and homunculus hunger"** (Interface): their fullness in the character block, amber and red when hungry, with a sound.
- **"Afterimages when fast"** (Effects → Characters): pale copies of the hero behind it when it moves faster than a walk.
- **"Body tints"** (Effects → Characters): a warm red under Fury, steel under Steel Body.

# uaRO Graphics 3.0.16-beta.12 — what's new

- **The Monk's spirit spheres** are a little bigger and blue: a blue ball with a shine, a light heart and a soft blue aura.

# uaRO Graphics 3.0.16-beta.11 — what's new

- **The Monk's spirit spheres around the hero** are light blue again, as in the original: a white heart, a light blue ball and a soft light blue aura with a shine.

# uaRO Graphics 3.0.16-beta.10 — what's new

- **The Monk's spirit spheres around the hero** (the acolytes' new effects) are now warm glowing orbs with a soft aura, depth and a shine, instead of flat blue discs with a yellow dot. The Gunslinger's coins too.
- The spheres' plate on the skill bar is plain dots again, as in 3.0.16-beta.8.

# uaRO Graphics 3.0.16-beta.9 — what's new

- **Spheres on the skill bar** are now glowing golden orbs with a halo, depth and a shine; empty places are faint glass beads (instead of plain dots).

# uaRO Graphics 3.0.16-beta.8 — what's new

- **"Gems and spheres on the skill bar"** (Interface, off at first): next to the ammo's plate - what the skills on your bar spend and how many are in the bag: blue, red and yellow gems, Holy Water, the Sage's and the Ninja's stones, the Alchemist's bottles and coats. A red edge and a short chime at 10 or fewer and when none are left. The Monk's spirit spheres and the Gunslinger's coins as gold dots.

# uaRO Graphics 3.0.16-beta.7 — what's new

Three new looks, all off by default (turn them on in the uaRO program):

- **«Signs over the characters»** (Interface): the shops' and chat rooms' signs in the new look — a dark plate with the title, gold for a shop, blue for a room with its places (3/20), grey for a locked one. A click opens them as before.
- **«Cell under the mouse»** (Interface): the ground cell under the mouse as a soft light square with a thin edge, like the skills' cells, in place of the game's own.
- **«Flash on a death»** (Effects → Characters): a dying monster lights up white, brighter and longer than from a hit. Needs the game's sound on.

- The minimap on ama_dun01 (Tatami Maze) shows the maze again instead of an «X»: the red route on the map's picture is no longer taken for a warp dot. The map's picture is ready at once instead of after 47 seconds.

# uaRO Graphics 3.0.16-beta.5 — what's new

**No more doubled letters.** In the battle mode, after Enter to type, the game put every letter twice into any field but the chat's: an NPC's question, «How many» and the like. The game's own bug; now a letter goes in once.

# uaRO Graphics 3.0.16-beta.4 — what's new

**Ctrl+F opens «Reference» again.** The game keeps its chat field active nearly always, and Ctrl+F went to the chat's search - the reference never opened. Now the chat's search only with words typed in its field; else Ctrl+F is the reference.

**Doubled letters looked into:** when one key brings two letters, the log tells by which way - to find the cause.

# uaRO Graphics 3.0.16-beta.3 — what's new

**«New damage numbers» removed** - the damage numbers are the game's own again.

# uaRO Graphics 3.0.16-beta.2 — what's new

**«New damage numbers»** («Interface», «Fight», off by default). Damage and healing over characters and monsters as crisp numbers with a dark rim in place of the small pixel ones: in the game's colours, a critical larger and orange with a pop, a heal green with a plus; thousands apart (1 234 567), rising and fading.

# uaRO Graphics 3.0.16-beta.1 — what's new

**New effects beyond the skills** (effects, «Skills», a switch a group, off by default): «New hits» (the weapons' hit sparks, combos, dark and holy hits), «New potions, buffs and items» (potions, Concentration and Berserk, food, Kaizel, Kaahi, thrown items, cooking), «New states» (frozen, stone, sleep, stun, curse, blind, poison), «New teleport, levels and refining» (teleport, coming and going, Level Up, Job Level Up, a job changed, refining, the MVP's), «New monsters' and homunculi's effects». As the skills': crisp layers in their own colour in place of white blots.

# uaRO Graphics 3.0.15 — what's new

New skill effects for every class, the crashes entering castles fixed and smaller item windows. Everything new that changes the look is off by default - turn it on in the uaRO program.

## Fixed

- **A crash loading some maps, guild castles above all** («Unexpected program error», cps.dll). The unpacker of the game's files read and wrote a little past its buffers' ends - it no longer does.

## New skill effects

Effects, «Skills», a switch a branch. In place of white blots: crisp layers in the element's colour, solid parts (spikes, walls) lit by their faces.

- **«Mages' new effects»** - Mage, Wizard, Sage; boxes under it: «Casting», «Fire», «Water and ice», «Wind», «Earth», «Ghost and the rest». **Casting** - a magic circle on the ground in the element's colour and a low crisp flame in place of the column up the screen. **Storm Gust** - crisp snow and cloud in icy blue, faceted spikes.
- **«Swordsmen's», «Acolytes'», «Archers'», «Thieves'», «Merchants'», «Other classes' new effects»** - colours of their own: holy white and gold, healing white and green, poison green, dark violet, weapon strikes steel, songs and coins gold.

## Interface

- **«Item windows size»** («Interface», «Main»: 70, 85, 100 %) - «Items», the storage (the guild's too) and the cart smaller.
- **The how many window** («Into the bag», «Into the storage»...): the field empty - type at once; Enter alone takes all. The «1» button is «10».

# uaRO Graphics 3.0.15-beta.12 — what's new

**Every class's new effects** (effects, «Skills», a switch a branch, off by default): «Swordsmen's new effects», «Acolytes'», «Archers'», «Thieves'», «Merchants'», «Other classes'» (Taekwon, Star Gladiator, Soul Linker, Ninja). As the mages': crisp layers in place of white blots, solid parts by their faces, colours of their own - holy white and gold, poison green, dark violet, weapon strikes steel, healing white and green, songs and coins gold.

# uaRO Graphics 3.0.15-beta.11 — what's new

**«Mages' new effects»** (effects, «Skills», off by default). The Mage's, Wizard's and Sage's skills in their own look: crisp, in the element's colours, no white blots - the layers the game added on now lie over each other with a crisp edge, the solid parts (spikes, walls) lit by their faces. Boxes under the switch turn each part off on its own: «Casting», «Fire», «Water and ice», «Wind», «Earth», «Ghost and the rest». «New casting look» and «New Storm Gust» are here now.

# uaRO Graphics 3.0.15-beta.10 — what's new

**Getting the mages' new effects ready.** The skill effects record in betas (uaro-graphics\logs\uaro-effects.txt, sent with a report) now takes up to 64 effects a game and names each by the skill the character cast before it - to record all the Mage, Wizard and Sage skills in one go.

# uaRO Graphics 3.0.15-beta.9 — what's new

**«Item windows size»** (the «Interface» page, «Main»: 70, 85 or 100 %). «Items», the storage (the guild's too) and the cart smaller - two side by side no longer cover the whole screen; the other windows stay as they are. Applied at once, no game restart.

# uaRO Graphics 3.0.15-beta.8 — what's new

**New Storm Gust** («New Storm Gust» in the effects' «Skills» group, off by default). In place of the white blot: crisp snow and cloud in icy blue, lightning in its own colour, a soft haze below, faceted ice spikes with a glint. Made from a record of how the game draws Storm Gust: the same layers and timing.

**One more guard against a crash loading maps (cps.dll):** the unpacker writes no further than the game really allocated.

# uaRO Graphics 3.0.15-beta.7 — what's new

**A crash loading some maps (guild castles) fixed.** Our unpacker of the game's files (cps.dll) read a little past the data's end, and when a file lay in memory right at a boundary the game fell («Unexpected program error», cps.dll). There since 3.0.2.

**The how many window («Into the bag», «Into the storage»...):** the field is empty now - type the number at once, no erasing the most; Enter alone still takes all. The «1» button is «10».

# uaRO Graphics 3.0.15-beta.6 — what's new

**«New casting look»: the casting at a target as the game draws it, with a magic circle.** For ground skills (Storm Gust, Lord of Vermilion...) the game draws a casting where the skill will land too: its rings now stay as the game draws them, with one even magic circle under them (a double, crooked one before). The casting under the character keeps the new look, as in beta.5.

# uaRO Graphics 3.0.15-beta.5 — what's new

**«New casting look» smaller.** The flame at three quarters of the game's size and lower, the magic circle clearly narrower (1.25 of the game's rings' radius, not 1.8).

# uaRO Graphics 3.0.15-beta.4 — what's new

**«New casting look» as in the picture.** In beta.2-beta.3 the card never got the new look: the flame was only lower and the circle was drawn with the game's texture. Now crisp flame tongues and a magic circle with rings, marks and a star in the element's colour. The circle lies on the ground along its slope - its near half no longer sinks into it.

# uaRO Graphics 3.0.15-beta.3 — what's new

**«New casting look» now shows.** The magic circle never appeared: the casting was quietened before its rings at the feet were found - the new look has its own brightness now. The circle and the flame in the element's colour (the game gives it by texture: fire, water, wind, earth), the circle's lines with a thin dark rim so they read on Prontera's light paving too.

# uaRO Graphics 3.0.15-beta.2 — what's new

**New casting look** («New casting look» in the skill effects group, off by default). While a skill is cast: a magic circle on the ground in the element's colour (two rings, marks, a slowly turning star) and a low crisp flame round the character in place of the blurred column up the screen. Made from a record of how the game draws the casting (beta.1's report): the same rings and timing, the flame lower and crisper.

# uaRO Graphics 3.0.15-beta.1 — what's new

**Getting ready for new skill effects.** In betas the driver records how the game draws skill effects: each effect's first frames in a game - triangles, blending, textures - into uaro-graphics\logs\uaro-effects.txt (1.2 MB at most), sent with a report. The casting, Heal, Magnus Exorcismus and Storm Gust will be redrawn from these records. Releases record nothing.

# uaRO Graphics 3.0.14 — what's new

Players' ideas from the «50 ideas» list: following a player, long chat lines, the character's silhouette behind buildings, a big screenshot and many smaller things. Everything new that changes the look is off by default - turn it on in the uaRO program.

## Game

- **Steady following of a player.** Shift+click a player - your character follows and keeps up when they stop and set off again. Handy with two game windows: the priest in the second one runs after you. Stop with a click on the ground or Esc. «Steady following of a player», on.
- **Long chat lines** - up to 234 letters: a long line goes as several messages, cut between words. «Long chat lines», on. The /p, /g, /w and «/ex» commands from the chat menu now put their text in the field.
- **The camera lower, down to the ground:** «Camera tilt» - «As the game», «Lower», «To the ground».
- **Small windows** 960×540, 800×600 and 640×480 - for several game windows side by side.
- **Esc closes an NPC's picture** left after a talk. «Esc closes the NPC's picture», on.
- **Screen shake, Frost Joke and Scream lines in the chat and the game's swear filter can be turned off:** «Screen shake», «Frost Joke and Scream lines in the chat», «The game's swear filter» (on, as in the game).
- **Quick teleport:** Teleport and Fly Wing without the screen going dark - «Quick teleport on the map».

## Interface

- **Boss HP bars:** an MVP's wide and golden, a mini-boss's its own - «Boss HP bars».
- **«Who hits me»:** monsters hitting you framed in red, and how many over the character.
- **The party's damage in «Hunt»:** each member's share since the hunt began.
- **Your own chat colours** for the party, the guild, your lines and others'; **the chat log to a file** - «Chat log to a file».
- **Arrows and tips** on the side of the hotbar with room; **the NPC talk window** sized to its words, and it can be moved.
- **Experience in the chat once**, MVP lines with spaces, «Not Enough SP» in the interface's language.
- **The minimap lighter** on the game.

## Graphics

- **«Character's silhouette behind buildings»:** where a house, a tree or a wall hides your character, its light-blue ghost shows.
- **«Others' skill effects fainter»:** skill effects on other players and monsters at a third of their strength, yours and your party's bright (guild wars, the market).
- **«Aura does not hide portals»:** the level 99 aura fades over a portal.
- **Big screenshot: Ctrl+Alt+F11** - a frame without the interface, twice the screen's size, with every effect.

# uaRO Graphics 3.0.13-beta.33 — what's new

**«Character's silhouette behind buildings»: the neck and head no longer go missing.** While the character walked, the camera trailed it, and the sprite's narrow parts (head, neck) sometimes missed the silhouette - now taken with room to spare. And with the character right behind a wall, its top edge no longer leaves an empty band at the neck.

# uaRO Graphics 3.0.13-beta.32 — what's new

**«Character's silhouette behind buildings» nicer.** In place of a flat blue blot, a ghost: a bright rim round the character and a faint cold fill with the sprite itself showing through. The silhouette no longer creeps over the character standing in the open (the ground by its feet nearer the camera than the sprite's bottom): it shows only where something clearly nearer hides it. The head is in the silhouette too.

# uaRO Graphics 3.0.13-beta.31 — what's new

**«See-through houses before the character» removed** - the buildings before the character are as the game draws them again. In their place, **«Character's silhouette behind buildings»**: where a house, a tree or a wall hides your character, its shape shows over them in light blue. The silhouette did not show while the see-through buildings were on: they cut the wall away, leaving nothing to show it over.

# uaRO Graphics 3.0.13-beta.30 — what's new

**New: «Character's silhouette behind buildings»** (effects, the characters' group, off by default). Where a house, a tree or a wall hides your character, its shape shows over them in light blue - the buildings stay as they are.

**«See-through houses before the character»:** the see-through window back to its earlier size (as in beta.28).

# uaRO Graphics 3.0.13-beta.29 — what's new

**«See-through houses before the character»: the see-through window larger and softer.** The window round the character is nearly twice as wide, and its rim fades into the wall smoothly over most of it - no sharp circle any more.

# uaRO Graphics 3.0.13-beta.28 — what's new

**«See-through houses before the character»: the wall see-through, not a hole.** Round the character the houses, trees and walls before it go half see-through - the character and the ground show through, the wall itself stays visible. The dark ring at the rim is gone: the game draws its walls before the ground and the rim was laid over nothing - now the see-through part is drawn once the ground and the character are down.

# uaRO Graphics 3.0.13-beta.27 — what's new

**«See-through houses before the character»: the window with no pattern and no dark spots.** The middle of the window round the character is now clear, and its rim comes back smoothly in real see-through - the fine pattern is gone, and with it the dark dots of the shadows in the corners. The window is now the character's size (it was three times larger, taking half a wall).

# uaRO Graphics 3.0.13-beta.26 — what's new

**«See-through houses before the character» redone: a see-through window round the character.** The driver used to choose which models to make see-through whole, and they went on and off as you walked and turned the camera. Now the card leaves a see-through window round the character in the houses, trees and walls before it - it moves with the character smoothly, no blinking. Only what is nearer the camera than the character fades; what is behind does not.

# uaRO Graphics 3.0.13-beta.25 — what's new

**«See-through houses before the character»: smooth at any frame rate.** The see-through faded and came back in 8 frames: at 120 frames a second that is 70 ms - a blink. Now by the clock: it fades in a quarter of a second, holds half a second after the model stops covering and comes back in 0.4 s. Along with the model over the character, a house's neighbouring parts fade only near the character on the screen - lamp posts and benches round it stay as they are.

# uaRO Graphics 3.0.13-beta.24 — what's new

**«See-through houses before the character»: only what covers the character fades.** The game may draw many alike models at once (all trees of a kind), and along with the tree over the character the trees, benches and fences all over the square faded - and blinked as you walked. Now each model inside such a draw fades on its own.

# uaRO Graphics 3.0.13-beta.23 — what's new

**«See-through houses before the character»: the fences round the character no longer blink.** Along with a model covering the character only its neighbouring parts in the world fade (the whole house), not everything over it on the screen - the long fences and kerbs round a square no longer go on and off at each step. The see-through holds a little longer after a model stops covering.

# uaRO Graphics 3.0.13-beta.22 — what's new

**«See-through houses before the character»: statues and houses no longer blink while you walk behind them.** The game draws a model a little differently each frame, leaving out the faces turned away from the camera, so a statue counted as a new one each time and started fading over again. Now a model is known by its texture and middle.

# uaRO Graphics 3.0.13-beta.21 — what's new

**«See-through houses before the character»: the see-through no longer blinks.** Walking behind a fountain or a house, it keeps one see-through: no flash when the body slips past its edge for a moment, and no fading over again when the model moves (the fountain's water).

# uaRO Graphics 3.0.13-beta.20 — what's new

**See-through buildings: what is behind no longer fades.** Tall things behind the character (Prontera's fountain) went see-through: the camera looks down and their tops are nearer to it. Now only what is nearer the camera along the ground fades - really between the camera and the character.

**Others' skill effects fainter** (a new switch «Others' skill effects fainter» in the skill effects' group, off by default). Skill effects on other players and on monsters neither you nor your party hit are at a third of their strength; yours and your party's as bright as ever. You see your own in a guild war and on the market.

# uaRO Graphics 3.0.13-beta.19 — what's new

**See-through buildings - the whole house.** Only the parts of a house over the character went see-through before, its front and roof staying whole - a ghostly house. Now all of it before the character fades together; what is behind the character and the houses next to it stay as they are.

# uaRO Graphics 3.0.13-beta.18 — what's new

**See-through buildings - close walls and roofs over the feet too.** A wall the character stands right behind and a roof covering only its feet now go see-through as well (in 3.0.13-beta.17 they stayed whole).

# uaRO Graphics 3.0.13-beta.17 — what's new

**See-through buildings before the character** (a new switch «See-through buildings before the character» in the effects' Characters group, off by default). A house, a tree or a wall between the camera and your character glides half see-through and comes back once you have passed it. The character is no longer lost behind buildings in towns and trees in forests.

# uaRO Graphics 3.0.13-beta.16 — what's new

**A big shot: Ctrl+Alt+F11.** One frame of the game without the interface, drawn twice the screen's size (7680×4320 on a 4K monitor) with all its effects, into the game's ScreenShot folder as uaro-big-….png. The game draws the world at double size for a moment, so a short pause may show. With Direct3D 11 the shot is the screen's size, smoothed from the double one.

# uaRO Graphics 3.0.13-beta.15 — what's new

**Aura does not hide portals** (a new switch «Aura does not hide portals» in the effects' Characters group, off by default). A level 99 aura over a warp portal goes faint and the portal shows through it: with many aura characters at a portal it is no longer lost under them.

# uaRO Graphics 3.0.13-beta.14 — what's new

**The party's damage in the Hunt panel.** The Hunt panel now shows each party member's share of the damage since the hunt began (or since «↺»): «Party: Lina 60 % · you 30 % · Tor 10 %» - who gave how much on a boss. The blows seen on your screen count: a mate's off the screen do not.

# uaRO Graphics 3.0.13-beta.13 — what's new

**Who hits me** (a new switch «Who hits me» on the Interface page, off by default). The monsters that hit you in the last 6 seconds get a red frame round their HP bar, and a red ⚔ with their number shows over your character - seen at once when the crowd grows.

# uaRO Graphics 3.0.13-beta.12 — what's new

**Long chat lines.** The chat's field now takes up to 234 letters instead of 70. The server takes 70 at most, so a longer line goes as several messages in a row, cut between words - in the public chat, the party, the guild and whispers. A switch «Long chat lines» on the Interface page, on.

**The /p, /g, /w commands and «/ex» from the chat's menu** now really put their words into the field - the game skipped them silently before.

# uaRO Graphics 3.0.13-beta.11 — what's new

**Steady following of a player.** Shift+click on a player and the character follows them and keeps up when they stop: as soon as the leader is more than a cell away, the character runs after them again. It works with the game window behind another too - handy with two windows: a priest in the second window runs after you. A plain click on the ground or Esc in that window stops it; a leader out of sight for 15 seconds ends it by itself. A switch «Steady following of a player» on the Interface page, on.

# uaRO Graphics 3.0.13-beta.10 — what's new

**Esc closes the NPC's picture.** The big NPC picture that sometimes stays on the screen after a talk now goes away with Esc - the way the server removes it. A switch «Esc closes the NPC's picture» on the Interface page, on.

# uaRO Graphics 3.0.13-beta.9 — what's new

**The game's swear filter can be turned off** (a switch «The game's swear filter» on the Interface page; on, as in the game). The game refused to send a line holding a word of its list - even inside an ordinary word (computer, say). Clear the switch and every line goes.

# uaRO Graphics 3.0.13-beta.8 — what's new

**Screen shake can be turned off** (a switch «Screen shake» on the Interface page; on, as in the game). Clear it and Earthquake and similar skills no longer shake the camera - the picture stays still.

**No Frost Joke and Scream lines in the chat** (a switch «Frost Joke and Scream lines in the chat»; on, as in the game). Clear it and the skill works as always, but your character no longer says its joke line in the public chat.

# uaRO Graphics 3.0.13-beta.7 — what's new

**Quick teleport on the map** (a new switch «Quick teleport on the map» on the Interface page, off by default). Teleport and Fly Wing within one map without the darkening: the game no longer stops for a quarter of a second, the new place shows at once - handy when farming with wings. Moving to other maps stays as it was.

# uaRO Graphics 3.0.13-beta.6 — what's new

**Small windows for several games.** The uaRO program's window sizes now include 960×540, 800×600 and 640×480 - several games side by side on one screen. Nothing under 640×480: the game's interface would not fit.

# uaRO Graphics 3.0.13-beta.5 — what's new

**The camera lower, down to the ground.** A new choice «Camera tilt» on the game page: as the game (-28°), lower (-13°) or to the ground (-3°) - how low Shift with the right button (or the wheel) tilts the camera. As the game by default. In buildings it stays steeper; on another map the game brings back its usual tilt.

# uaRO Graphics 3.0.13-beta.4 — what's new

**Your own chat colours.** On the Interface page: the party's, the guild's, your own and the others' lines - blue, green, yellow, orange, pink, violet, white or «As it is». A GM's lines keep their colour. A new colour applies to the lines coming after the change.

**Chat log to a file** (a switch «Chat log to a file», off by default). Every chat line with its time goes into uaro-graphics\chat-logs, a file a day - handy for disputes over trades. Whispers too; the file stays on your computer and is never sent with a report.

# uaRO Graphics 3.0.13-beta.3 — what's new

**Boss HP bars** (a new switch «Boss HP bars» on the Interface page, off by default). An MVP's bar is wider and gold, a mini-boss's wider and violet - a boss stands out even in a crowd. Below a quarter of HP it turns red like any other.

# uaRO Graphics 3.0.13-beta.2 — what's new

**A lighter minimap.** It is redrawn at every step and was the dearest part of the new interface. Now the map's picture round you is made once and only shifted at a step - the minimap draws about 40 % faster, more noticeable on weak computers.

# uaRO Graphics 3.0.13-beta.1 — what's new

**Arrows on the side of the hotbar with room.** A hotbar in the screen's upper half shows the arrows left and the skill tips under it, and the bar itself goes up to the screen's edge. At the bottom of the screen it stays as before, over the bar.

**The NPC talk window is smaller and moves.** Its width follows the NPC's words (short lines, a narrow window; long ones, wider), and the window can be dragged by its title or its text.

**Experience in the chat once.** With the server's experience lines on, it showed twice: «Experience: Base … · Job …» and then «Base EXP …», «Job EXP …» again. Now only the server's line.

**MVP lines spaced:** «Your reward EXP Points are 29587 acquired!», «Your reward item is Blue Potion» instead of glued words.

**«Not enough SP»** in the interface's language.

**NPC talk:** the NPCs' leading spaces dropped - the text starts at the edge.

# uaRO Graphics 3.0.12-beta.3 — what's new

**Experience in the chat once.** With the server's experience lines on, it showed twice: «Experience: Base … · Job …» and then «Base EXP …», «Job EXP …» again. Now only the server's line.

**MVP lines spaced:** «Your reward EXP Points are 29587 acquired!», «Your reward item is Blue Potion» instead of glued words.

**«Not enough SP»** in the interface's language.

**NPC talk:** the NPCs' leading spaces dropped - the text starts at the edge.

# uaRO Graphics 3.0.12-beta.2 — what's new

**Arrows on the side of the hotbar with room.** A hotbar in the screen's upper half shows the arrows left and the skill tips under it, and the bar itself goes up to the screen's edge. At the bottom of the screen it stays as before, over the bar.

**The NPC talk window is smaller and moves.** Its width follows the NPC's words (short lines, a narrow window; long ones, wider), and the window can be dragged by its title or its text.

# uaRO Graphics 3.0.12 — what's new

**The mouse in the new interface with the controller off.** With «Gamepad (uaRO Pad)» cleared, the new interface's windows did not answer clicks: tabs, the close cross and buttons lit up but did not press. They work without the controller now; the controller itself stays off.

# uaRO Graphics 3.0.12-beta.1 — what's new

**The mouse in the new interface with the controller off.** With «Gamepad (uaRO Pad)» cleared, the new interface's windows did not answer clicks: tabs, the close cross and buttons lit up but did not press. They work without the controller now; the controller itself stays off.

# uaRO Graphics 3.0.11 — what's new

The biggest update yet: a new game interface, a reference inside the game, the controller in every window, a smoother
connection. The new interface is on for everyone; to go back to the old one, clear «New interface» on the Main page.

## Highlights

- **Monsters no longer get stuck far away and hit from a distance.** The cause was found: the server dealt the blow
  before the client showed it, and the monster stopped where it was not on the server. Now it catches up as it should.
- **A smoother connection.** Commands go to the server at once, without Windows' delay; the server's position fixes
  glide instead of teleporting; monsters and players no longer jump ahead after a lag. The ping on its own plate with
  a graph.
- **HP bars over monsters** - wider on the target; the monster under the cursor gets one too.
- **Things on the ground are named:** valuable ones (cards, gear, expensive items) always, the rest while Alt is held.
- **Boss timers.** A boss died before your eyes - a panel counts to its return by the server's rule and remembers it
  across game starts.
- **Grass, flowers and sun on green maps that used to be flat:** Mjolnir, the novice grounds, the guild castles'
  fields, Byalan Island, Alberta Island, Moscovia's forests and more. Prontera's benches no longer sway in the wind.
- **The camera remembers its zoom** between game sessions.
- **Our own AI for the homunculus and the mercenary:** follows you, defends, Lif heals. Another AI is never touched;
  the game uses ours after /hoai and /merai.
- **Crashes fixed:** entering a map, after a warp, on Print Screen.
- **Updates ride out GitHub hiccups** - they retry and continue the download.

## The new interface

- **Minimap and big map (Ctrl+M):** the way to a place, NPCs, Kafras, portals, the party, monsters; a «Scheme» look.
- **Character block (Alt+V):** HP/SP and experience in numbers, weight by the server's rules, regeneration standing
  and sitting, what the last fight cost; experience bars at the bottom and experience over a killed monster.
- **Target card, the party with faces and casts** (click a member - a skill on them), **buffs with timers**, **the
  cast over the hero**, **tracking** of quests and the way.
- **Hotbar:** each character's own, three sets (Ctrl+Shift+1/2/3), arrows and potions running out - the number turns
  red with a chime, the delay after a skill as a ring, the mouse wheel over a skill changes its level (Teleport 1 -
  at once, no menu).
- **Chat:** tabs and filters, whispers by person, links into the reference, Ctrl+F search, resizable.
- **Every game window in the new look:** items (tabs, «new», gear comparison, the site market's price, search across
  all characters' bags), stats, skills (tree and plan), equipment, storage, trade with scam protection, cart, shops,
  NPC talk, guild, pet, homunculus, mercenary, options, login, character select and creation - over forty windows.
- **Stats (Alt+A) explain themselves:** how each number is counted on your own numbers, ASPD and stat cost reference,
  a planner with goals, every class's buffs and a comparison on the current map's monsters.
- **Login screen:** an account shelf (one-click login), login with the site account, registration, server status,
  news; character select as a carousel with a portrait and where and when you played.
- **In a fight:** «What killed me» after a death, a «Hunt» panel (damage per second, experience and zeny per hour), a
  healer's alarm when an ally's HP is low.
- **Interface parts move and resize with the mouse** («Place interface» in the console), a layout per character if
  you like.

## The reference in the game (Ctrl+F)

Monsters, items, maps, NPCs, quests - with pictures, the data from uaro.kiev.ua. How to get to a map, the world map,
where a monster lives and where an item drops, bosses and MVPs, favourites and recent.

## Controller

Works throughout the new interface: a window tile on Start, moving items, NPC talk (menus, choosing an NPC with the
stick, numbers digit by digit), ground skills with an aim, walking with the stick and an arrow, a chat phrase wheel,
logging in.

## Settings app

- **The search finds:** in every mode at once, in any of the three languages, by word stems and synonyms («font»,
  «lags», «window mode»).
- **The Interface page in sections**, the windows behind one button; the in-game console (~) has the same sections.
- **The blue dot** by a value other than the recommended one - a click sets the recommended value. **★** - new since
  the update.

# uaRO Graphics 3.0.11-beta.305 — what's new

**The new interface and the reference are on for everyone by default.** Whoever did not turn «New interface» off themselves gets it together with the reference (Ctrl+F). To turn it off: Main → «New interface». Whoever turned it off before keeps it off.

# uaRO Graphics 3.0.11-beta.304 — what's new

**The blue dot no longer turns parts of the new interface off:** with «New interface» on, its parts (the target card, the panels, the windows) counted as «recommended off», and since 3.0.11-beta.303 a click on the dot turned them off. Their recommended values now follow «New interface». If something went missing, turn it on again on the Interface page (e.g. «Target card» under Panels on the screen).

# uaRO Graphics 3.0.11-beta.303 — what's new

**Marks in the settings:**
- the blue dot by a row whose value is not the one recommended for this computer is clickable now: a click sets the recommended value;
- a green star ★ marks a row added since the last update (shown until the app's next start; nothing is marked on the very first start).

**Main page:** «New interface» and «Text size» are here now - no need to look for them on the Interface page.

# uaRO Graphics 3.0.11-beta.302 — what's new

**The interface page in sections:** instead of 109 rows in one list - Main, Panels on the screen, Minimap, Fight, Things on the ground, Login and characters, Homunculus and mercenary, Connection and Windows in the new look. The 35 windows are behind a «Show each window» button. The controller's rows are on the Controller page (the windows' tile joined them).

**The in-game console (~, Alt+D):** the interface in the same sections - every row of it now, the fight, the login and the homunculus included; the windows open with one row. Typing letters in the console finds the interface's rows too, not only the effects.

# uaRO Graphics 3.0.11-beta.301 — what's new

**The settings search finds:**
- in every mode at once: Simple finds the Detailed mode's settings too - marked, and the button switches the mode;
- in any of the three languages;
- by the word's stem: «grasses» finds «Grass»;
- synonyms: font - text size, smoothness - smoothing, lags - frames and connection, mouse - cursor;
- the Screen, Gamepad and Play pages: «window mode», «vibration», «graphics test»;
- matches in a name first, then those only in a hint.

**Honest names:** «New minimap (prototype)» is now «New interface» - it turned the whole new interface on; «Reference» without «(prototype)». The Sound page shows «Swap left and right».

# uaRO Graphics 3.0.11-beta.300 — what's new

**Prontera's benches no longer sway in the wind:** the wrought bench is painted blue-green with the gaps between its bars cut out - the driver took it for a tree's leaves and swayed it with the grass. Only a yellow-green now counts as leaves (as all the client's real leaves are - checked over 867 textures of trees and outdoor props).

# uaRO Graphics 3.0.11-beta.299 — what's new

**Grass and sky on green maps the driver took for indoors:** Mjolnir (all 12 maps), the novice grounds, the guild castles' fields (Greenwood Lake, Luina, Valkyrie Realm, Nidhoggur Realm), Alberta Island, Glast Heim's outskirts, Jawaii, Byalan Island, Moscovia's forests, Prairie of Morestone, Odin Shrine, Capitolina Abbey, Good Turtles Village, Hvergelmir, Payon's PvP copies, Ile du Printemps, Poring War and the Monster Race Arena. They now get grass, flowers, butterflies and birds, the sun and its shadows, sky rays, morning fog and pollen like other fields. Where the ground is not very green (the novice grounds) there is less grass.

# uaRO Graphics 3.0.11-beta.298 — what's new

**Every class's buffs in the stats plan (Alt+A):** the «Buffs in the plan» row now has:
- for everyone - Blessing, Increase AGI, Gloria, Angelus, Impositio Manus, Wind Walk and the speed potions (Concentration, Awakening, Berserk Potion);
- class skills when learned or on now - Improve Concentration, Two-Hand Quicken, One-Hand Quicken, Spear Quicken, Adrenaline Rush (and for anyone with an axe or a mace), Loud Exclamation, True Sight, Fury.

What each gives is counted: stats, HIT, FLEE, CRIT, ATK, VIT DEF (Angelus) and attack speed - the best skill's (only with its weapon) plus the best potion's, as the server does. The results, the goals and the map's monsters follow the chosen buffs.

**Fixed:** with a speed potion or another haste on, the window could not tell the weapon, and the ASPD row and the ASPD goal disappeared.

# uaRO Graphics 3.0.11-beta.297 — what's new

**Buffs in the stats plan (Alt+A):** the planning tab has a «Buffs in the plan» row - Improve Concentration (when learned), Blessing and Increase AGI. A buff on the character now is marked «now». A click (or left/right on a controller) counts it in the plan or not. Improve Concentration counts as a percentage of the planned AGI and DEX, so its share grows with them. The results, the ASPD/cast goals and the map's monsters follow the chosen buffs. Passive skills (Owl's Eye, Vulture's Eye and the like) were counted before. The choice is kept with the plan.

# uaRO Graphics 3.0.11-beta.296 — what's new

**The stats window (Alt+A) — the rest of the concept:**
- **The map's monsters:** on the planning tab under the goals - up to five monsters of the map you are on: your chance to hit, to dodge and the blows to kill, now → with the plan (better green, worse red). By the server's rules, as the monster card.
- **The plan is kept:** each character's plan (level, stats, ASPD goal) stays after the game restarts.
- **The controller on every tab:** LB / RB the tabs; on the planning tab up and down the row (level, stats, ASPD goal), left and right less or more, Y resets the plan; on the reference tab left and right the class.

# uaRO Graphics 3.0.11-beta.295 — what's new

**Auto Spell in the new look:** the Sage's Auto Spell list now shows in the same card as the other skill lists: a row per skill with its icon, «Choose» and «Cancel», a double click chooses at once, the D-pad and buttons on a controller. The game's old window is hidden.

# uaRO Graphics 3.0.11-beta.294 — what's new

**Monsters should no longer be drawn over the chat:** when the game drew part of the world (monster sprites) among its own windows, our code lifted them over our panels together with the game's windows - a crab and a ghost ended up over the chat. Now only the game's windows and its cursor are lifted; the world stays under our panels. Each such case is written to the log.

**The monster card by the cursor is off by default:** with a controller it hung by the target all the time. It can be turned on in the interface settings.

# uaRO Graphics 3.0.11-beta.293 — what's new

**No per cent plate by the monster:** the dark plate with the target's HP per cent (e.g. «95%») to the right of its bar is no longer shown. It can be turned back on in the interface settings («HP per cent by the target's bar»). The HP bars over monsters stay as they were.

# uaRO Graphics 3.0.11-beta.292 — what's new

**Even frames under Windows scaling (test):** a new switch «Window with Windows scaling (test)» on the quality page, after «High resolution». At 125–175 % Windows scaling the game's window came out a pixel short of the monitor (3839×2159 of 3840×2160) and Windows composed every frame itself - on a 144 Hz monitor that is jumps between 72 and 48 frames. With the switch the game's window tells Windows it follows the scaling itself, covers the monitor pixel for pixel, and the picture goes to the screen directly. Takes effect after the game's restart. Off by default. If the mouse starts missing, turn it off and send a report.

The areas of our panels that keep clicks from the game are now told again in the window size they are checked in.

# uaRO Graphics 3.0.11-beta.291 — what's new

**Updates ride out GitHub hiccups:** when the download server briefly answers with an error (503), does not answer or drops the connection, the app tries three more times (after 2, 4 and 8 seconds) and continues the file where it stopped, instead of failing at once.

# uaRO Graphics 3.0.11-beta.290 — what's new

**uaRO AI for the homunculus and mercenary:** our own AI, put by the driver into the game's AI\USER_AI folder (never over another AI): it follows you, defends you and itself, attacks by itself if you want, Lif heals you under half HP, Filir and Vanilmirth open a fight with their skill. The game's commands (move, attack, stop, follow) work. Settings under «Interface»: «uaRO AI for the homunculus and mercenary», «AI: attacks by itself» (off), «AI: defends you», «AI: uses skills». For the game to use it, turn on «Own homunculus AI (/hoai)» and «Own mercenary AI (/merai)».

# uaRO Graphics 3.0.11-beta.289 — what's new

**Skill level by the wheel on the hotbar:** point at a skill on the hotbar and turn the mouse wheel — the level it is used at goes from 1 up to the one learned («Lv» in the tip; its third line says «wheel — level»). For example, Teleport can be set to level 1 — it warps at once, without the choice menu. The level is kept like the rest of the bar.

# uaRO Graphics 3.0.11-beta.288 — what's new

**Hotbar profiles:** each character now has three hotbar sets — Ctrl+Shift+1, 2, 3 switch between them (the current one is kept). A new set starts as a copy of the current one. The set's number (2 or 3) shows by the bar's toggle. Works with the per-character hotbar (on).

**Layout per character:** a new switch «Layout per character» under «Interface» (off): the places and sizes of the interface's parts are kept for each character separately; until you change them for a character, the common ones are used.

# uaRO Graphics 3.0.11-beta.287 — what's new

**Market price in tips:** in the items window, the storage and the cart, the tip over an item shows the site's market: «Market: from 12 000 z · 3 lots» — the cheapest active lot (per piece) and how many lots. The same data as the in-game reference, refreshed every few minutes.

# uaRO Graphics 3.0.11-beta.286 — what's new

**Search across characters:** the game now remembers what lies in each of your characters' bag and cart and in the account's storage (as last seen). When you search in the items window, the storage or the cart, a line at the bottom shows where else such things are: «Also: Storage ×12 · Anna ×3 · Boris, cart ×1». Open each character's items window once (the storage — open it) so the game learns its things.

# uaRO Graphics 3.0.11-beta.285 — what's new

**Gear compare:** in the items window the tip over a weapon or an armour now has a third line comparing it with what is worn there: «Instead of +4 Knife: ATK 25 → 31 (+6)», DEF for armour, and the slots when their number differs. Worked out from the server's numbers with the refine (without cards and special effects). With nothing worn there: «Nothing worn there: ATK 31».

# uaRO Graphics 3.0.11-beta.284 — what's new

**Chat search:** click into the chat's field and press Ctrl+F — the field becomes «Search»: what you type is looked for in the open tab's lines (in the words and the speaker's name), regardless of case. Backspace erases, Esc or Enter close it, so does Ctrl+F again. Outside the chat's field Ctrl+F still opens the in-game reference.

# uaRO Graphics 3.0.11-beta.283 — what's new

**Item names on Alt:** hold Alt and every item on the ground gets a small plate with its name and amount. Switch «Item names on Alt» under «Interface», on.

**Valuable drops:** valuable items on the ground are always named, without Alt: cards purple, weapons and armour blue, others worth 10 000 zeny or more gold. Switch «Valuable drops» under «Interface», on.

# uaRO Graphics 3.0.11-beta.282 — what's new

**Healer's alarm:** when someone of your party on your map falls under 30 % HP, a short chime sounds and their row on the party panel pulses red for 3 seconds. Switch «Healer's alarm» under «Interface», on (works with the party panel).

# uaRO Graphics 3.0.11-beta.281 — what's new

**Potions running low:** healing items on the hotbar are now watched like arrows: a soft chime when 10 are left and a lower one when they run out; the count on the cell turns red from 10 down. Switch «Potions running low» under «Interface», on.

# uaRO Graphics 3.0.11-beta.280 — what's new

**Boss timers:** when a boss dies in front of you, a «Bosses» panel shows on the right: name, map and the time left until it returns, by the server's rule (exactly its spawn delay after death). Once the time is up it shows «+» and how long ago. A click on a row removes it. Timers are kept between game starts. Switch «Boss timers» under «Interface», on; the panel's place is in the interface layout («MVP»).

# uaRO Graphics 3.0.11-beta.279 — what's new

**Monster card by the cursor:** point the mouse at a monster (or pick a target with the controller) and a card shows next to it: name and level, HP, element, race and size, your chance to hit it, its chance to hit you, its blow after your DEF and its damage per second, your blow and how many blows it takes. Worked out by the server's rules (without element, size and cards). Switch «Monster card by the cursor» under «Interface», on.

- **Hunt.** A new panel over the game: damage per second (the last 10 s and the last minute) and each skill's share, experience per hour as a share of the level and the time to the next level, zeny per hour, how many monsters and which were killed. The round arrow in its corner starts counting anew. Turn it on in «Interface» → «Hunt panel»; its place and size are in the interface layout («Hunt»).

# uaRO Graphics 3.0.11-beta.277 — what's new

**Feature 8 — «What killed me»:** the death screen lists the last hits of the 10 seconds before it (up to six, the last on top): who, by which skill or a plain attack, how much damage and how long before the death, with all the damage together in its title. The switch «Що мене вбило» in «Інтерфейс».

# uaRO Graphics 3.0.11-beta.276 — what's new

**Feature 7 — the delay after a skill on the hotbar:** after a skill the hotbar's skills darken and clear clockwise until the server allows the next one. The time is the server's for the skill's level, cut by DEX as on uaRO (× (150 − DEX) / 150). The switch «Затримка після вміння на панелі» in «Інтерфейс».

# uaRO Graphics 3.0.11-beta.275 — what's new

**Feature 2 — the character select as a carousel:** the chosen character in the middle and larger, two neighbours each side smaller. ‹ ›, the wheel, the D-pad or a click on a neighbour go round all the account's places (all the game's pages together), «N / M» at the top. A double click on the middle one enters. The switch «Вибір персонажа каруселлю» in «Інтерфейс» (off: the three places as before).

# uaRO Graphics 3.0.11-beta.274 — what's new

**Feature 4 — the session's summary:** the character select shows under the places the chosen character's last game: how long, how much of a Base and a Job level gained (per cent of a level), the zeny won or spent. For example: «Остання гра Ліра: 1 год 20 хв · Base +40 % · +12 500 z».

# uaRO Graphics 3.0.11-beta.273 — what's new

**Characters — new features 1 and 3:**
- **Character portraits:** now and then, while the character stands with no window open, its look in the game (hair, headgear, gear) is kept and shown on its place in the character select instead of the plain head. The first a minute into the game, then at most once in 10 minutes. The switch «Портрет персонажа» in «Інтерфейс».
- **When and where played:** the character select's place shows the map it was last on and «2 год тому» by its levels. Only this computer's (uaro-graphics\characters.txt).

# uaRO Graphics 3.0.11-beta.272 — what's new

**Fixed a crash after a warp (R-EP778D):** the smoothing of other units' moves kept a monster's address from the map before; after the warp its memory was freed but not yet written over, and the driver called a function through garbage. Each unit is checked now — of this map and truly the game's object — or left alone.

# uaRO Graphics 3.0.11-beta.271 — what's new

**The guild notice is edited in the new look:** on «Оголошення», «Змінити» opens the fields «Тема» and «Текст» right on the card (no old game window). A click or Tab — the other field, Enter or «Зберегти» — send, Esc or «Скасувати» — no change. The lengths as the server's (the subject 59, the text 119 bytes).

# uaRO Graphics 3.0.11-beta.270 — what's new

**From the tester's list:**
- Descriptions of skills several jobs have (Cure, Heal, Divine Protection and others — 29 lines in the client's table): instead of «4Skill Requirements…, 8Skill Requirements…» only your job's line; when yours is not there — each with its job's name («Acolyte: …»).
- The party window and «Налаштування групи» are dragged by their empty part.
- The party window's map without «.rsw» («pay_dun01»).
- «Налаштування групи» of a party already made: the experience's sharing alone; the things' sharing is set only when the party is made.
- The Alt+D console: the mouse cursor shows over it now.

# uaRO Graphics 3.0.11-beta.269 — what's new

**«Характеристики», stage 5 — «Цілі» in «Планування»:**
- The ASPD you aim at (‹ › by one, up to 190): the AGI it takes with your planned DEX and the points still needed — or that the base stat up to 99 cannot reach it (only skills or things).
- «Без касту»: the DEX to 150 (with bonuses) and its points, or how much more things or buffs must give.

# uaRO Graphics 3.0.11-beta.268 — what's new

**«Характеристики», stage 4 — the «Планування» tab** (between «Персонаж» and «Довідка»):
- «Рівень плану» ‹ › — from your level to 99 (Shift: by 10); the points that level brings, what the plan takes, and whether they suffice.
- Each stat has − / + (Shift: by 10): «70 → 73» and its price in points.
- At the right «БУЛО → СТАНЕ»: ATK, MATK, HIT, FLEE, ASPD (to a tenth), CRIT, Max HP, Max SP, the cast, the weight — the server's numbers now and what they become by the formulas, things and buffs as they are.
- The plan only shows: nothing goes to the server, you put the points yourself with «+» on «Персонаж». «Скинути» brings the plan back to the character.

# uaRO Graphics 3.0.11-beta.267 — what's new

**«Характеристики», stage 3 — the «Довідка» tab:** the window's title has the tabs «Персонаж» and «Довідка». In «Довідка»:
- the class (‹ › go through any class of the server, yours first);
- ASPD for each of the class's weapons at AGI 1 / 50 / 80 / 99 and at your AGI (with your DEX), your weapon lit;
- the points a stat costs from 1 to 50, 70, 80, 90, 99;
- the job levels giving each stat its +1 (and how many you have already).

# uaRO Graphics 3.0.11-beta.266 — what's new

**«Характеристики», stage 2 — «Як рахується»:**
- New rows: «Затримка після вміння», Max HP, Max SP, «Відновлення за тік», «Вага»; hits a second beside ASPD.
- The panel «Як рахується» always open at the window's bottom: point at any row (a stat or a battle number) — the server's formula on your character's numbers, what the server says and how much things, buffs and skills add, what the next point gives.
- A click on a battle row keeps it in the panel (another click lets it go).
- For ASPD: the class and the weapon with its delay, the count, hits a second and the AGI to the next ASPD.

# uaRO Graphics 3.0.11-beta.265 — what's new

**«Характеристики», stage 1 — checked against the server (R-VZ69Q2):** the Max HP and weight formulas follow the server's numbers now (HP: the 35 added after VIT's per cents; weight by the base STR). The rest matched. A report from a transcendent character (Alt+A open) helps the next check.

# uaRO Graphics 3.0.11-beta.264 — what's new

**«Характеристики» with explanations, stage 1 (nothing visible yet):** the driver now has the server's formulas and every class's numbers (HP, SP, weight, weapon delays, job bonuses, points per level). Opening «Характеристики» logs a check: what the server sent against what the formulas give. To check it: a character with no gear and no buffs, open Alt+A and send an in-game report.

# uaRO Graphics 3.0.11-beta.263 — what's new

**Skill lists in the new look** — all the client has, not only Teleport: weapon repair, weapon upgrade, forging and potions (Forging, Pharmacy), arrow crafting, cooking, the pet incubator. A row per choice, «Вибрати» and «Скасувати», a double click chooses at once. Only Auto Spell stays as it was (it lists skills, not things).

**Party members' portraits:** a party member who is also in your guild gets a portrait on the map and minimap from the guild's list, even before you have seen them nearby.

# uaRO Graphics 3.0.11-beta.262 — what's new

**Minimap:** in a small game window the marks change size with the zoom more visibly — the smaller the window, the more (up to twice), so the difference shows on a small minimap too.

# uaRO Graphics 3.0.11-beta.261 — what's new

**From the tester's second list:**
- Overweight: the «90%» icon is in the new style now too, like «70%».
- Buffs placed in the middle of the screen keep their top edge and grow down only, not over the map; what does not fit down to the bottom goes into a second column («Бафи, що не вміщаються» → «У другий стовпчик») or under «N more». Buffs moved before need dragging once more.
- Skill menus (Teleport, Warp Portal) in the new look: the skill's name, a row per choice, «Вибрати» and «Скасувати», a double click chooses at once.
- After «Увійти» in the character select, the «Вхід у гру…» veil until the map shows, not the game's old loading window.

# uaRO Graphics 3.0.11-beta.260 — what's new

**From the tester's list:**
- Class portraits on the login screen: every class has its own now — the novices, the first and second jobs, Taekwon. The second jobs used to show the transcendent portrait, Taekwon and the first jobs only the head.
- The Alt+D console: an opaque ground; rows take mouse clicks (a slider or a choice: the left half less, the right half more), the wheel scrolls; Alt+D again closes it, as Esc does.
- «Емоції» (Alt+L): the card is dragged by any place outside the emotions.
- The small cards (emotions, commands, chat room, pet, homunculus, guild, the game's questions and others) follow «Непрозорість вікон» too, no longer always nearly opaque.
- The big map acts as a window: opened or pressed, it lies over «Речі» and the NPC talk; press in a window or open a new one and that one is over the map again.

# uaRO Graphics 3.0.11-beta.259 — what's new

**Talking to NPCs with the pad, stage 5 — hold to page on:**
- Hold A on an NPC's text, and the «Далі» pages turn one after another.
- Paging stops at a menu, a number field or «Закрити». A held A never chooses or closes: the decision stays yours.
- An A held from before the talk opened turns no pages.

# uaRO Graphics 3.0.11-beta.258 — what's new

**Talking to NPCs with the pad, stage 4 — numbers by their digits:**
- When an NPC asks for a number, ←/→ pick the digit (units, tens, hundreds…) and ↑/↓ add or take away one of that digit. 1,250 takes six presses instead of nineteen.
- LB sets 0, RB puts in the number you entered last time.
- The hint line at the bottom shows which digit is changing («↑ ↓: ±100»). It also shows over the talk window while the number field is open.

# uaRO Graphics 3.0.11-beta.257 — what's new

**Talking to NPCs with the pad, stage 3 — NPCs across the screen:**
- The Talk button now reaches NPCs up to 20 cells away, about the whole visible screen. It was 15 before. The right stick picks distant NPCs the same way.
- No need to walk up: the uaRO server accepts talking from far away, so the talk starts right where you stand. The pad does not take NPCs beyond the screen.

# uaRO Graphics 3.0.11-beta.256 — what's new

**Talking to NPCs with the pad, stage 2 — you can see who you will talk to:**
- **A plate over the NPC.** When you play with the pad, a small plate floats over the NPC the Talk button (↓) will take. It shows the button's glyph and the NPC's name, or «Talk» until the server has sent the name.
- **Other NPCs nearby** get a faint gold dot over their heads: you can pick them too.
- **The right stick moves the plate.** Tilt it toward another NPC and the plate moves there. The choice stays while that NPC is within reach, and ↓ talks to it.
- **In a fight the plate is hidden** so it does not get in the way. ↓ works as before.

# uaRO Graphics 3.0.11-beta.255 — what's new

**Talking to NPCs with the pad, stage 1 — menus:**
- **The D-pad moves the way the menu looks.** Options sit in two columns: ↑/↓ move a row up or down, ←/→ move to the other column. LB/RB page by 8 options.
- **Remembered choices.** At the same NPC's same menu, the option you chose last time is already selected and marked «last time». A Kafra teleport to the same town is A, then A. Each character has its own memory. It works with the mouse and keyboard too. Switch: «Пам'ятати вибір у меню NPC» under «Інтерфейс».
- **No accidental A.** For the first 0.4 s after a new page or menu, A does nothing, and the A glyph at the bottom of the window is dimmed meanwhile. An A already held when the window opened (attacking, picking up) is not passed to it.

# uaRO Graphics 3.0.11-beta.254 — what's new

- **Pad buttons light their hotbar cells.** Before, only a cell used from the keyboard (F1–F9 or the battle-mode keys) showed a short press. The pad uses skills and items directly rather than through the keys, so the hotbar never knew. Now a pad button lights its cell the same way, ground-aimed skills included.

# uaRO Graphics 3.0.11-beta.253 — what's new

- **The pad keeps your monster when you run away from it.** The target used to drop as soon as it was more than 15 cells away, and the next A took another monster. An archer who runs off and shoots again got switched to someone else. Now a monster you are already fighting, or picked with the right stick, is kept up to 24 cells. A still takes a new monster within 15 cells, as before.

# uaRO Graphics 3.0.11-beta.252 — what's new

- **Archers shoot distant monsters from the pad again** (report R-3T958C). When a monster is beyond the bow's reach, the server asks the game to walk the character up, and the game starts walking. Every new press of A sent the attack again, and the server stopped the walk on it, so the character stood still and never shot. Now pressing A again on the same target while the character walks up to it leaves the walk alone.

# uaRO Graphics 3.0.11-beta.251 — what's new

- **Monsters no longer get stuck far away and hit from a distance** (reports R-AMNV99, R-GKXJT6, R-RWR2Z8, R-UCE7J8, R-YB4S2K).

  **Cause.** The server counts a hit at once, and the monster walks on right after a short flinch. The game shows the same hit later, after the swing. Its flinch then cuts off the new step, leaving the monster standing on its old spot. The game carries out the server's «stand here» only for a walking monster, so that order was lost too.

  **Fix.** If a monster stands for more than 0.3 s away from where the server put it, the client gives the game its step again. The monster slides over to its real place and fights from there.

  Switched off together with «Плавні поправки позиції» (Smooth position fixes) under «Інтерфейс».

# uaRO Graphics 3.0.11-beta.250 — what's new

- **A longer «monster stands far away» record.** Report R-RWR2Z8 showed the monster started walking after the character before the record began. The log now keeps 8 seconds instead of 3 and the steps of every monster, not just the ones you fight. Each line also has the character's cell.

# uaRO Graphics 3.0.11-beta.249 — what's new

- **A fuller record for «the monster stands far away and teleports»** (report R-GKXJT6). There, the game showed a monster on one cell for 6 seconds while the server had it next to the character. The game ignored even the server's «stand here». Now the log also has every other packet about that monster, how many copies of it the game holds, and where the game moves it over the next 3 seconds. If the glitch comes back, send a report right after it.

# uaRO Graphics 3.0.11-beta.248 — what's new

- **A record for «the monster hits from afar».** When a monster hits the character from 2+ cells away from where the game shows it, its last 3 seconds go to the log.
- The log also notes when the game cuts a monster's smooth position fix short. The monster may then stay on its old cell.
- To compare: switch off «Плавні поправки позиції» (Smooth position fixes) under «Інтерфейс» and see whether «hits from afar / teleports» remains.

# uaRO Graphics 3.0.11-beta.247 — what's new

- **A record for «the monster teleports».** When a monster you fight jumps, the log gets everything the server said about it in the last 3 seconds: its steps, hits and stops, and where the screen showed it at each moment. A jump counts when the server puts the monster 2+ cells from where the game showed it. If you see such a jump, send a report right after it (report R-AMNV99).

# uaRO Graphics 3.0.11-beta.246 — what's new

- **Experience with the hit.** The server counts a hit at once, but the game shows it only after the character's swing. Because of that, «+… Base» over the monster and by the bars appeared before the monster fell. Now the label waits for the swing and appears when the game shows the kill (report R-Q2C54N).

# uaRO Graphics 3.0.11-beta.245 — what's new

**Chat:**
- **Scroll bar.** A thin bar now runs to the right of the chat history. Drag its thumb with the mouse, or click the bar to jump to that place. The wheel works as before.
- **Server lines.** The server's own messages, such as «Autolooting items…», are now shortened like other system lines. They go to the system lines instead of the public chat.

**«Навички» (Skills):**
- **Draggable scroll bars.** In the tree, the bottom and side scroll bars can now be dragged with the mouse and clicked. The list's bar works the same way (report R-YCYYXU).

# uaRO Graphics 3.0.11-beta.244 — what's new

**The chat, stage five:**
- **Links to «Reference».** In «Picked up: …» lines the thing's name and in «… defeated» the monster's are links: point (underlined), click - its card opens in «Reference».
- **Copy a line.** A right click on any chat line - «Copy the line». On a name the menu now has four: «Whisper», «Block whispers», «Copy the name», «Copy the line».

# uaRO Graphics 3.0.11-beta.243 — what's new

**The chat, stage four:**
- **«All»'s filters.** On «All», by the size grip - a gear. Its menu: «The server's lines», «Picked up and experience», «Announcements», «Trade» - untick one and it leaves «All» (its own tabs keep it). The choice is kept; the gear turns gold while something is hidden.
- **Repeats.** The same line again within a minute is not doubled - it gets «×2», «×3»…
- **The server's lines shorter:** «Autoloot: up to 100.00%», «Autoloot off», «Experience shown», «Zeny shown»; «Experience Gained Base:… Job:…» → «Experience: Base +1 442 · Job +952» (and it is now among the picked up and experience, and on «Battle»).
- **A line's time.** Point at a line - its time («14:32») at its right.

# uaRO Graphics 3.0.11-beta.242 — what's new

**The chat, stage three:**
- **Whispers by person.** On «Whisper», under the tabs, a row of names (the latest first), each with its unread. A click on a name shows only that talk and the input goes to that person; «All» - everyone again.
- **A name's menu.** A right click on a name in any chat line: «Whisper», «Block whispers» (puts the game's own «/ex Name» into the field - press Enter), «Copy the name».

# uaRO Graphics 3.0.11-beta.241 — what's new

**The chat, stage two:**
- **Its size by the mouse.** A grip (two slants) at the chat's top right corner. Drag it: 360 to 900 pixels wide, 4 to 20 rows high. The size is kept. F10 still goes round 7 / 12 / 4 rows (and drops the corner's height).

# uaRO Graphics 3.0.11-beta.240 — what's new

**The chat, the first stage of its new concept:**
- With no new lines the chat no longer hides its tabs and input - only its ground fades. It no longer jumps under the mouse; the unread counts always show.
- The switch «The chat fades when idle» (Interface): «After 12 s» (as before), «After 30 s», «Never».
- Commands in the input: `/p words` - to the party, `/g words` - to the guild, `/w Name words` - a whisper (a name with spaces in quotes), `/r words` - answer the last who whispered. The channel stays as it was after sending.
- Type `/` - the commands show over the input.

# uaRO Graphics 3.0.11-beta.239 — what's new

- **The character block: the experience gained on the Base / Job rows too.** After a kill the block's own experience bars light the part gained in yellow for 4 seconds, and the row says «+1 234 (0.05 %)» in the pace's place. Job the same.

# uaRO Graphics 3.0.11-beta.238 — what's new

**The character block (Alt+V):**
- **The regen as the server gives it, standing and sitting.** The line under HP and SP now says how much a minute and the pose: «+3 872/min · sitting». Counted: sitting, the natural regen twice as often (each 3 s, not 6), Increase HP / SP Recovery and Spirit of the Blade (each 10 s), Meditatio, a Monk's Spiritual Cadence while sitting. The tip splits it: natural and from skills.
- **What the last fight took.** After a monster dies, the HP and SP bars show in yellow for 4 seconds how many per cent this fight took (from the first hit to the monster's death), and «last fight −12 %» at the right.

# uaRO Graphics 3.0.11-beta.237 — what's new

- **Experience over a killed monster**: a monster you hit dies - «+1 234 Base · +567 Job» rises over where it stood and fades in a second and a half (gold, dark outline).
- **By the experience bars at the bottom** for three seconds: how much came, with its part of the level - «+1 234 Base (0.05 %) · +567 Job (0.2 %)». Several kills in a row add up. Experience from the party or a quest - by the bars only.
- The switch «Experience over a killed monster» (Interface), on at first.

# uaRO Graphics 3.0.11-beta.236 — what's new

- **Experience bars at the bottom of the screen** (the concept «Character block»): two thin bars along the bottom edge - Base over Job, ticks each 10 %. Point at them for the per cent, the numbers, the pace an hour and the time to the level. Gone at the last levels. The switch «Experience bars at the bottom» (Interface), on at first.
- **Tips on the rings' dots** (the compact «Rings» look): gold - «+3 points» of free stats (a click opens «Status»), orange or red - the weight and what it stops.

# uaRO Graphics 3.0.11-beta.235 — what's new

**The character block (Alt+V), full view - more information** (the concept «Character block»):
- Under the name, the guild and the party («Party Maze · 3/4» - how many in the game of all).
- The weight as a bar with marks at 70 % (no HP/SP regen) and 90 % (no attacks); the exact numbers in its tip.
- Under HP and SP, the regen: «+88 every 6 s» and «full in ≈ 2 min» (the server's formulas). From 70 % weight - «no regen: overweight».
- An ATK · DEF · FLEE · ASPD line; a click on it opens «Status».
- At the last levels, in place of the empty experience rows - «Session»: time in the game, zeny this session and an hour, kills (monsters you hit), deaths.
- **Tips** on everything: point at the weight, zeny, guild, points, regen, each battle number or session cell - what it is and where it comes from.
- A click on the block is taken where the game's cursor is (as in the other windows).

# uaRO Graphics 3.0.11-beta.234 — what's new

- **Quick clicks on the interface's parts are no longer lost** (R-G4QKMZ: the buffs' names button «shows, then doesn't»). A press on a part the mouse can move waits for the button's release to tell a click from a drag. When a game frame fell right between the release and Windows' message about it, the press was forgotten. Now it waits for the message; a drag starts only with the button down. The buffs' button, the minimap, the character block, the party and the other parts.

# uaRO Graphics 3.0.11-beta.233 — what's new

- **The buffs' round names button (three lines) works every time.** The press was taken from the point Windows sends, the hover from the game's own cursor; on a scaled 4K screen they parted and the second or third press missed. Now the press is where the game's cursor is, as in every other window. A click on an icon (favourites) the same.

# uaRO Graphics 3.0.11-beta.232 — what's new

- **The buffs' round names button works on every press.** A quick second press came from Windows as a double click and the button let it pass; a click on an icon (favourites) the same.
- **Unknown effects have names** (R-DS774A): Auto Berserk showed as «#132». Now the name comes from the server's skill giving the effect, for every such icon (effects without a time, toggles).

# uaRO Graphics 3.0.11-beta.231 — what's new

**Buffs and debuffs without the black plate** (the concept «Buffs and debuffs», variant C, «Column» and «Row»):
- The icons stand straight on the picture at the right edge, as the game's own: no common column, no rings, just a thin dark outline and a shadow.
- The time gone darkens the icon clockwise from twelve. The numbers always show, on the icon's foot, with a dark outline. The last 10 seconds in orange.
- Debuffs in a red outline, last.
- Instead of the «‹ Names» plate, a small round button over the icons (three lines); point at it for «Names» / «Icons». The names stand left of the icons, on the picture too; the icons do not move.
- The icons a little larger: favourites 36, the rest 30 pixels.

# uaRO Graphics 3.0.11-beta.230 — what's new

**«Skills» (Alt+S): the skill's panel in «List» too**
- Right of the list stands the same panel as in «Tree»: the level, what it does or why it is closed, «Needs» with your levels, «Opens», the points to it.
- New in the panel: «SP by level» (every level, yours in gold, planned ones in blue) and the game's description of the skill, as much as fits.
- The window is always eight rows high, so the panel has room.
- Controller: RT switches «List» / «Tree» (the choice is kept).

# uaRO Graphics 3.0.11-beta.229 — what's new

**«Skills», the «Tree» view redone** (R-EQDQWH: «a tree nobody can make sense of»):
- On the right, a panel of the skill pointed at (or the controller's): its level, what it does or why it is closed, «Needs» - every requirement with your level (✓ or «0/5»), the earlier jobs' too, «Opens» and the points to it.
- No more column of the earlier job's chips on the left and no numbers on the links. Links only between the tab's skills, through the gaps, never under another skill. Point at a skill: its chain turns gold, the other links fade.
- Closed skills read better: brighter name and icon.

**The skill's description card** (R-ZUY4CM: «the description under the window»):
- A right click on a skill in «Skills» always shows the card over the window, also when it was open for another skill.
- A skill the game has no description of (like Parry) shows what is known: «The game has no description of it», its kind, levels, SP by level, requirements.

# uaRO Graphics 3.0.11-beta.228 — what's new

- **Print Screen no longer crashes** (R-P6U4YJ): at some sizes of the game's window (a width not divisible by 4) the screenshot read past the picture's end and the game fell. Now it is saved at any size.
- «Skills»: skills the client's name table lacks show the server's name instead of «Skill not defined !!»; a right click on one no longer opens an empty «Unknown-Skill» card.

# uaRO Graphics 3.0.11-beta.227 — what's new

**«Skills» (Alt+S): the controller in the tree and the plan** (the fifth stage of the new skills window):
- In «Tree» the D-pad (or the left stick) goes to the nearest skill the way it points; the tree scrolls to it.
- LT turns «Plan» on and off. In the plan Y adds a planned level, X takes one off, Start is «Apply». Without the plan Start closes the window as before.
- A, X (take onto the bar), LB / RB and B work as before.

# uaRO Graphics 3.0.11-beta.226 — what's new

**«Skills» (Alt+S): the «Next» tab** (the fourth stage of the new skills window):
- The last tab holds the new skills of the job you may change to next. With two choices (Swordsman → Knight or Crusader, Taekwon → Star Gladiator or Soul Linker) two buttons stand over the list; the window remembers which one you looked at. With one (Knight → Lord Knight) it is in the tab's name: «Next · Lord Knight».
- The requirements are marked by your current levels: Spiral Pierce waits for Spear Mastery 10. All met - «After the job change».
- Works in «Tree» and «Plan» too: what you plan for the next job is kept and applied after the change.
- A Novice has no such tab.

# uaRO Graphics 3.0.11-beta.225 — what's new

**«Skills» (Alt+S): «Plan»** (the third stage of the new skills window):
- A «Plan» button in the title. In the plan «+» asks the server nothing - it sets a blue level; «−» takes it off with whatever needed it.
- Requirements count by the planned levels: plan Pierce 5 and Spear Stab right away, even a closed one.
- At the bottom «Free points: 3 · planned: 7» and the buttons «Reset» and «Apply». «Apply» raises the skills one by one, each once the server confirmed the one before. If the server does not let a skill rise (say, the job before's points come first), the plan stops and the line over the list names the skill.
- The plan is kept between games, per character, and shows in blue in the list and the tree even with «Plan» off.

# uaRO Graphics 3.0.11-beta.224 — what's new

**«Skills» (Alt+S): the «Tree» view** (the second stage of the new skills window):
- The window's title has the buttons «List» and «Tree». The list opens first, as before; the choice is kept.
- «Tree»: the job's skills in columns by the depth of their requirements, a line from each requirement with the level it needs (gold - met, red - not). The requirements from the job before are chips on the left with your level.
- Point at a skill: its whole chain of requirements lights up, and a line at the window's bottom says «To Spear Stab: 6 more points».
- «+», a double click, dragging onto the bar and a right click (the description) work as in the list.
- The tree scrolls with the wheel (with Shift - sideways) or by dragging its empty ground.

# uaRO Graphics 3.0.11-beta.223 — what's new

**«Skills» (Alt+S)**: every skill of the job shows, not only the open ones (the first stage of the new skills window):
- The skills the server has not sent yet sit on their job's tab after the open ones - dimmed, without «+», a gold line over them.
- Each says why it is closed: «Needs: Twohand Quicken 10» (only what is still missing), «Quest - not learned with points», «Married characters only», «Only under a Soul Link».
- After a job change, while the points of the job before are not spent, a line over the list: «Knight skills open once the free points go into Swordsman skills». A Novice sees «A job change needs Basic Skill 9».
- A right click on a closed skill shows its description, as on an open one.

# uaRO Graphics 3.0.11-beta.222 — what's new

**The minimap and the big map** (the tester's list):
- A guild mate's triangle is now pink - the colour the game itself marks them with.
- A new switch, "Map marks as in the game" (Interface): guild mates and party members as round dots in the game's colours instead of the triangle and the faces. Off at first.

# uaRO Graphics 3.0.11-beta.221 — what's new

**The homunculus** (the "Homunculus card" switch, Alt+R):
- The homunculus' skills are now listed on the card itself: each one's name and level and the skill points left. The game's own skills window did not know their names.
- A "+" by a skill that can rise while points are left.
- A click on a skill's name uses it - as the game's own skills window does.

# uaRO Graphics 3.0.11-beta.220 — what's new

**The guild - all six tabs in the new look** (the "Guild card" switch, Alt+G):
- "Positions": each position's name, rights ("Invite", "Expel") and tax. "Change" opens the game's own window to rename positions or change rights and tax; another tab brings the card back.
- "Skills": the guild's skills with their levels and the points left; a "+" by a skill that can rise (the guild master sees it).
- "Expelled": name, account and reason.
- "Notice": its subject and text. "Change" opens the game's window to write a new one.

# uaRO Graphics 3.0.11-beta.219 — what's new

**Controller** (report R-4DDZ4R):
- Pick a far monster with the stick and kill it with one shot while A is still down - the game no longer moves on to a nearer one by itself. Only A held for 0.4 s and longer goes on to the next monster.

**"Tip of the day"** (the tester's list):
- The tips are now in the interface's language - Ukrainian, Russian or English - instead of the client's Russian file. All 68 tips rewritten; weight thresholds as on uaRO (70 %).

**The minimap and the big map** (the tester's list):
- Guild mates are violet triangles, not green rounds like the NPCs'.
- The "Party" layer button is now "Party and guild", showing both signs - the party's ring and the guild's triangle.
- The big map writes the character's name instead of "A guild mate" (from the guild's list; if the game has not got it yet, open the guild window, Alt+G).

# uaRO Graphics 3.0.11-beta.218 — what's new

**The mercenary** (report R-S8BERR):
- The right numbers on the "Mercenary" card and in its block: HP and SP with their most, "Contract until", "Loyalty", "Summons". The uaRO server sends the mercenary's packet in a later field order than the 2007 client reads - the graphics now lays it out in the client's order before the game reads it. The server does not send "Kills" in that packet, so it starts at 0.
- The mercenary's skills are now listed on the card itself: each one's name and level. The game's own skills window was empty - the client does not know the mercenaries' skill names. The "Skills" button is gone.
- The question on dismissing the mercenary is now in your language ("Dismiss the mercenary? Everything it has gained so far will be lost.") instead of broken Korean letters. The game's colour marks (^ff0000) no longer show in messages.

# uaRO Graphics 3.0.11-beta.217 — what's new

- A mercenary no longer brings up "AI.lua error" ("cannot read ./AI/AI_M.lua", "attempt to call a nil value"). The game runs the mercenary's AI from AI\AI_M.lua (with /merai - AI\USER_AI\AI_M.lua), and the uaRO client has no such file. Now the graphics puts an AI there that does nothing at the game's start: on uaRO the server leads the mercenary. An empty file, or one without an AI function, is replaced; your own AI stays as it is.

# uaRO Graphics 3.0.11-beta.216 — what's new

**The mercenary in the interface** ("Mercenary card" switch):
- While a mercenary is hired, its block shows under the party's panel (or the character block): its kind and level, its name, HP and SP bars with numbers, the contract's time left (orange in its last five minutes) and its loyalty. A click on the block opens or closes the "Mercenary" card - the same as Ctrl+R; while the card is open the block is edged gold.
- Controller: the Start menu gets a tenth tile, "Mercenary" (under the middle column), opening the same card.
- There are no orders for the mercenary (attack, come back): the server leads it on its own - it fights enemies nearby and keeps close to you.

# uaRO Graphics 3.0.11-beta.215 — what's new

**Controller**:
- The right stick picks a monster by the camera's real turn: the direction comes from the game's camera every frame. Before, while the character walked by the arrow without the mouse, the stick did not know the camera's turn and took it as looking north - with the camera turned it pointed at the wrong monster. The same is fixed for aiming ground skills with the sticks and the D-pad and for picking an NPC with the right stick.
- A monster you have attacked (with A or a skill) stays the target when you step back with the stick: the next A goes on hitting it, not the nearest neighbour - while it is alive and within 15 cells. Pick another with the right stick; "Clear target" (L3 by default) drops the target. A target not attacked yet gives way to the nearest one as before.
- The cell under the mouse on the ground (the game's Alt+Home turns it on) no longer shows while you play with the controller and the mouse cursor is hidden; move the mouse and it is back.

**Walk without waiting for the server (an experiment)**:
- While the character sits, no step is guessed. The server walks no one sitting, and a guessed step put the character back standing while the server had it sitting - so the next clicks led nowhere until "Sit / stand" was pressed once more.

# uaRO Graphics 3.0.11-beta.214 — what's new

**Chatting from the controller**:
- The chat wheel: hold the talk button (the D-pad down, «Говорити з NPC · утримати: чат») - a wheel opens at the screen's middle; the right stick points at a phrase, letting the button go sends it. LB/RB switch «Фрази» and «Емоції» (ten emotions, shown instantly), LT/RT the channel: «Усім», «Групі», «Гільдії». A short press still talks to the nearest NPC. Your own words in the chat line stay where they were.
- A on the wheel - the on-screen keyboard (a grid, as on consoles): the D-pad or the left stick moves, A types, X erases, Y a space, LB/RB the language (Ukrainian, Russian, English, symbols), LT Shift, Start sends, B closes. It types into the chat line, or into the whisper card or the chat room that has the keys.
- Y on the wheel - Windows voice typing (Win+H) into the chat line.
- The settings app, page «Контролер», part «Колесо чату»: switch it off, write your own eight phrases (empty - the defaults «Привіт!», «Дякую!», «Хіл, будь ласка», «Йду», «Зачекайте», «Так», «Ні», «Бафніть, будь ласка») and the wheel's emotions (empty - the game's own Alt+1 to Alt+0: /! /? /ho /lv /swt /ic /an /ag /$ /...).

# uaRO Graphics 3.0.11-beta.213 — what's new

- @mobsearch on the new minimap: type, for example, `@mobsearch Poring` - every monster the server lists is marked with an orange mark at its cell on the minimap and the big map (pointed at - its name), and a plate under the minimap's bar says what was searched and how many («◆ Poring · 12»). Its ✕ clears the marks; a new search replaces them; another map clears them too. The monsters walk on, so the marks show where they were at the search.

# uaRO Graphics 3.0.11-beta.212 — what's new

**The tester's list, the new part (2)**:
- The 1:1 whisper windows in the new look: a card «Шепіт: name» for each partner (several cascade at the screen's left; a click brings one to the top) with the conversation's lines, the game's own line to type into (a click gives it the keys, Enter sends as before), «Надіслати» and ✕. A new switch «Вікна шепоту» under «Інтерфейс» (and in the console); off - the game's own windows.

# uaRO Graphics 3.0.11-beta.211 — what's new

**The tester's list, the new part**:
- The game's popup menus in the new look: a right click on another player («Request a deal», «Open 1:1 Chat», «Register as a Friend», …), on a chat room's member, on a guild's member - a card at the click with the game's own items; a click on an item does what the game's did, a click elsewhere or Esc closes it. A new switch «Спливні меню» under «Інтерфейс» (and in the console); off - the game's own menus.
- «Hard to hit a player to open the menu»: before changing how the game picks a character under the cursor, this beta writes down what it sees at each right click (the nearest pick box, its size and distance, the game's smallest box) - a report after a few right clicks on players tells what to widen.

# uaRO Graphics 3.0.11-beta.210 — what's new

**The tester's third list, part eleven**:
- The guild card (Alt+G) has all six of the game's tabs now: «Інформація», «Учасники», «Посади», «Навички», «Вигнані», «Оголошення» - a tab the game does not allow you is dimmed. «Посади», «Навички», «Вигнані» and «Оголошення» open the game's own window for now, with all of the guild master's tools in it (positions and tax, guild skills, the notice); its tabs bring you back to the card. They get the new look one by one next.

# uaRO Graphics 3.0.11-beta.209 — what's new

**The tester's third list, part ten**:
- Alt+D: the game's own detail window (object detail, shadows, trilinear - with this renderer it changes almost nothing) no longer opens; Alt+D opens the graphics console with the real settings instead (as Ctrl+` does), from «Список команд» too. A new switch «Alt+D відкриває консоль графіки» under «Інтерфейс» (and in the console); off - the game's window.
- The graphics console fits small windows: its letters get smaller in a small window, all of it stays inside the window, and its foot with the keys gets as many lines as it needs (it was cut at the bottom). Its ground is as see-through as the interface's panels («Непрозорість панелей»).

# uaRO Graphics 3.0.11-beta.208 — what's new

**The tester's third list, part nine**:
- The chat room itself in the new look (under «Картки чат-кімнат»): a card at the screen's left - «Відкрита» or «З паролем», the room's name and how many are in it, its lines (the newest at the bottom), «Учасники» (the owner with ★), the room's line to type into (a click gives it the keys, Enter sends as before) and «Вийти». The owner gets «Налаштування»; a member clicked shows «Вигнати» and «Зробити власником» (the owner's) and «У друзі». Esc no longer risks leaving the room.
- The room's settings (the owner's) use the new card too: «Налаштування кімнати» with the same name, places, open or with a password, and «Зберегти».

# uaRO Graphics 3.0.11-beta.207 — what's new

**The tester's third list, part eight**:
- Making a chat room (Alt+C) in the new look: the card «Нова чат-кімната» - the room's name (the game's own field, typed as before), the places as chips (2 to 20), «Відкрита» or «З паролем» (its password field then gets the keys; the password is shown as dots only), «Створити» (the game checks the name and the password and makes the room, as its OK did) and «Скасувати». Esc cancels. A new switch «Картки чат-кімнат» under «Інтерфейс» (and in the console); off - the game's own windows. The room's own window comes next.

# uaRO Graphics 3.0.11-beta.206 — what's new

**The tester's third list, part seven**:
- The game's command list (Alt+Y; Alt+J in this client is the pet's window, already in the new look) as the card «Список команд»: three tabs - «Перемикачі» (/battlemode, /noctrl, /showname ... each with its state «увімк.» / «вимк.»), «Команди» (/sit, /where, /memo ...) and «Вікна» (each window by its Alt key, named in your language). A click does what the game's click does: a switch flips, a command runs, a window opens. ✕ or Esc closes. A new switch «Картка списку команд» under «Інтерфейс» (and in the console); off - the game's own window.

# uaRO Graphics 3.0.11-beta.205 — what's new

**The tester's third list, part six**:
- The emotion list (Alt+L) in the new look: the card «Емоції» - all 49 emotions seven a row, each with the game's own picture over its command. A click puts the emotion into the chat's line, as before (or into the macros' field while «Макроси чату» is open); a double click shows it at once (the chat's line sent as Enter does). ✕ or Esc closes. A new switch «Картка емоцій» under «Інтерфейс» (and in the console); off - the game's own window.

# uaRO Graphics 3.0.11-beta.204 — what's new

**The tester's third list, part five**:
- «Tip of the Day» in the new look: the card «Порада дня» at the screen's middle on the first map after the start - the tip in the game's words, its number of all, «Назад», «Далі», «Закрити» and the game's own box «Показувати поради під час входу в гру» (it keeps working as before: off - no tips at the next start). The arrows ← → turn the tips, Esc closes; on a controller A and X, B closes, Y the box. A new switch «Картка поради дня» under «Інтерфейс» (and in the console); off - the game's own window.

# uaRO Graphics 3.0.11-beta.203 — what's new

**The tester's third list, part four**:
- No more old-style windows on the way in: the login's screens (the login, the characters, the server select) drew the game's own window a frame before the new one hid it - «Please wait...», the message boxes and the rest flashed in the game's look. They are now out of sight before the game draws them, as in the world.
- «Одразу в гру» no longer shows the windows clicking through: while the shelf's one click takes you from the login to the map, the screen is dark under a card «Вхід у гру…» with the character's name, job and level and three running dots. It goes away when the map starts loading, when a box asks something (a refusal, an error), when the login card comes back, or after 30 seconds.

# uaRO Graphics 3.0.11-beta.202 — what's new

**The tester's third list, part three**:
- Double clicks work in the new windows again: the game's window gets none from Windows, so «Спорядження» (a double click takes a thing off), «Навички» (uses a skill), the choice cards, the server select, the friends, «Лавка» and «Розміщення інтерфейсу» waited for them in vain. Now two left presses soon and near each other are one. Dragging stays.
- The weight on uaRO: HP and SP stop coming back at 70 % of the most weight (not 50 %). The game's line about it now says 70 % and comes at 70 %; its status icon shows «70%» instead of the game's «50%»; the buffs list names it «Weight 70 %».
- «Характеристики»: the cast time in the list to a tenth, as the DEX hint, and the hint shows from now to the next point (44,7 → 45,3 %).

# uaRO Graphics 3.0.11-beta.201 — what's new

**The tester's third list, part two: the maps**:
- The server's marks on the minimap: an NPC's «viewpoint» and @showmobs' monsters - a round in their colour, always shown, over the rest (they were not shown at all).
- Guild mates on the same map on the minimap (a green ring), with the party's layer.
- The Kafras' layer button shows the Kafra's face too, not the old «K».
- The minimap's marks grow as more of the map is shown and get smaller up close.
- The game's map pictures mark the warps with red dots of their own - taken out: the minimap's rings with their words mark the warps.
- A point set on the big map off the walked cells goes to the nearest walked one (it drew a straight line through the walls); off the map or with none near - no point.
- The big map lies over the panels (the chat, the hotbar, the character), under the windows.

# uaRO Graphics 3.0.11-beta.200 — what's new

**The tester's third list, part one**:
- A skill's description: the card said «Не вивчено» and showed no words for a learned skill (Coin Flip 5/5) - the game's card keeps the skill's name as shown, not its code; the card now finds the skill by its name: its level and its description.
- «Налаштування гри» → «Непрозорі вікна гри»: the game only kept this switch and drew nothing by it. Now on, the new interface's windows are whole; off, they are as «Прозорість вікон» has them.
- The party's settings: in a party already the server changes the experience's sharing alone (the things' stay as set when the party was made) - those two rows are not offered then, a line says so.
- The party: its members on the same map were taken for elsewhere (the server names a map «pay_dun03.rsw», the game «pay_dun03») - their way and buffs were not shown, they were missing on the minimap, the list showed the map's file. Now the map is compared rightly, and a member elsewhere shows its map's name («Payon Cave 3»).

# uaRO Graphics 3.0.11-beta.199 — what's new

**The cursor in the game's console**:
- «Курсор» («Гри», «Темний», «Світлий») is in the console (Ctrl + the key left of 1) too - in its «Інтерфейс» part right under «Розміщення інтерфейсу»; it changes at once.

# uaRO Graphics 3.0.11-beta.198 — what's new

**The windows' shadow narrower and neat**:
- 12 pixels instead of 18, a little lighter, dropped 3 pixels — and no light line any more between a window's bottom edge and its shadow (the shadow's whole picture used to be moved down, the empty place under the window with it).

**A cursor of your choice** — «Інтерфейс» → «Курсор»:
- «Гри» — the game's own (as before), «Темний» — dark glass in the interface's gold edge, «Світлий» — white in a dark edge.
- A shape for each of the game's cursors: the arrow, talk, a button, attack, pick up, a warp, a target, the camera turned, not allowed; sharp at any size, with a soft shadow, its size following the interface's.

# uaRO Graphics 3.0.11-beta.197 — what's new

**The tester's notes, part three**:
- A white square round the head (Assumptio with Angel Wings): an HD picture keeps its edge's colour under its see-through parts, and a draw that adds it ignoring its transparency showed that colour as a square. Such a draw now takes the HD picture's colour times its transparency, as the game's own picture would add. The log says when it happens («AI textures: an HD copy added ...»).
- «Кадри» → «Інші вікна гри — легкі»: the hint said 10 frames a second; it is 5 (2 when wholly behind another game window).

# uaRO Graphics 3.0.11-beta.196 — what's new

**The tester's notes, part two**:
- The game's «Please wait...» (on login, on the way to the character select) is now the new interface's waiting card, not the old light-blue window.
- «Продовжити»: «Enter» stands on «Грати»'s line instead of floating in the middle (on the shelf's cards too).
- The shelf's «+ Новий ігровий логін» and «Прив'язати наявний логін» sit on dark plates with light words — no longer lost on the blue wallpaper.
- The minimap: a Kafra is shown by a Kafra's face in a pink ring (the pink «K» was taken for a «close» cross), and the Kafras are drawn over the other NPCs' dots.
- «Речі»: the ☆ of a thing stays while the pointer goes onto it, so it can be pressed.

# uaRO Graphics 3.0.11-beta.195 — what's new

**The tester's notes, part one**:
- «Навички»: a right click on a skill opens its description (the game's skill card in the new look); again on the same skill closes it. The hint under the list says so.
- The words over the heads (a skill's shout like «Raging Quadruple Blow !!», the chat) and the chat rooms' and shops' signs no longer come over the new windows — they stay under them, as the HP bars do.
- The character card: a long name no longer runs into «Base · Job» — the job is left out where it does not fit, the name gets a size smaller, and is cut short only if even then it does not fit.
- The «Вікна» panel takes the mouse where it is drawn: over the character card it can be dragged and clicked.
- The Kafras' menus («Save / Teleport / Storage»; the voting staff's) come in the new NPC talk too — a menu with no words before it used to stay in the old look.
- The old «Hit Point / Defence Rate» bar of the NPC shop no longer shows while the new shop is on (it stays with a player's shop).

# uaRO Graphics 3.0.11-beta.194 — what's new

**Enter no longer crashes the game through a window that is already gone**:
- A player's game closed on Enter: the game still kept the address of its menu (Esc) after the menu itself had been
  freed, and Enter "pressed OK" in it. Now the driver checks before every frame and on Enter that each window the game
  answers Enter with is still among its windows; a freed one is forgotten and written to the log instead of a crash.

# uaRO Graphics 3.0.11-beta.193 — what's new

**The bar under the character no longer comes over a window on hover**:
- The new interface's windows look for the game's cursor to stay under it. While the cursor was set aside, they took
  the HP bar under the mouse for it and went under it. Now no cursor is looked for then — the bar stays under the window.

# uaRO Graphics 3.0.11-beta.192 — what's new

**Diagnostics of the bar under the character**:
- With the cursor over the new interface's windows the log tells every second and a half what the game draws under
  the cursor ("diag:" lines). It is to find why the HP bar shows over a window when the mouse is over it.

# uaRO Graphics 3.0.11-beta.191 — what's new

**The bars under the characters — under the windows (for real now)**:
- The HP and SP bar under the character, the cast bar and the other players' bars are no longer drawn over the new
  interface's windows. In the game they are small "windows" riding with the character — now drawn with the world,
  before the windows.

# uaRO Graphics 3.0.11-beta.190 — what's new

**The HP bars under the characters — under the windows**:
- The HP and SP bar under the character (and under the others) is no longer drawn over the new interface's windows
  ("Chat macros", "Skills" and the others). Over the windows only the game's own windows and its cursor stay.

# uaRO Graphics 3.0.11-beta.189 — what's new

**The shelf with many accounts**:
- The mouse wheel scrolls the shelf — a card a notch, the row gliding (with the ‹ › arrows too). The arrows really
  scroll now: with the first card chosen they did not move the row before.
- A bar under the cards shows the part of the shelf in sight; a click on it goes there.
- Search: with more accounts than fit, a "Search…" field at the top left — just type, the shelf keeps the cards whose
  login, label or character's name match (any case). Esc empties it, Enter plays the chosen card.
- Keys: ← → over the cards, Home / End to the first / last, Page Up / Page Down by four.
- "Characters" scrolls with the wheel too.

# uaRO Graphics 3.0.11-beta.188 — what's new

**A smoother login screen**:
- On the login and the character select the game draws up to 60 frames a second now, not 30: the hover, the views'
  switch and the shelf no longer stutter (the counter showed 30 frames and 33 ms a frame — the "Save power in the
  background" limit, not a load). In the background 15 as before.

# uaRO Graphics 3.0.11-beta.187 — what's new

**The interface: more fixes after the tester**:
- "Server select" and the other choice lists: with more rows than shown, a scroll bar at the right tells that
  there is more below (the wheel scrolls). No more empty dark squares in place of the servers' icons.
- The minimap: the camera's view fan is a neutral light now, not blue like the arrow — no longer taken for the
  character's sight (the arrow shows where the character faces). A new box "The camera's view on the minimap"
  ("Interface", on) — off, and the arrow stays alone.
- The minimap: zoomed out to the end (the whole map) and back in, the map no longer gets even smaller at the first
  step — the steps wider than the map itself are skipped.

# uaRO Graphics 3.0.11-beta.186 — what's new

**The interface: fixes after the tester**:
- A thing picked up is no longer shown twice: in place of the game's box at the top of the screen, "+1 …" over the
  bag stays. A new box "The line on a thing picked up" ("Interface", on); off — the game's box as before.
- The character card no longer folds when "Status", "Items" or "Reference" open: its buttons stay, and the same
  button closes the window.
- The buffs' toggle is written with capitals: "‹ Names" and "Icons ›" (before "names" and "hide names").

# uaRO Graphics 3.0.11-beta.185 — what's new

**The interface's windows over the world again**:
- Water, trees, monsters and characters are no longer drawn over the new windows ("Skills", the character card and
  the others). It showed without DLSS: the game's own windows were moved over ours together with almost the whole
  frame of the world.
- Over our windows now only the game's own windows and its cursor; the names over the heads and the chat's balloons
  under them.

# uaRO Graphics 3.0.11-beta.184 — what's new

**The camera remembers its zoom**:
- After buildings (inns, shops, prt_in…) the camera outdoors no longer comes close in by itself: the free camera in
  buildings used to write its zoom over the outdoors' one, and after every building the camera had to be zoomed out
  again.
- A new box "Remember the camera's zoom" ("Game" → "Camera", on): the zoom and the tilt stay as you left them —
  outdoors and in buildings apart — after the next start of the game too. Before, the game started with the camera
  close in every time.
- In buildings with the free camera the zoom comes back to yours too (before — 300 at most every time).

# uaRO Graphics 3.0.11-beta.183 — what's new

**The login screen, the "Shelf" view**:
- The "Another account", "Create an account" and "Exit" buttons now stand on dark plates like the title and the
  arrows: they are easy to see over a bright login picture (before, white words with almost no ground under them were
  lost on it).
- The site account's links "Characters ›", "+ A new game login" and "Link an existing login" — on plates too.

# uaRO Graphics 3.0.11-beta.182 — what's new

**The login screen: fixes after the review**:
- The site's refusals in the sign-in, the registration and the forms — in your language: "A wrong e-mail or
  password", "This login is taken already", "This e-mail is registered on the site already", "A wrong game login or
  password", "The current game password does not fit" (before — the site's words).
- Fewer disk reads on the login screen: the site account's file is looked at once a second, not several times a frame.

# uaRO Graphics 3.0.11-beta.181 — what's new

**Registration right in the client** (the new login screen; shows once the site is updated — until then "Create an
account" opens the site's page as before):
- "Create an account" opens one form "1 Account → 2 Character → 3 Into the game": the login (while you type, "free" or
  "taken" shows), the password twice, the e-mail, the sex, "I agree with the server's rules". No captcha.
- One password for both the site and the game. After "Create the account" the game logs in by itself and opens the
  first character's make; the account is on the shelf already, then — one click to enter. The e-mail confirmation as
  from the site.
- "An account on the site already? Sign in and add a login" — the site account's sign-in, then "+ A new game login".

# uaRO Graphics 3.0.11-beta.180 — what's new

**Signing in with the site account: all logins and characters** (the new login screen; shows once the site is
updated):
- On the login window "Bring in all logins from the site account" — sign in to the uaro.kiev.ua account three ways:
  the site's password (and its two-factor code when it is on), a code in the browser (the game opens the cabinet's
  page, you press "Allow") or a key from the cabinet ("Security → Devices").
- Signed in, the shelf has all the account's game logins; the password of a login not on this computer yet is typed
  once. "Characters ›" — a row a login: a click on a character enters with it, "+ A new character" — to the select.
- "+ A new game login", "Link an existing login", in a card's menu "Change the game password…".
- On the login window — "nick · logins: N" and "Sign out"; the devices are seen and revoked in the cabinet.

# uaRO Graphics 3.0.11-beta.179 — what's new

**The gamepad on the login screen; "Forget all accounts"** (the new login screen):
- "Shelf" from the controller: ◀ ▶ — the card, A — play, X — another account, Y — the card's menu (↑ ↓, A picks,
  B closes). "Continue": A — play, X — another character, Y — the next account. The login window: A — "Enter", B —
  back to the shelf.
- B to leave the game asks first: the line "B: leave the game · A: stay", B again leaves.
- At the bottom — the hint line with your controller's buttons (Xbox or PlayStation).
- The settings app, "Chat and login": "Forget all accounts" next to "Forget the login" — takes the shelf's saved
  accounts off the computer (asks first; not while the game runs).

# uaRO Graphics 3.0.11-beta.178 — what's new

**Class pictures on the cards; "already in the game" for several windows** (the new login screen):
- The "Shelf" and "Continue" cards show the class picture of the character you entered with (uaRO's 17 pictures
  from the site: Lord Knight to Soul Linker; a second class, a baby or a mounted one with its class's picture). For
  novices and first classes — the character's own head.
- Several game windows: an account another window plays is marked "Already in the game (window N)" on its card. A
  click on it does not log in by itself (the server would throw that window out) — the login window opens with that
  login, and you press "Enter".

# uaRO Graphics 3.0.11-beta.177 — what's new

**Three login views to choose from** (the new login screen):
- At the bottom left "View": "Classic", "Shelf", "Continue". The choice is kept; the same switch is "The login
  screen" under the settings app's "Interface". While the shelf has no accounts, every view shows the login window.
- "Classic": the login window as before, plus ▾ in the "Login" field — the saved logins (the one picked puts its
  login and password in, you press "Enter"), an eye in the "Password" field — the password seen while you hold the
  mouse button. Under the buttons "Create an account" and "Forgot the password?".
- "Continue": the last account big — its character, job, level, map, "Play" (Enter) straight into the game; "Another
  character" — to the character select; "Change account" — another from the shelf, or "+ Another login".

# uaRO Graphics 3.0.11-beta.176 — what's new

**The account shelf: one click to enter** (the new login screen, the shelf, step 2):
- With accounts on the shelf, "Choose an account" shows in place of the login window: a card an account (the
  character you entered with: its name, job, level, map; your label; the login). A click on a card logs in and goes
  straight into the game with that character ("Straight into the game"). Shift held while clicking stops at the
  character select. ← → and Enter work too.
- "⋯" or a right click on a card — its menu: "Straight into the game", "Keep the password", "Label…", "Change login
  and password…", move left/right, "Take off the shelf" (asks again).
- "Add an account" and "Another account" open the usual login window; its "‹ Shelf" goes back. Starting to type a
  login on the shelf does the same.
- The server refused a saved password — the card says "The password does not fit" and the login window asks for it
  again; that password is not sent by itself any more.
- The first launch (the shelf empty): the usual login window with the line "The shelf is empty yet: after a login
  the account shows on it, then - one click to enter".

# uaRO Graphics 3.0.11-beta.175 — what's new

**Accounts go on the shelf by themselves after a login** (the new login screen, the account shelf, step 1):
- Under "Remember login" — "Add to the shelf after login" (on). When the server accepted the login, the login and the
  password are kept on this computer, protected by Windows for your user: another user of the computer, or a copy of
  the file on another computer, cannot read them. The logs have the account's number only, never the login or the
  password.
- At the top of the screen for 6 s: "kweb added to the shelf · Undo" — "Undo" takes the account off again.
- The shelf remembers the character you entered with and the map it came to.
- Logging in from the shelf with one click — in the next betas.

# uaRO Graphics 3.0.11-beta.174 — what's new

**Language and news on the login screen** (the new login screen, steps 4–5):
- At the bottom right of the login screen "УКР / РУС / ENG": a click changes our windows' language at once, without
  a restart, and keeps it (the same language as the settings app's). The game's own texts (its messages, the item
  names) stay as they were.
- A language changed in the settings app while the game runs applies at once too — to all our windows and the
  controller's hints.
- Above the server's state — the event on now or next, else the newest news from uaro.kiev.ua. A click opens it in
  the browser, "All news" - the site's news. The line shows once the site serves it.
- The switch "News on the login" under "Interface".

# uaRO Graphics 3.0.11-beta.173 — what's new

**The server's state on the login screen** (the new login screen, step 3):
- A line at the foot of the login screen: "Server up · 157 players online · ping 52 ms". The ping is measured by a
  connection to the login server every 8 s, the players come from the site.
- The server not answering — instead of the game's "Failed to Connect to Server." boxes one line "The server does not
  answer - trying again in 8 s", and the login is sent again by itself (for 10 minutes, until you type another login).
- The switch "Server status on the login" under "Interface".
- Fixed: the login errors in words of 3.0.11-beta.172 did not show — the game closes the login window while the server
  answers.

# uaRO Graphics 3.0.11-beta.172 — what's new

**Login errors in words and hints under the fields** (the new login screen, step 2):
- Instead of the game's English "Unregistered ID" box — a line under "Login": "There is no login … Check how it is
  written or" and a "Create an account" link. A wrong password — a line under "Password" and "Reset the password on
  the site". The server's other refusals (maintenance, login forbidden until a date, a full server) — in the message
  box, in our words.
- Hints while typing: "Caps Lock is on" and "Layout UKR - logins are usually Latin (kweb?)".
- After three wrong passwords the game offered to open a Korean site and closed itself — that offer no longer comes.

# uaRO Graphics 3.0.11-beta.171 — what's new

**The server choice is skipped** (the first step of the new login screen):
- When the server list has one row, the card picks it and presses "Next" by itself — the window is not shown. The
  same for the character servers' list after the login. Two rows or more — the choice card as before.

# uaRO Graphics 3.0.11-beta.170 — what's new

**Logging in works again** (a fix for betas .151–.169):
- The new login window mixed up the fields: what was typed into "Password" went into the game's own login field,
  and the server answered "Unregistered ID". The window now tells the fields by the game itself (the game draws the
  password's field as asterisks); when it cannot tell, the game's own window stays.
- If logging in failed in those betas: the password may have gone to the server as the login, and with "Remember
  the login" the game may have saved it as the login. Check the "Login" field (clear what does not belong) and
  better change the password in the site's cabinet.
- The game's message card no longer grows with the same line ("Failed to Connect to Server." dozens of times) when
  the game repeats one message.

# uaRO Graphics 3.0.11-beta.169 — what's new

**The chat's macros in the new look** (the eighth group of the remaining windows):
- Alt+M shows a card "Chat macros": ten lines "Alt+1"..."Alt+0". A click on a line gives it the keyboard, the text is
  typed as before (the game's own fields), "Save" writes them.
- The switch "Chat macros card" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.168 — what's new

**The whisper's settings in the new look** (the fifth group of the remaining windows):
- The friends' card's Settings show a card of three switches: "Own window for strangers' whispers", "Own window for
  friends' whispers", "A sound for a new whisper". A click on a row flips it.
- The controller: up/down a row, A flips, B closes.
- The switch "Whisper settings" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.167 — what's new

**A fix for the new windows:**
- Since the new interface's parts passed 32 (beta .154), the "already logged" marks of the windows past the 32nd
  mixed with the first ones (the interface itself worked, but the log for reports could miss lines). Each window has
  its own mark now.

# uaRO Graphics 3.0.11-beta.166 — what's new

**One's own shop in the new look** (the fourth group of the remaining windows):
- With a merchant's shop open a card "My shop: <sign>" replaces the old window: a row a thing still for sale with
  its icon, price and "N left"; at the foot what is left is worth.
- Sold things go by themselves (as in the game); "Close the shop" presses the game's button. Esc does not close the
  shop - so it is not closed by chance.
- The controller: up/down scroll, Y closes the shop (not B, for the same reason).
- The switch "Own shop card" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.165 — what's new

**The guild in the new look** (the sixth group of the remaining windows):
- Alt+G shows a card with the guild's name and two tabs. "Information": the master, the level, the members of the
  most, the average level, the experience to the next level, the tax points, the territory.
- "Members": a row each - a green dot when online, the name, the title, the job and the level; how many are online
  at the top. The wheel scrolls.
- The game's other tabs (positions, guild skills, expulsions, notice) stay in the game's own window with the switch
  off.
- The controller: up/down scroll, LB/RB the tabs, B closes.
- The switch "Guild card" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.164 — what's new

**The server select in the new look** (the third group of the remaining windows - the whole login is new now):
- Between the login and the character select a card "Server select" replaces the old window: a row a server, the
  only one chosen at once.
- A double click or "Next" goes to the character select, "Back" to the login.
- The controller: up/down a server, A next, B back.
- The switch "Server select" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.163 — what's new

**Friends and the party in the new look** (the fifth and sixth groups of the remaining windows):
- Alt+H and the party's button show a card with the tabs "Friends" and "Party": a row a person - a green dot when
  online, ♛ by the party's leader, where they are on the right (or "offline").
- A click chooses a person, a double click whispers to them. The buttons: "Whisper", "Delete" (a friend) or "Expel"
  (from the party), "Leave the party", "Settings" (the party opens the new settings card). The game still asks first.
- The controller: up/down a person, left/right a button, A presses, LB/RB the tabs, B closes.
- The switch "Friends and party card" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.162 — what's new

**The game's options in the new look** (the eighth group of the remaining windows):
- Alt+O shows a card "Game options": "Music" and "Sounds" - a switch and a volume slider (in per cent), "Snap: the
  attack / the skills / the things" (the cursor finds the nearest target by itself) and "The game's windows opaque".
- The slider follows the mouse or the wheel; everything changes at once through the game's own controls.
- The controller: up/down a row, left/right a volume, A a switch, B closes.
- The switch "Game options card" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.161 — what's new

**The mercenary in the new look** (the ninth group of the remaining windows, its end):
- Ctrl+R shows a card: the mercenary's kind ("Mercenary archer", "spearman", "swordsman"), its name and level, HP and
  SP bars, "Contract until" (date and time), "Loyalty", "Summons", "Kills" and its eight stats.
- The button "Skills"; "Dismiss" small at the foot, the game asks first.
- The controller: A its skills, B closes.
- The switch "Mercenary card" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.160 — what's new

**The homunculus in the new look** (the ninth group of the remaining windows):
- Alt+R shows a card: its name and level, HP, SP and experience bars with numbers, "Fullness" (of 100) and
  "Closeness" in the game's word ("Hostile"... "Loyal"), its eight stats (ATK, MATK, HIT, CRIT, DEF, MDEF, FLEE, ASPD).
- The buttons "Feed" (the game asks first) and "Skills"; "Delete" small at the foot, the game shows its red warning
  first.
- The controller: left/right between "Feed" and "Skills", A presses, B closes.
- The switch "Homunculus card" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.159 — what's new

**The pet in the new look** (the ninth group of the remaining windows, its start):
- Alt+J shows a card: the pet's name and level, "Fullness" and "Closeness" as bars with the game's own words
  ("Hungry", "Satisfied"... "Shy", "Loyal"...).
- The buttons "Feed" (the game still asks first), "Perform", "Accessory off" (only with one worn) and "Into the egg".
- The controller: the D-pad over the buttons, A presses, B closes.
- The switch "Pet card" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.158 — what's new

**The party's settings in the new look** (the sixth group of the remaining windows, its start):
- Alt+P (or making a party) shows a card: "Experience" - "Each takes" / "Shared evenly", "Things picked up" and
  "Sharing the things" - "Each takes" / "Shared".
- A click chooses through the game's own buttons (its leader check stays; when only the leader may change them the
  card says so). "Done" and "Cancel" are the game's buttons.
- The controller: up/down a row, left/right its choice, A done, B cancel.
- The switch "Party settings" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.157 — what's new

**Choosing a thing in the new look** (the seventh group of the remaining windows):
- The magnifier ("Identify") and inserting a card ("Insert the card") show a card at the middle: a row a thing with
  its icon, the chosen one lit in gold.
- A click chooses, a double click chooses and confirms at once; "Identify" / "Insert" and "Cancel" press the game's
  buttons. The wheel scrolls a long list.
- The controller: up/down a thing, A confirms, B cancels.
- The switch "Choice of things" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.156 — what's new

**A merchant's shop in the new look** (the fourth group of the remaining windows):
- In place of the two old windows (the goods and the purchase) one card "Shop" with the merchant's sign: a row a
  thing, its price and how many there are.
- "−" and "+" beside each thing choose how many to buy (with Shift ten at a time); a double click on a row adds one.
  The game's checks stay: no more than the shop has, no more than the zeny pays.
- At the foot "Total" (red when the zeny is short) and "Your zeny"; "Buy" and "Cancel" press the game's buttons.
- The controller: up/down a row, A one more, X one less, Y buys, B gives up.
- The switch "Merchant's shop" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.155 — what's new

**The how many window in the new look** (the fourth group of the remaining windows):
- When the game asks how many to move (a stack into the storage, the cart, the bag, a trade, a shop, or to drop), a
  card at the middle replaces the old little window: what for ("Into the storage", "Into the cart"...) and the most.
- The number is typed as before (the game's own field, Enter - done); "1", "Half", "All" in one click.
- "Cancel" is there now (the old window had none): the move is given up, nothing moves. Esc does the same.
- The controller: A takes all, B gives up.
- The switch "How many window" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.154 — what's new

**The merchant's cart in the new look** (the fourth group of the remaining windows, its start):
- "Cart" opens in place of the old window (Alt+W or the cart button in the character window): the same tabs,
  search, "★" and grid as "Storage", the cart's weight at the foot, the things carried in the title ("Cart · 12/100").
- Drag things between the cart, "Items" and "Storage" - the game itself moves them (its count window for a stack).
- A double click on a thing in the cart takes it into the bag; a right click shows its card.
- The controller: the D-pad moves the frame, A into the bag, Y its card, LB/RB the tabs, B closes.
- The switch "Cart" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.153 — what's new

**The character make in the new look** (the third group of the remaining windows, its end):
- On the left the new character's face; under it "Hair" and "Hair colour" with ‹ › arrows, the face changes at once.
- On the right "Name" (typed as before - the game's own field) and the six stats in the game's three pairs:
  STR - INT, AGI - LUK, VIT - DEX. A stat's "+" takes a point from its pair, each pair is always 10 together.
- "Make" and "Cancel" press the game's own buttons; its name checks and messages stay.
- The controller: up/down a row (the hair, its colour, the pairs), left/right change it, A makes, B cancels.
- The switch "Character make" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.152 — what's new

**The character select in the new look** (the third group of the remaining windows, continued):
- Three cards a page: the character's face, the name, the job, the level and the job level, HP and SP, the six
  stats. A free place reads "Free place" with "Make a new character here".
- A click chooses a character, a double click enters the game (on a free place: makes a character).
  The buttons "Enter", "Make", "Delete", "Back"; the arrows and the mouse wheel turn the pages.
- The controller: the D-pad left/right chooses, A enters, X makes, LB and RB the pages, B back to the login.
- Deleting still asks the game's own confirmation (now in the new message card).
- The switch "Character select" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.151 — what's new

**The game's messages and the login window in the new look** (the third group of the remaining windows, its start):
- The game's messages - errors ("Unregistered ID...", a lost connection), Yes/No questions and "Please wait..." - are
  now a card at the screen's middle, on the login's screens too. Enter and Esc answer as in the game; A and B on the
  controller.
- The login window: the account and the password (typed as before - the game's own fields; the password as dots),
  "Remember the account", "Enter", "Exit". A click on a field moves the typing there.
- At its foot "No account? Register on uaro.kiev.ua" opens uaRO's real registration page.
- The switches "Game messages" and "Login window" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.150 — what's new

**Requests and NPC inputs in the new look** (the second group of the game's remaining windows):
- A trade request, a party or guild invitation, a friend request - a card at the screen's middle: who invites and to
  what, "Accept" and "Decline". Esc and the controller's B decline, A accepts.
- A number or words an NPC asks for - the same card with a field: type on the keyboard (or with the controller, as
  before), Enter or "Done" sends it. The field is the game's own, so typing works as before.
- The switch "Requests and NPC inputs" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.149 — what's new

**Item and skill cards in the new look** (the first of the game's remaining windows):
- A right click on an item (in "Items", on the hotbar) or a skill opens its card beside the cursor: the icon, the name
  (refine "+7", slot count), the count, the description from the game's own tables in its colours, and for gear the
  cards in its slots. When the client has the item's big picture, it is on the right.
- A long description scrolls with the wheel (and the arrows), Esc or ✕ close it; on the controller up/down and B.
- An unidentified item shows its "unidentified" description, a skill whether it is learned and at what level.
- The switch "Item and skill cards" under "Interface", on with the new minimap.

# uaRO Graphics 3.0.11-beta.148 — what's new

**A monster's HP bar no longer jitters** (your message):
- The bar stood over the box of the sprite's current frame, and that box differs between the frames of a walk, an
  attack and a hit - the bar jumped after it. Now it stands over the monster's feet (its place in the world, as the
  game's camera puts it) at the height of its tallest frame of the last seconds - still while it walks and in a
  fight. Zoom the camera out and after a second and a half it glides down.

**The game crashing as it entered a map** (report R-DEHAHA):
- The game crashed in its own sort of see-through faces by depth: a face's depth that is not a number breaks the
  game's comparison, and the sort ran past the end of its list. Now such depths are mended before the sort, and the
  log tells where it happened.

**Disconnected on entering the game** (report R-4FBB8B):
- The guard for getting onto a map made in October is back: the game waits for the map server up to a minute instead
  of 15 seconds and connects again by itself; after a failed entry the next character selection waits until the
  server has forgotten that attempt (a countdown over the game) - straight away it would fail again. The switches:
  "Getting onto a map" on the "Game" page of the settings app, both on.

# uaRO Graphics 3.0.11-beta.147 — what's new

**Hotbar: icons with the mouse and a battle mode button**:
- The pencil is gone. A cell's icon can be picked up with the mouse: dragged to another cell - the two swap places,
  carried off the bar - the skill or item is taken off the bar (a red minus shows on the icon).
- So that a quick click in a fight drags nothing by chance, the icon is picked up only when the button is held a
  quarter of a second and the mouse moves. A plain click, even if the mouse twitched, just uses the cell.
- A right click on a cell shows the skill's or item's details (the game's own window).
- In the pencil's place - a button with swords: turns the battle mode on and off, as the /battlemode command. While
  the mode is on, the button is red.

**The bar no longer moves away by itself** (your message; report R-VYV3AG shows it at 10:01:13):
- A click on the bar during which the mouse slipped a couple of pixels counted as dragging the whole bar. Now any
  part of the interface moves only when the button is held 0.3 s and the mouse moves - a quick click no longer moves
  it. In a cell with an icon the icon itself is dragged; the whole bar - by its frame or an empty cell.

# uaRO Graphics 3.0.11-beta.146 — what's new

**Fire Wall no longer "hangs" after the mouse** (report R-VYV3AG "the fire wall is always active"):
- A ground skill pressed with the mouse on the hotbar and then play with the controller - the game kept waiting for
  where to put it: "Select a Target" on the screen, the slot outlined, the cells under the hidden cursor - to the
  end. Now as soon as the controller casts a skill itself or starts aiming, the game's wait ends - as Esc ends it.
- While you play with the controller, the skill's cells under the mouse cursor are not drawn - only the controller's
  aim.
- The walls' counter "2/3" on the hotbar saw none of your walls. This beta writes the server's first packets about
  skill cells to the log - send a report after a couple of Fire Walls and I will fix it by it.

# uaRO Graphics 3.0.11-beta.145 — what's new

**Ground skills, step 4: aiming with the controller**:
- A short press of a ground skill's button places it by the rule of beta 142 (a wall between you and the monster, an
  area where most monsters are, a trap 2 cells toward the target) - now at the moment the button is let go.
- Hold the button longer than a quarter of a second - the aim: the character stands, the skill's cells show on the
  ground. Either stick moves it - the one whose thumb is free: a skill on X/Y/B/A - the left one, on the D-pad - the
  right one, on the shoulders - either.
  - Walls, traps, Fire Pillar: the stick chooses the side from the character (8 sides, as the server's; a diagonal -
    the 5-cell staircase) and the distance (1-4 cells by the lean, a trap up to 3).
  - Areas (Storm Gust, Lord of Vermilion, Sanctuary and the others): the stick moves a cursor; let the stick go next
    to a monster (for Sanctuary, Pneuma, Safety Wall - next to a party member) - the aim moves onto it.
  - The D-pad moves the aim by one cell.
  - Let the button go - the skill is cast. Press a stick (L3 or R3) or B (when the skill is not on B) - cancelled.
- A hint line at the bottom with the aim's buttons.
- While a ground skill's button is held, the other buttons do not repeat.

# uaRO Graphics 3.0.11-beta.144 — what's new

**Ground skills, step 3: the cells under the mouse before the click**:
- Pressed a ground skill's key (Fire Wall, Storm Gust, Sanctuary, a trap) - while the game waits for where to put it,
  the cells it will take show under the cursor: a wall's 3 or 5 cells turned the way the server will put it, Storm
  Gust's square, Sanctuary's shape. Move the mouse round the character - you see on which side the wall comes out as
  a 5-cell staircase.
- The same switch "Skill cells on the ground" (`interface.skill_cells`).

# uaRO Graphics 3.0.11-beta.143 — what's new

**Ground skills, step 2: walls and traps on the hotbar**:
- Fire Wall's slot shows how many of your walls stand: "2/3" (the server allows 3 at once). The same for Quagmire (up
  to 3) and Fire Pillar (up to 5).
- When a wall has burnt out entirely, the slot's edge lights amber for a second and a half - time to put a new one.
- A hunter's trap slot shows the Trap items left in the bag: "×37", in red when none is left.
- Only your own walls count, by the server's packets about skill cells; a map change starts the count anew.
- Switched in "Interface" -> "Walls and traps on the hotbar" (console: `interface.ground_counts`), on at once.

# uaRO Graphics 3.0.11-beta.142 — what's new

**Ground skills on the controller, step 1** (the concept "mage, priest and hunter on the controller"):
- Fire Wall and Ice Wall by a press stand between you and the monster, facing it: 2 cells from you, 1 when the monster
  is 2 cells away, on it when it is next to you; with no monster - 2 cells the way you were walking. A monster coming
  at an angle - the wall comes out as a 5-cell staircase by itself (the server turns it so).
- Storm Gust, Lord of Vermilion, Meteor Storm, Magnus, Heaven's Drive, Thunder Storm by a press land where they cover
  the most monsters within reach.
- Fire Pillar goes like a trap - 2 cells toward the monster.
- After any ground skill's cast its cells show on the ground for half a second: a wall's 3 or 5 cells, Storm Gust's
  square, Sanctuary, a trap. With the controller and the mouse alike. Coloured by element: fire, ice and water, holy,
  wind and lightning, traps.
- Switched in "Interface" -> "Skill cells on the ground" (console: `interface.skill_cells`), on at once.

# uaRO Graphics 3.0.11-beta.141 — what's new

**The character block: Alt+V and the experience in numbers**:
- Alt+V, as in the game, unfolds and folds the block: full <-> small (compact or the icon - the one you chose last).
  The game's own window no longer gets the key. With windows open, Alt+V still shows the full block.
- The full block has two experience bars: Base (lilac) and Job (teal), with a tick each 10 %. The per cent at the
  right, the exact experience under the bar - "1 234 567 / 2 666 000". Beside it the pace an hour and the time to the
  level: "+4,2 %/h · ≈ 13 h". The pace counts the last 10 minutes of play; none in the first minute.
- Point at an experience bar for a tip: what is left to the level, the experience an hour, when the level comes, this
  session's gain and the last gain.
- The weight in numbers now - "Weight 1 710 / 4 500 · 38 %", the zeny beside it. Under it a line of its own: the free
  stat points, skill points and the arrows when few are left.
- The compact block has two thin experience lines at its foot. The compact block's and the rings' tips show the
  experience in numbers.
- The portrait's tip reminds of Alt+V.

# uaRO Graphics 3.0.11-beta.140 — what's new

**The menu and the death screen in the new look**:
- Esc's menu and the window after a death are drawn anew: a card under the middle of the screen (the character
  seen above it), a row a choice. The menu: "Character select", "Exit the game", "Back to the game". After a death:
  "To the save point" (in gold), "Character select", "Exit the game", "Back to the game" - stay where you fell and
  wait for a resurrection. With a Token of Siegfried in the bag "Revive here (Token of Siegfried)" comes first.
- The rows press the game's own buttons - everything works as before, only the look changes.
- The safe row "Back to the game" is chosen first: an Enter by chance does nothing. Mouse - click a row; keyboard -
  the up/down arrows and Enter, Esc closes as before; controller - the D-pad, A chooses, B is "Back to the game".
- Switched in "Interface" -> "The menu and the death screen" (console: `interface.menu`). On at once with the new
  interface; off - the game's own window comes back.

# uaRO Graphics 3.0.11-beta.139 — what's new

**The weight by our server's rules** ("it says 60 %, but HP recovers"):
- On uaRO the natural HP and SP recovery stops at 70 % weight (the server's setting), not at 50 % as in classic RO.
  The interface warned from 50 %. Now the dot by the portrait, the per cent's colour and the weight bar in "Items"
  take the overweight state from the server itself: amber - HP and SP do not recover, red - 90 %, no attacks or
  skills.
- The portrait rings' tip now shows the weight in numbers - "Weight 2 700 / 4 500 · 60 %" - and what an overweight
  stops.

# uaRO Graphics 3.0.11-beta.138 — what's new

**Walking without waiting for the server — no put-backs after a map's entry or in a stall** (report R-VXZ36T):
- No walk is guessed in the first 5 seconds on a map: until it is loaded the server takes no walk (the report had a
  put-back 1.3 s after the entry).
- A put-back counts as a lost packet itself: no walk is guessed for the next 30 seconds. The report had two
  put-backs in a row in one stall - the second click came before TCP noticed the first was lost.

# uaRO Graphics 3.0.11-beta.137 — what's new

**Walking without waiting for the server — only on a calm line** (report R-QTN6TF):
- A walk is guessed only when no packet was lost in the last 30 seconds and the round trip is under 150 ms. On an
  unsteady line both of the report's put-backs fell on a request lost right after the click.

# uaRO Graphics 3.0.11-beta.136 — what's new

**Walking without waiting for the server — only from the cell the server has the character on** (report R-JMHDUY):
- A walk is guessed only when the game shows the character on the cell the server last put it on (its way's end or
  a position fix). In a fight the game lags - finishing the hits while the server walks the character on - and 6 of
  the report's 12 guesses went another way because of it.

# uaRO Graphics 3.0.11-beta.135 — what's new

**Walking without waiting for the server — only on a healthy connection** (report R-DWCBHA):
- While the connection loses packets (a time-out or a server's delay in the last 5 seconds, bytes sent again, a
  round trip of 250 ms or more) no walk is guessed: the server's answers then come late and out of turn.
- No walk is guessed while the walk asked before still waits for the server's answer.
- Fixed: with no way known yet the guess wrongly took the server for still walking the character.

# uaRO Graphics 3.0.11-beta.134 — what's new

**Walking without waiting for the server — fewer mismatches** (report R-U436CU):
- No walk is guessed while the server still walks the character on its last way: the game may show it standing
  (finishing an attack) while on the server it already walks. The way's time is counted at the character's speed
  the server tells.
- When the server walks another way anyway, the log now shows why: its way before and the last position fix.
- The server's delays are no longer mistaken for the moment the game comes back from behind other windows and
  reads what piled up.

# uaRO Graphics 3.0.11-beta.133 — what's new

**Walking without waiting for the server — with the controller too** (report R-N2N7T8):
- Only a mouse click was guessed before: the controller sends its walk requests itself, past the game. Now a walk
  started with the stick does not wait for the server's answer either.

# uaRO Graphics 3.0.11-beta.132 — what's new

**Walking without waiting for the server — fewer put-backs** (report R-6D43Y6):
- No walk is guessed while the server holds the character still anyway: right after it is hit and after its own
  attack. That is how the report's character was put back - five Seals were hitting it at the click.
- A dead character is neither moved nor put back.

# uaRO Graphics 3.0.11-beta.131 — what's new

**The server's delays measured more truly** (report R-6MF4UU):
- When the game itself stood still a moment (taking the screenshot for a report, say), the packets waited in the
  game, not on the network - this no longer counts as the server's delay nor draws a red bar on the ping's graph.

# uaRO Graphics 3.0.11-beta.130 — what's new

**Walking without waiting for the server (an experiment, off at first)**:
- A new switch "Walk without waiting for the server (an experiment)" in "Interface". On, the character starts
  walking right after the click instead of a ping later, when the server answers.
- When the server answers with the same way, the walk joins it without a jump. When the server does not let it walk
  (a cast, a stun) or does not answer in time, the character is put back on the cell it stood on.
- It works only while the character stands still and casts nothing.

# uaRO Graphics 3.0.11-beta.129 — what's new

**A sound when the arrows run low**:
- When 10 arrows (bullets, shuriken) are left, a short soft two-note chime; when none are left, a lower
  three-note one.
- It plays through the game's own sound: at its volume, and not at all while the game's sound is off. After a map
  loads, the same stack does not chime again.
- The switch "A sound when the arrows run low" in "Interface" (on).

# uaRO Graphics 3.0.11-beta.128 — what's new

**Network: the server notices its lost packets sooner** (step 5 of the network plan):
- Windows acknowledges a lone packet of the server only after 200 ms, so the server takes the connection for slower
  than it is and sends a lost packet again later — the world stands still longer, then jumps. Now the game
  acknowledges each of the server's packets at once.
- A new measure: when a server's packet is lost and the ones after it come late at once, the log gets a line and the
  ping's graph a red bar, as for a stall.
- It turns off in "Debugging" with the switch "Quick TCP acknowledgements" (to compare).

# uaRO Graphics 3.0.11-beta.127 — what's new

**The ping on its own plate with a graph**:
- The ping no longer sits on the bar under the minimap, where it cut the map's name short ("Clock T…"); the name
  shows whole again.
- Under the bar, a small plate of its own: "● 50 ms" and a small graph of the last minute (a bar each second):
  green under 120 ms, amber under 250 ms, red slower; red to the top — a stall (the connection waited for a lost
  packet).
- The same switch: "Ping on the minimap" in "Interface".

# uaRO Graphics 3.0.11-beta.126 — what's new

**Dungeon maps are readable again** (reports R-4ZHJ3R and R-4T7N4D, Clock Tower):
- With "Light where you can walk (an experiment)" on, the map dimmed everything you cannot walk to — the walls
  and the ground too. On dungeon maps (a light ground, dark corridors) the ground went exactly as dark as the
  corridors and the map turned one flat grey.
- Now only the walkable places of another part of the map, the ones you cannot reach on foot, are dimmed; the
  walls and the ground stay as the picture has them.

# uaRO Graphics 3.0.11-beta.125 — what's new

**How many arrows are left — always in sight** (over the left end of the F1–F9 skill bar):
- While the character wears arrows (or bullets, shuriken), a plate over the bar shows their picture and count.
- Under 50 the count and the edge turn amber, at 10 or fewer red.
- When the arrows fall under 50 and when 10 are left, "Arrows running low · 37" shows for 5 seconds; none left —
  "Arrows ran out · 0" until you wear new ones.
- The switch "Ammo left on the skill bar" in "Interface" (on).

# uaRO Graphics 3.0.11-beta.124 — what's new

**Monsters and other players no longer jump forward after lags** (step 4 of the network plan):
- When a move packet comes late, the game puts the unit at once where it should be "by the server's clock" — a jump
  forward. Now the jump is smoothed over 0.1–0.3 s: the unit catches up with its way.
- Works with "Smooth position fixes" (the same switch in "Interface").

# uaRO Graphics 3.0.11-beta.123 — what's new

**Fewer "teleports"** (step 3 of the network plan):
- The server often puts a monster or your character a cell or two farther than the game shows them (the game is a
  little behind the server): before each attack of a monster that chased you, when one casts on the move, after a
  lag. The game used to jump them there — now they **glide** there in 0.16–0.24 s. The camera follows smoothly.
- Far moves (a teleport, a knock-back, wings) stay instant.
- The "Smooth position fixes" switch in "Interface" (on).

# uaRO Graphics 3.0.11-beta.122 — what's new

The first step against the character and the monsters "teleporting":
- **Commands go to the server at once.** The game never turned off the small packets' delay on its connection
  (Nagle's algorithm), so a step, a hit or a skill in a row could wait 40–200 ms. Now it is off.
- **The ping on the minimap.** On the bar under the minimap — "● 45 ms": green under 120 ms, amber under 250 ms,
  red slower or when packets were lost; "stall" in red — the connection waited for a lost packet (the world "jumps"
  after it: that was the network, not the game). Off in "Interface" — "Ping on the minimap".
- **A network log.** Once a minute the log gets the ping, the packets lost, the stalls and how many times the server
  "fixed" positions; with a report it shows what causes the jerks.

# uaRO Graphics 3.0.11-beta.121 — what's new

**Carrying things between the windows with the controller** (stage 5 of the windows concept):
- **X — "Take"** on a thing in "Items", a skill in "Skills" or a thing in "Storage".
- The places that take it light up in gold: the "Equipment" slots it fits, the F1–F9 bar's cells, "Storage" and
  "Trade" (for the bag's things), the bag (for the storage's things).
- **The D-pad** (or the left stick) moves a gold frame from place to place — into the other windows too; the thing's
  icon over the frame, a hint under it.
- **A — "Put"**: wear it in the slot, set it on the bar's cell, put it into the storage or the trade (for a stack the
  game asks how many), take it from the storage into the bag. **B** — cancel.
- The old "Assign" list gives way to this: to put a thing or a skill on a button, lay it on the cell with that
  button.

# uaRO Graphics 3.0.11-beta.120 — what's new

- **The arrows worn show right.** With one kind of arrows worn at the login and another put on later, "Equipment",
  "Items" and the character block kept counting the first ones as worn. Now the ones worn now.
- **"Scheme" — a new look of the maps.** The minimap and the big map can show, instead of the game's picture, a clean
  scheme by the cells you can walk on: the floor light (where you can walk to lighter still), the walls dark, the
  edges drawn. Switched by the last round button in the minimap's top left or the "Scheme" chip under the big map;
  in "Interface" — "The maps' picture".

# uaRO Graphics 3.0.11-beta.119 — what's new

**The map's layers switch on the map itself** (a player's request):
- Round buttons in the minimap's top left: portals, NPCs, Kafra, the party. Point at one — its name shows on the bar
  under the map; a click hides or shows those marks.
- The same switches under the big map, with their names: "Portals", "NPC", "Kafra", "Party".
- A layer turned off is struck through. The choice is kept and matches the switches in "Interface" (new: "Kafra on
  the maps" and "The party on the maps"; the Kafras can stay with the other NPCs hidden).

# uaRO Graphics 3.0.11-beta.118 — what's new

- **The arrows show in "Items".** The arrows worn (and new ones added to their stack) did not show before. Now they
  are in "All" and "Equipment" with a green **E**, the tip saying "worn".
- **"Equipment" shows the worn things.** The window was empty (only the arrows showed), because the game keeps what is
  worn apart from the bag. Now every slot is filled as it should be.
- The arrows' stock in the "Character block" finds arrows worn during the play too.

# uaRO Graphics 3.0.11-beta.117 — what's new

Fixes from players' reports:
- An NPC's name in the talk window no longer shows colour codes (`^0000FF…`).
- A Cyrillic character name shows right in the "Character block" (it was «��»).
- The F1–F9 bar no longer jumps right for a second: it moves aside only when a game window stands in its place for
  longer than a second and a half.

# uaRO Graphics 3.0.11-beta.116 — what's new

A new **"Trade"** window — the trade between players in the new look (the windows plan's last stage):
- Your side on the left, the partner's on the right: ten cells each and the zeny under them. A side that pressed
  "Ready" gets a green "✓ Ready" mark.
- "Items" opens with the trade and stands beside it. To offer a thing — drag it from "Items" or double click it.
- Zeny: click your field and type the amount, Enter shows it to the partner at once; "Ready" sends it too.
- **Scam guard:** if the partner adds a thing or changes the zeny after your "Ready", it turns red and a line
  "⚠ Changed after your «Ready»" appears.
- Buttons "Ready", "Trade" (when both are ready), "Cancel"; ✕ and Esc cancel the trade too.
- The right button on a cell — the item's card (the partner's too).
- Controller: A — the next step ("Ready", then "Trade"), Y — the card, B — cancel; the D-pad walks both sides and
  into "Items".
- The "Trade" switch in "Interface"; off — the game's own window.

# uaRO Graphics 3.0.11-beta.115 — what's new

A new **"Storage"** window — the Kafra storage in the new look (the windows plan's storage stage):
- The same tabs, search and grid as "Items"; at the bottom — how many places are used ("57 / 600").
- "Items" opens with the storage and stands right beside it; drag them by either title.
- **Moving things:** drag a thing from one window into the other or double click it — in "Storage" it goes into the
  bag, in "Items" into the storage. For a stack the game itself asks how many.
- The right button — the item's card; the ★ star is shared with "Items".
- Controller: A moves the thing, Y — its card, LB / RB — the tabs, B closes the storage; the D-pad past the grid's
  edge goes into the window beside it.
- The game's own windows (such as the "how many" window) now get the clicks too, not only drawn over ours.
- The "Kafra storage" switch in "Interface"; off — the game's own window.

# uaRO Graphics 3.0.11-beta.114 — what's new

The windows and inventory concept, question 10 — the arrows' stock in the "Character block":
- With fewer than 50 arrows worn, **"Arrows N"** shows on the line under the bars: amber, red at 10 or fewer.
- The arrows ran out (gone from the bag) — a **red "0"** for half a minute, so it is clear why the character stopped
  shooting. Arrows taken off by hand — the line just goes.
- Bullets and thrown weapons say "Ammo".

# uaRO Graphics 3.0.11-beta.113 — what's new

The windows and inventory concept, question 9 — "Items"' new tabs:
- **Six tabs**: "All", "Consumables" (potions, food, scrolls), "Equipment" (weapons, armour and now the arrows),
  "Cards", "Misc" (materials, loot, pet eggs) and "★" — the starred things.
- **The star**: point at a thing — ☆ shows in its cell's lower left corner; click it — ★, the thing goes into the "★"
  tab. Click again — off. The starred things are kept for each character apart. The right click stays the thing's
  description.
- The tab open last time starts at "All" after this update.

# uaRO Graphics 3.0.11-beta.112 — what's new

The windows and inventory concept, question 3 — new things are seen at once:
- **A "new" mark** on the cell of a thing that has just come into the bag (picked up, arrows made, bought, a quest's
  reward) until the cursor points at it.
- **A count on the tab**: a gold number by the name of the tab holding new things (the arrows — in "Other"), so you
  know where to look.
- **"+500 Fire Arrow"** for 2 seconds: over the bag when it is open, or at the bottom middle of the screen when closed.

# uaRO Graphics 3.0.11-beta.111 — what's new

The windows and inventory concept, stage 3 — "Equipment" and "Items" side by side (question 1):
- **Open both and they stand side by side** as one window: "Equipment" left of "Items" (right of it when the left has
  no room), their tops in line. A thing from the bag goes onto a slot next to it, the windows not over each other.
- **They move as one**: drag either by its title and the other follows; both places are kept.
- **Apart**: drag a window by its title holding Shift — it goes alone. Together again once one is closed and opened.
- In the log: "windows: «Спорядження» docked left of «Речі»", "windows: «Спорядження» and «Речі» apart (dragged with
  Shift)".

# uaRO Graphics 3.0.11-beta.110 — what's new

The windows and inventory concept, stage 2 — dragging:
- **Where it may go is lit in gold.** Drag a thing out of "Items" — the "Equipment" slots it fits light up (a bow on
  "Weapon", arrows on the arrows' slot), and the hotbar's cells. Drag a skill out of "Skills" — the hotbar's cells. Drag
  a worn thing out of "Equipment" — the "Items" bag.
- **Taking a thing off by dragging it from "Equipment" into the bag works now.** The window began the game's drag at
  the press, but the game never saw the button come up (as with the skills before). Now the window drags it itself —
  the thing's icon under the cursor — and, let go, begins and at once lets go the game's drag.
- In the log: "equip: #… dragged and let go - let go through the game's own drop".

# uaRO Graphics 3.0.11-beta.109 — what's new

The windows and inventory concept, stage 1 (the rest):
- **The game's own windows over ours** (question 5): the windows we have not redrawn — the arrows' crafting list, the
  storage, the trade, the game's options and others — now show over our windows and panels. Ours used to cover them.
  The game's cursor is over everything.
- **A new window does not open over another** (question 4): if a window already stands in its place, it goes beside
  it — right, else left — and with no room, 24 pixels down and right. Its new place is kept.
- In the log: "windows: the game's own windows and its cursor (N draws) over the driver's interface", "windows:
  «Навички» opened where another stands - at …".

# uaRO Graphics 3.0.11-beta.108 — what's new

The windows and inventory concept, stage 1 — the windows in one stack:
- **The window opened or clicked in last is on top.** "Items", "Status", "Skills", "Equipment", the shop, the NPC talk
  and "Reference" are in one stack now. The drawing order and the clicks' order used to differ, fixed in the code: the
  window on top on the screen might not get the click.
- **A click anywhere in a window raises it**; clicks, drags and drops go to the window seen on top.
- **The top window has a gold edge** while two or more are open (the others keep their quiet edge).
- **Esc closes the top window** (when nothing is typed in the chat); the shop and the NPC talk keep their own Esc.
- **The controller drives the top window.**
- In the log: "windows: «Навички» on top (2 shown)", "windows: «Речі» raised by a click", "pad: «Навички» takes the
  controller (the top window)".

# uaRO Graphics 3.0.11-beta.107 — what's new

Groundwork for the windows and inventory concept:
- **The log now says what came into the bag**: the item, how many, its place and the "Items" tab it shows under
  ("items: … came into the bag … - «Речі» shows it under «Різне»"). One report after making arrows shows where they
  went.
- To find why "Equipment" is empty: open it and send a report — the log has the line "equip: «Спорядження» open - N
  thing(s) worn, M in the bag".

# uaRO Graphics 3.0.11-beta.106 — what's new

From the reports R-WCXUQP, R-JDFY25 and R-N6M9DY:
- **Skills drag onto the bar again** (R-WCXUQP). "Skills" began the game's drag at the press, but the press and the
  release were the window's — the game never saw the button come up, and the skill never landed on the bar. Now the
  window drags it itself (the skill's icon under the cursor) and, let go, begins and at once lets go the game's drag
  where the cursor is — as "Items" does.
- **The controller drives the window in use** (R-WCXUQP). There was a fixed order: with "Items" open, the tabs
  switched there even while working in "Skills" (the shop too). Now the buttons go to the window opened last, or the
  one clicked in with the mouse.
- **The walking arrow reads on light ground** (R-JDFY25, Prontera): the white arrow merged into the white paving. Its
  dark border is wider now (0.1 of a cell) and denser (3/4 of the shape's) — clear on paving, unobtrusive on grass.
- **A crash at the game's exit no longer turns the safe mode on** (R-N6M9DY): the system's DirectInput crashed 1 ms
  after the graphics had closed — counted as a failed start, and two short games in a row would have turned the safe
  mode on. Now a crash after the graphics closed normally counts as the game's exit.
- In the log: "skills: … dragged and let go - let go through the game's own drop", "hotbar: a drop on the cell …",
  "pad: «Навички» takes the controller (the last opened or clicked in)".

# uaRO Graphics 3.0.11-beta.105 — what's new

The cell it goes to is no longer drawn at the walking arrow's end:
- **A new box "The cell it goes to"** in "Interface", on the "Controller" page (under "Walking arrow") and in the game's
  console. **Off** by default: the arrow alone — no gold diamond (chevrons), no cell's corners (slim arrow), no cell's
  frame (the concept's arrow), no small gold ring (ring and pointer, dots of the way).
- Turn it on — the mark at the arrow's end comes back. It changes at once, no restart.
- "The cell alone" always shows its cell — it is that look.
- The red bar at a wall stays.

# uaRO Graphics 3.0.11-beta.104 — what's new

The controller's walking arrow no longer shakes:
- **Why it shook.** The arrow went to the cell the driver asks the server for: "the character's cell + 5 cells
  ahead". That cell hopped a cell on each time the character's cell changed (6–7 times a second) while the feet glided
  up to it. The arrow's end and its cell mark shook against the character by a whole cell. A walking model on the
  controller's real code: 0.19 cells a frame on average, peaks of 1.3.
- **Now** the arrow lies along the stick's way and moves with the character as if glued on: the same model — no shake
  at all. Its length is how far ahead the step is asked (3–6 cells by the speed).
- **At a wall** the arrow shortens softly to the last walkable cell, its end stays put as the character comes up (it
  does not poke into the wall); past it — the red bar. Sliding along a wall — the arrow points along the wall.
- **The way is exact**: with the stick at 30° the arrow used to point at 36–37° (at the rounded cell), now at 30°.
- The chevrons' run and the fading go by a precise clock (it stepped 15.6 ms — in jerks).
- In the log after each walk (the first 6, then every 25th): "walk arrow: a walk of N frames - its end moved against
  the feet at most … cells a frame …" — it should be about 0.

# uaRO Graphics 3.0.11-beta.103 — what's new

The controller's walking arrow — your choice, every variant of the doc on the arrow's looks:
- **A new row "Walking arrow"** in "Interface" and on the "Controller" page (and in the game's console):
  "Chevrons" (the default — the doc's advice), "Slim arrow", "The concept's arrow" (as in beta.102),
  "Ring and pointer", "Dots of the way", "The cell alone", "None". It changes at once, no restart.
- **Chevrons**: three white marks run from the feet to the cell (the nearest fainter), a gold diamond in the cell.
- **Slim arrow**: a thin line coming out of nothing at the feet widens to a white head edged in gold; the cell's gold
  corners.
- **Ring and pointer**: a ring round the feet with a gold pointer on it, a small gold ring in the cell.
- **Dots of the way**: dots from the feet, brighter toward the end, a gold ring in the cell.
- **The cell alone**: its gold frame and a small gold wedge at the feet.
- **For all of them**: a dark shade under the arrow (readable on light ground), a soft turn (~0.08 s), fading in over
  0.12 s and out over 0.2 s after letting go of the stick. **A wall in the way** — a red bar past the cell (pushing
  into a wall standing — the bar right in front of the feet).
- To check: switch the looks in "Interface" while walking, walk along walls and on slopes. In the log:
  "walk arrow: the look …", "walk arrow: chevrons - N triangles on the ground …".

# uaRO Graphics 3.0.11-beta.102 — what is new

Walking with the controller — by the map's cells, with the arrow of the concept:
- **Why it jerked before.** The stick held the mouse cursor so many pixels from the screen's middle and the game
  walked to the cell under it. With a near camera that is 1–2 cells: the character got there and stood until the next
  step (jerks). With a far one 6–10 cells: after letting go of the stick it walked on.
- **Now** the driver asks the server itself for a step to a cell 3–6 cells ahead along the stick (by the character's
  speed, the same at any camera distance) and asks for the next one before the character gets there — no stops on the
  way. A turn of the stick past 22° asks at once.
- **Letting go of the stick** stops the character at the next cell it steps into, no step back.
- **Walls**: the step to the last walkable cell; against one it slides along the wall and goes round a post.
- **The arrow of the concept**: a soft white trail from the feet, a white head edged in gold, the destination cell in
  a gold frame — on the ground's heights (no floating over slopes).
- **The game's cursor is hidden** while you play with the controller; move the mouse and it is where it was.
- The character's place comes from the game itself, not a guess from the packets.
- The setting «The controller walks» is «With an arrow, no mouse» by default now; «With the cursor (as before)» is the
  old way.
- To check: walk with a near and a far camera, let go of the stick (where it stops), walk into a wall, turn the camera.
  In the log: «walk without the mouse: to …, 5 cells ahead», «walk stop at …».

# uaRO Graphics 3.0.11-beta.101 — what is new

The NPC's shop — the keys, the amount, taking back, the description (after the report R-2REP95):
- **The button's words fixed**: «Buy Â· Enter» is «Buy» now with the Enter key beside it (with the controller its
  Start button).
- **The keys** (while the chat's line is empty): the arrows ↑↓ move the frame over the list, ← → move it into the cart
  and back (Tab too); Page Up / Page Down, Home / End. Space or + one more, − one fewer, Shift+Space ten.
- **The amount from the keys**: type digits - the chosen item's amount becomes what you typed (its cart row appears by
  itself). Enter: done, Esc: undo. No more than the zeny or the weight left allow: «at most N» shows under the cart.
  With the mouse: click the number in a cart row and type.
- **Taking an item back**: the cross ✕ in its cart row, the Del key or LT on the controller - the whole row leaves the
  cart.
- **The item's description**: the right mouse button on an item, F1 or Y on the controller - the game's item card opens
  on the shop's right. Again to close it.
- The cart takes no more than the zeny and the weight allow.
- The controller: A +1, X −1 (held: faster, then ten at a time), LT takes back, Y the description, Start buys or sells,
  LB / RB the tabs, B closes (the amount typed or the card first).
- The window's bottom shows the keys as key caps instead of a line of hints.
- To check: buy with the keys (arrows, digits, Enter), take an item back by its cross and by Del, look at a description
  by the right button and F1, the same with the controller.

# uaRO Graphics 3.0.11-beta.100 — what is new

HP bars over every monster:
- **«Monsters' HP bars» is «All» by default now**: a bar over each monster on the screen, a whole one too. A whole
  monster's per cent is asked of the server by the driver with the same name request the game sends when the mouse
  points at it (at most 8 a second, the same one once in 15 seconds). The target's and the one under the cursor are
  wider, with the per cent.
- If you chose another value in «Interface», it stays.

The NPC's shop in the new look:
- On the left the list with icons, prices and weights under a search; on the right the cart with «−» and «+», each
  row's sum, «Total», «Left» and «Weight after buying» (yellow from 50 %, red from 90 %). The trader's name comes from
  the reference.
- It opens on «Buy» at once; «Sell» is a tab (the game asks the server for the list again). Selling, each item shows
  how many the bag holds.
- The mouse: a click puts one in, with Shift ten (selling: the whole stack), the right button takes one out; the wheel
  scrolls. Enter buys or sells, Esc closes.
- The controller: the D-pad goes round the list and the cart, A +1, X −1, Y buys or sells, LB / RB the tabs, B closes.
- Buying and selling go through the game's own functions and buttons: the server gets the packets the game's windows
  send.
- The setting «NPC shops» in «Interface» (and in the console); off brings the game's windows back.
- To check: buy a few potions from a Tool Dealer, sell something, switch the tabs, the same with the controller. In the
  log: «shop: the deal's choice … pressed», «shop: … row(s) bought …», «mob hp: «Усі» - … asked of the server».

# uaRO Graphics 3.0.11-beta.99 — what is new

Monsters' HP bars with the mouse too, not only with the controller:
- **The target without the controller** is the monster the character hits (or the one hitting it). It used to come
  from the target's card, so without the card (off, no reference pack, an unknown monster) the target had no wide bar
  with its per cent, and «The target only» showed nothing.
- **The monster under the mouse cursor** gets the wide bar with its per cent too - even a whole one, while the cursor
  is on it.
- The bars now go under the game's windows, its names and its cursor: a game window over a monster covers its bar.
- «Target's ring on the ground» set to «Always» no longer depends on the target's card either.
- The setting «Monsters' HP bars» is in «Interface» alone (off the controller's page: it is not only the controller's).
- To check: without the controller point the mouse at a monster - a bar and the per cent over it; hit one - the
  target's bar. In the log: «mob hp: the target's bar on N - the monster under the mouse cursor» and «… - the one the
  character fights».

# uaRO Graphics 3.0.11-beta.98 — what is new

The interface's windows, talking with NPCs:
- **A new talk window at the bottom middle** in place of the game's text and menu windows. The NPC's name (from the
  line «[Kafra Employee]») and the map, the text with its colours (black and the too dark ones lit to read on the dark
  glass), the menu's options numbered in two columns, the button «Next» or «Close». The text is as the server sent it,
  untranslated.
- Keys: 1–9 choose an option, Enter answers, Esc ends the talk (new: the game's Esc never did), the arrows go round
  the menu. They work while the chat's line is empty, so typing is not in the way.
- **With the controller** as before (A next / OK, the D-pad chooses, B closes), the chosen option lit in the new
  window; its buttons as glyphs at the window's bottom (the separate hint line hidden meanwhile).
- The number field (Kafra's, the shops') stays the game's for now.
- The setting «Talk with NPCs» in «Interface» (and in the console); off brings the game's windows back.
- To check: talk with a Kafra by the mouse, the keys and the controller. In the log: «npc: the talk in the
  interface's window - N line(s), M option(s)…», «npc: a key chose the option 2 (the menu's own OK)», «npc: Enter -
  next (the game's own button)».

# uaRO Graphics 3.0.11-beta.97 — what is new

The interface's windows, «Equipment»:
- **A new equipment window (Alt+Q, the button of the character block and of the Start tile)** in place of the game's.
  The slots round the character as in the concept: on the left the middle and lower head, armour, garment, an
  accessory; on the right the upper head, weapon, shield, shoes, an accessory; the ammo under the character. A thing:
  its icon, refine «+4» and name, dots for its card slots (gold with a card). In the middle the portrait, name, job,
  levels and ATK, MATK, DEF, MDEF, HIT, FLEE.
- With the mouse: a double click takes off, a press drags into the bag, a right click shows the item's card; a thing
  dragged from «Items» onto the window is put on in its slot.
- **With the controller:** the D-pad goes round the slots, A takes off (on an empty slot: a list of the bag's things
  that fit, A puts on), X changes (the same list), Y the card, B back from the list or close, Start close.
- The setting «Equipment» in «Interface» (and in the console); off brings the game's window back.
- To check: take a thing off and put it on with the mouse and the controller, drag a thing from «Items». In the log:
  «equip: «Спорядження» open…», «equip: … takes off … (the game mode's 0x15…)», «equip: … puts on … (the game mode's
  0x14)».

# uaRO Graphics 3.0.11-beta.96 — what is new

The interface's windows, «Skills»:
- **A new skills window (Alt+S, the «Skills» button of the character block and of the Start tile)** in place of the
  game's skill tree. Tabs by the jobs the character passed through (a Lord Knight: Novice, Swordman, Knight, Lord
  Knight). A row: the icon, the name, what it is used on, SP and range (an unlearned one: what it needs), the level's
  dots out of its most and «5/10», a «+» where a free point raises it, the ‹ › arrows of the level it is used at. The
  most levels and the needs come from the server's skill tree.
- With the mouse: a double click uses, a press drags onto the skill bar, the wheel scrolls, the title moves it.
- **With the controller:** the D-pad or the left stick chooses, ← → the use level, LB / RB the tabs, A uses, Y +1
  level, X «Assign» to a button, B or Start close. These buttons as glyphs at the window's bottom.
- The setting «Skills» in «Interface» (and in the console); off brings the game's window back.
- To check: open Alt+S, raise a skill with «+», drag a skill onto the bar, from the controller Y and X. In the log:
  «skills: «Навички» open - N tab(s)…», «skills: its «+»: … +1 … asked as the game's own «+» (0x4A)», «skills: …
  dragged (the game's drag, 0x51)».

# uaRO Graphics 3.0.11-beta.95 — what is new

The controller in the interface, stage 7, and your «the ring is fine»:
- **HP bars over the monsters - for every player, not only with the controller.** A thin bar over the head: green,
  red below a quarter; the target's wider with its per cent «57%». «The target and the wounded» by default: the
  target's always, a wounded one's for 8 seconds after its last hit, a whole one none. The per cent comes from the line
  the server itself sends to everyone near on each hit; a monster gone out of sight is forgotten (better no bar than a
  stale one). The setting «Monsters' HP bars» in «Interface» (and on the «Controller» page, and in the console):
  «Off» / «The target only» / «The target and the wounded» / «All».
- **The bar and the per cent on the target's card** at the top of the screen.
- **The ring in place of the old mark.** While «Target's ring on the ground» is on, the controller no longer puts a
  false state (poison and the like) on its target; the target's chevron stands over its HP bar. The old mark works
  only with the ring off.
- To check: hit a monster - a bar over it, the target's with its per cent; a bar on the target's card. In the log:
  «mob hp: … at 57% (its name line)», «mob hp: 3 bar(s) over the monsters, the target's with its per cent», «the
  target's mark: the interface's ring».

# uaRO Graphics 3.0.11-beta.94 — what is new

Diagnostics after the report R-3S33MK («the game lags»):
- **Stalls recorded.** In the report the game stopped completely twice - for 15 and for 5 seconds (Windows even put
  its «not responding» window over it) - and the log did not say where. Now the driver watches the game's loop: when
  it stands still for more than a second, the driver looks where the game's thread is, then writes «the game's
  thread: stood still N s» to the log - the Windows function it waited in and who called it. It does not slow the
  game.
- **The window's size.** On a 4K monitor at 175 % the window is placed at exactly 3840x2160 but becomes 3839x2159
  some 40 ms later, and Windows mixes the picture with the desktop (a frame's extra delay). Now the log shows the
  game's own calls that move its window («window: the game's ShowWindow…») - the exact fix will follow from them.
- If the game stalls again, send a report right away.

# uaRO Graphics 3.0.11-beta.93 — what is new

The controller in the interface, after the report R-KMG8GE:
- **B closes windows.** The big map and the game's windows the tile opens (equipment, skills, options, party and
  friends) now close with B - the last one opened first. Before, B went to the slot F3 («the slot is empty») and the
  big map closed only with the mouse. That press goes no further: a skill on B works from the next one.
  While such a window is open and you play with the controller, the line «B close the window» shows at the bottom.
- To check: Start → the tile → equipment (or the big map) → B. In the log: «B: the game's equipment closed»,
  «B: the big map closed».

# uaRO Graphics 3.0.11-beta.92 — what is new

The controller in the interface, stage 6 (part 3):
- **The controller's hints in the interface's style.** The hint line in NPC windows and while choosing whom a heal goes
  on is drawn by the interface itself over the skill bar: a glass bar, the buttons as Xbox or PlayStation glyphs («↑↓
  choose · A OK · B cancel»). The separate little hint window no longer comes (while «Controller's buttons on the
  interface» is on).
- **«Status» from the controller.** While the window is open: the D-pad or the left stick chooses a stat (its «+» lit,
  the line saying what a point gives), A adds a point, B or Start close it.
- To check: talk to an NPC with the controller - a bar with glyphs at the bottom; open «Status» (Start → the tile) and
  add a point with A. In the log: «pad hint: a line of 3 parts in the interface's style», «stats: the controller's A -
  a point to the stat …».

# uaRO Graphics 3.0.11-beta.91 — what is new

The controller in the interface, stage 6 (part 2):
- **«Items» from the controller.** While the items window is open, the buttons are its: the D-pad or the left stick
  moves a gold frame over the cells (the tip at once, with the controller's buttons), A uses or wears (as a double
  click), Y the item's card, LB / RB the tabs, B or Start close it. The character stands meanwhile.
- **X - «Put on a button».** A list of the controller's buttons that use the skill bar's cells, each with what its cell
  holds now. The D-pad chooses, A puts the item into that cell (on that button at once), B cancels.
- To check: Start → «Items» → move the frame, A on a potion, X on a potion → choose a button → A; the potion on that
  button in the skill bar. In the log: «items: the controller's A on …», «pad assign: … into F6 - put», «hotbar: the
  slot … given …».

# uaRO Graphics 3.0.11-beta.90 — what is new

The controller in the interface, stage 6 (part 1):
- **Start opens the windows' tile.** In the middle of the screen - «Menu» of nine tiles: «Items», «Status», «Skills»,
  «Equipment», «Party», «Map», «Довідник», «Game options» and «Controller» (the controller's old menu). The D-pad or
  the left stick moves the gold frame, A opens the window, B or Start closes the tile; the mouse clicks the tiles too.
  The game's windows open through its own buttons (as in the Basic Info window), «Map» - the new minimap's big map.
- The switch «Start opens the windows' tile» in «Interface» and the console (on by default; off - Start opens the
  controller's menu again).
- To check: Start → choose each tile in turn; «Controller» opens the old menu. In the log: «pad tile: opened (Start)»,
  «pad tile: the game's items», «pad tile: closed (B or Start)».

# uaRO Graphics 3.0.11-beta.89 — what is new

The controller in the interface, stage 5 - everything about the controller in one program:
- **The page «Controller»** in the app (was «Gamepad»): which controller the game saw last and when; the interface's
  switches for the controller («Controller's buttons on the interface», «Target's ring on the ground», «The controller
  walks»); **each character's buttons** - choose a character and give each button an action (a bar's cell, attack,
  pick up, sit, talk…), as the Start menu does; how far the stick walks; the old target mark («None» possible);
  vibration, PlayStation's light bar and touchpad, the hint in NPC windows.
- **The controller's language = the interface's**: its menu and hints speak uaRO Graphics' language, no choice of its
  own any more.
- The Start menu in the game stays for now and changes the same.
- To check: open «Controller», choose a character, change a button, enter with that character - the button works the
  new way and shows on the skill bar. In the log: «gamepad: DualSense (Bluetooth)» (or Xbox).

# uaRO Graphics 3.0.11-beta.88 — what is new

The controller in the interface, stage 4 (a prototype, off by default):
- **Walking without the mouse, an arrow on the ground.** In «Interface» → «The controller walks» choose «With an
  arrow, no mouse». The left stick no longer moves the cursor: the character goes where the stick points (the camera's
  turn counted), an arrow on the ground runs from its feet to the cell it goes to with a small gold ring. Before a wall
  the arrow is shorter - no way on (the cells come from the reference). Let go of the stick - the character stops, as
  before.
- The default «With the cursor (as before)» - everything as it was until you check it.
- To check: choose «With an arrow, no mouse», walk in a field and a narrow passage; turn the camera and walk again;
  let go of the stick. In the log: «walk without the mouse: on», «walk without the mouse: <map> - its walkable cells
  read», «walk without the mouse: to …», «walk arrow: from …», «walk stop at …».

# uaRO Graphics 3.0.11-beta.87 — what is new

The controller in the interface, stage 3:
- **The target is a gold ring on the ground.** Under the monster the controller aims at - a gold ring, over its head
  a gold chevron. A light tilt of the right stick shows where the target would go: a dashed white ring under that
  monster; tilt it further - it becomes the target; let go - the target stays. While a heal waits for a choice
  (stage 1), a green notched ring under the chosen member.
- The choice «Target's ring on the ground» in «Interface» and the console: «With the controller» (the default),
  «Always» (also under the monster you fight with the mouse), «Off».
- The old target mark (the poison tint and so on) stays for now, as agreed. To compare, turn it off in the controller's
  menu: Start → «Settings» → «Target mark».
- To check: the ring at the monster's feet, the monster hides the ring's back half; the chevron over its head; light
  grass and dark caves. In the log: «target ring: under … (the pad's target) at the world's …, the chevron over its
  sprite».

# uaRO Graphics 3.0.11-beta.86 — what is new

The controller in the interface, stage 2:
- **The controller's buttons on the interface.** While you play with a controller (a button pressed or a stick pushed
  past half its way), the skill bar's cells show the button that uses them instead of F1–F9: Xbox - coloured A B X Y,
  LB RB LT RT, the D-pad's arrows; PlayStation - ✕ ○ □ △, L1 R1 L2 R2. Cells without a button are dimmer. The target's
  card says at its end: A - «Attack», the right stick - «Another target». A click, a key or the mouse moved brings
  F1–F9 back; it switches at most once in a second and a half, a drifting stick does not switch it.
- The switch «Controller's buttons on the interface» in «Interface» and the console (on by default).
- **The monsters' HP checked on the live server** (for the HP bars' decision): log lines «monster …: HP 57% (its name
  line, 0x0195)» - the first 60 a game. Send the log after a fight near other players.
- To check: play with the controller - the badges come; move the mouse - F1–F9 again. In the log: «hotbar: the
  controller's buttons (Xbox)» or «(PlayStation)», «hotbar: the keys' names».

# uaRO Graphics 3.0.11-beta.85 — what is new

The controller in the interface, stage 1 (by the concept «Контроллер в интерфейсе uaRO»):
- **Heals and buffs on a party member from the controller.** A skill for your own (Heal, Blessing, Increase AGI…)
  with party members near no longer goes on you at once - it asks whom. In the «Party» panel the chosen one is filled
  gold, a hint line at the bottom. D-pad ↑ ↓ - the next (the character last), A - cast (far away - the character
  walks up), the skill's own button again - on yourself, B or Start - cancel; 8 seconds cancel it by themselves.
  Without the party near - on you at once, as before.
- To check: in a party on one map press Heal on the controller, choose a member and cast; again - on yourself; cancel
  with B. In the log: «F2: skill 28 - whom? 3 to choose», «F2: chosen member …», «F2: the choice cancelled».

# uaRO Graphics 3.0.11-beta.84 — what is new

Point 8 (your «1-8, do it») — the last one, all eight are done:
- **The party panel's compact view.** The ▦ button in the «Party» header (left of the fold) turns the panel into two
  columns of small cells: a face, a name and an HP bar (the cast line under it). A dead one's cell is dark red. The ▤
  button brings back the list with the class, buffs, arrow and distance. A click on a cell works as in the list (a
  skill on the member, the way to it). The view is remembered.
- To check: in a party press ▦, then ▤; click a cell; restart the game — the view stays.

# uaRO Graphics 3.0.11-beta.83 — what is new

Point 3 (your «1-8, do it»):
- **The party members on the minimap and the big map.** Each member on your map is a round with its face and a blue
  ring where it stands (the server sends the party members' cells on the same map); a dead one's ring is red. Point at
  it — its name on the bar under the map. The rounds move with the members.
- The party panel has to be on («Party» in «Interface»).
- To check: in a party on one map — open the minimap and the big map (the ⛶ button), walk about. Point 8 (the compact
  view) — the next beta.

# uaRO Graphics 3.0.11-beta.82 — what is new

The party panel, points 2 and 4 (your «1-8, do it»):
- **A click on a member — the skill on it.** Choose a skill (heal, blessing…) — the rows of the members near light
  gold; click the row you want — the skill goes onto that member, as if you clicked it in the world (the character
  walks up when it is far). A ground skill (Sanctuary and the like) lands under the member's feet. The member has to be
  near on your map.
- **Arrow and distance:** a member on your map has a gold arrow its way at the end of its row (as the camera turns)
  and how many cells away it is. A click on the row with no skill chosen lays a way to it on the minimap.
- **Another map:** a route mark by its map's name; a click on the row goes there (the way over the maps, as «Lead» in
  «Reference»; without the new minimap — the map's card opens).
- To check: as a priest choose Heal and click a member in the panel; watch the arrow while walking and turning the
  camera; click a member on another map. In the log: `party: … - skill … on it`, `party: … is on … - the way there
  followed`, `party: a way to …`.

# uaRO Graphics 3.0.11-beta.81 — what is new

The party panel, points 1, 5, 6, 7 of the list (your «1-8, do it»):
- **Class and level** after the name, faint: «Priest 85» — as the member was last seen.
- **A member's cast:** while it casts, the skill's name in place of the class (blue), under the HP bar — a blue line
  of the cast's progress.
- **Dead:** the row dark red, the face faint, «dead» in place of the per cent. Gone when the member is up (HP over
  zero came, or it is seen alive again).
- **Time to recast:** your own buff on a member (the one you put on yourself) pulses in a gold frame in its last 10
  seconds.
- To check: in a party with a priest — put Blessing on, wait for its last 10 seconds; watch a member cast; let a member
  die. Points 2, 3, 4, 8 — the next betas.

# uaRO Graphics 3.0.11-beta.80 — what is new

- **The party members' faces** (your «can the icons be the characters' faces?»). Each member's round now holds its head
  — the hair style and colour as in the game (the same as the character block's portrait). The looks are taken once
  the member has been in sight (the character appear packets) and kept (uaro-graphics\party-looks.txt) — the face stays
  when it is on another map and after a restart. On another map the face is fainter. One never seen yet — its name's
  letter for now.
- **The casts on the members** (your «to see which casts are on the characters with small icons»). Under the HP bar —
  small icons of the member's buffs (Blessing, Increase AGI and so on, debuffs too). The server tells them to everyone
  near, so they show for those on your map nearby. When the cast was seen (someone put it on in front of you), a thin
  gold line of the time left is under the icon, red at its last tenth.
- The members' HP as before — the bar and the per cent (the server sends it for those on your map).
- To check: gather a party, come near — the members have faces; put Blessing or Increase AGI on a member — an icon with
  its time line. In the log: `party: …'s face - …`.

# uaRO Graphics 3.0.11-beta.79 — what is new

- **Dragging without jumps** (your «when I drag the interface the window jumps a lot»). There were three causes:
  - the parts standing by the one dragged (the buffs under the minimap, «Tracking» by the buffs, the party under the
    character block, the hotbar by the chat) moved with it — and the one dragged snapped to them as well; now they take
    no part in snapping and moving aside;
  - the snapping was too strong (about 23 pixels on 4K) — now 6 of the game's pixels;
  - each move painted the part anew (the minimap every frame); now a move does not paint it again.
- **The party panel folds** (your «the party plate should be foldable»): a header on top — «Party» with the online count
  (3/5) and a ▾ button; folded — the header alone, ▸ opens it. It is remembered.
- To check: drag the minimap, the buffs and the party — they move evenly; fold and open the party. In the log:
  `party: its panel folded`.

# uaRO Graphics 3.0.11-beta.78 — what is new

- **Each character has its own skill bar** (your «the skill bar shows the wrong skills, from other accounts, and skills
  the character does not have»). The cause is the game itself: it keeps the hotkeys bar in the registry by the
  character's place on the selection screen (page × 3 + position) and the langtype, with no account in it — so the
  first character of any account shares one bar. Now the driver keeps each character's bar by its name
  (uaro-graphics\hotbar-bars.txt) and puts it in when the character comes into the game, with the game's own functions —
  so the F-keys, the game's bar and the new one all work with it.
- **A character's first time:** the bar the game loaded is taken, with the skills the character does not have (not
  learned) taken out. From then on any change you make to the bar is kept for that character.
- **A skill the character does not have** is grey on the new bar with a crossed circle; its tip says «The character
  does not have this skill».
- The switch «Each character's own skill bar» in «Interface» (on); it works with the game's own bar too, not only the
  new one.
- To check: enter with different characters (and from different accounts) — each its own bar; change a skill on the bar,
  come in again — it is there. In the log: `hotbar: … met the first time - … skills it has not learned taken out`,
  `hotbar: …'s own bar put back - … cells`, `hotbar: …'s own bar kept`.

# uaRO Graphics 3.0.11-beta.77 — what is new

- **The interface's parts move with the mouse straight away, no keys** (your «I wanted to move them without hotkeys,
  everything right away with the mouse»). Press on the minimap, the buffs, «Tracking», the character block, the target
  card, the party, the hotbar or the chat and drag — the part follows the mouse, snapping to the screen's edges and
  middle and to the other parts. A plain click (no movement) works as before: a skill, an item, a button — they act
  when you let the button go.
- **Size by the grip.** With the mouse over a part, a gold grip shows at its bottom right corner: drag it — the size
  from 60 to 160 %; twice on the grip — the part goes back to its own place and size.
- **The parts no longer pile up on each other** (your «some windows overlap one another and get in the way»): a part
  let go over another moves aside to the nearest free place by itself.
- Not to move anything by chance — «Lock the parts' places» in «Interface» (off). Ctrl+Shift+L still shows all the
  parts framed at once (with «Reset all» there).
- To check: drag the minimap and the hotbar elsewhere, drop one part on another — it moves aside; make the chat bigger
  by the grip; click a skill on the hotbar — it works. In the log: `interface layout: … dragged with the mouse`, `… let
  go over another part - moved …`, `… sized`.

# uaRO Graphics 3.0.11-beta.76 — what is new

- **Monster pictures from the AI texture pack** (more of your «load ... everything else from the processed textures
  where possible»). In «Reference» (monster cards and lists) and on the «Target card» a monster is put together from
  the pack's frames — four times bigger than the game's — and scaled down into its frame: smooth and detailed instead
  of stretched pixels, and bigger in its frame. Checked on your client's own files: Poring, Lunatic, Orc Warrior and
  Baphomet — all their frames are in the pack.
- For it the pack keeps a compact table of the sprites' frames of its own (about 290 thousand, ~7 MB of memory), read
  in the same pass in the background at the start.
- The switch is the same — «Icons from the AI texture pack» in «Interface».
- To check: open «Reference» on monsters and target a monster — the picture is sharp. In the log: `pack pictures: …
  and … sprite frames known in … ms`, `reference: …'s picture - … the AI texture pack's 1 frames 4x`, `target: …'s
  picture - … the AI texture pack's …`.

# uaRO Graphics 3.0.11-beta.75 — what is new

- **The interface's parts can be moved and sized** (your «some interfaces should be movable and configurable»).
  Ctrl+Shift+L in the game (or the row «Arrange the interface» in the console ~) turns on «Interface layout»: the world
  dims and takes no clicks, each part of the new interface — «Character block», «Minimap», «Buffs», «Tracking»,
  «Hotbar», «Target card», «Party», «Chat» — in a gold frame with its name.
  - Drag a part with the mouse — it stays there, snapping to the screen's edges and middle and to the other parts'
    edges (gold guide lines).
  - The wheel over a part — its size from 60 to 160 % (shown on its frame).
  - A right click — the part goes back to its own place and size; «Reset all» — all of them.
  - «Done» or Esc — out.
- A place is kept to the screen's nearest corner, edge or middle, so at another resolution the part stays there. The
  parts not moved stand as before (the buffs under the minimap and so on). All of it is kept in
  uaro-graphics\hud-layout.txt.
- A part not on the screen now (the target card with no target, the buffs with none) shows as a faint frame where it
  was last («not shown now») — it can be moved too. A part that has not shown at all since the game started cannot be
  placed yet.
- The cast over the character does not move: it belongs to the hero in the world.
- To check: Ctrl+Shift+L, move the minimap and the hotbar, make the chat bigger with the wheel, «Done»; restart the
  game — all in their places. In the log: `interface layout: open`, `interface layout: minimap moved - at the screen's
  point …`, `interface layout: closed`.

# uaRO Graphics 3.0.11-beta.74 — what is new

- **Icons from the AI texture pack** (your «load the icons of skills, items and everything else from the processed
  textures where possible»). The skill and item icons on the hotbar, in «Items», on the cast bar and in «Reference»
  (lists and item cards), and the status effect icons come from the pack uaro-textures.zip — four times bigger there
  than the game's (a 24×24 icon → 96×96) and scaled down to the size needed, so they are sharp at any scale. Where the
  pack has no picture, the game's stays. Your client's pack has 4008 item and skill icons and 91 newer status icons
 ; the other status icons are not in the pack — the game's stay for them.
- The pack is read once in the background at the start (its interface and texture contents); until then the game's
  icons show, then the panels draw themselves again.
- The switch «Icons from the AI texture pack» in «Interface» (on).
- Monster pictures in «Reference» and on the target card still come from the game's sprites — that is the next step.
- To check: the hotbar, «Items», the buffs — the icons are sharp. In the log: `pack pictures: the AI texture pack's …
  pictures known in … ms`, `client tables: the icon … from the AI texture pack`, `buffs: icon … - the AI texture
  pack's …`.

# uaRO Graphics 3.0.11-beta.73 — what is new

- **The new interface paints faster.** In your report (R-WXNNU2, «minimap test») the minimap took 13 ms to paint,
  while the same code in the test build takes 3.5 ms. The cause: in the 32-bit build every blended pixel of the panels
  was rounded and measured its distance through slow library functions (x87). Now it is worked out in place — the
  same pixels, and blending a pixel on the server became nearly twice as fast (5.7 ns instead of 10). It touches all
  the panels: the minimap, the buffs, the skill bar, the target card, the character's circle.
- The map pictures of beta.72 passed by your report: Morocc's 2048x2048 picture came from the AI texture pack in
  0.16 s in the background, no pink. The pack has pictures for 668 of the client's 670 maps (only cmd_fild and
  um_fild01 lack one).
- To check: run about with the new minimap and send a report — the log lines `interface: the minimap's picture (…)
  painted in … ms` should be well under 13 ms.

# uaRO Graphics 3.0.11-beta.72 — what is new

- **Sharper map pictures** (your «the map pictures themselves are small and untidy»). When the game has a map's
  picture four times bigger — in the AI texture pack (uaro-textures.zip) or the old client's texture_x4.zip — the
  minimap and the big map take it. It loads in the background a moment after you enter the map; the game does not
  stall. The map's edges now fade smoothly into the ground, and zoomed out the picture does not shimmer. The switch
  «Sharp map pictures» in «Interface» (on).
- **No more pink stripes at the edges** (your «pink stripes at the edges»). It was a nearly magenta fringe round the
  game's see-through colour: scaling the picture smudges it into pink. Those points are see-through now too — on the
  minimap, the big map and the world map in «Reference».
- To check: enter a few maps (a town, a field, a dungeon) and open the big map — edges without pink, the picture
  sharp. In the log the line `minimap: … picture 2048x2048 from …` says where the bigger picture came from;
  `no picture bigger than the game's` — there is none, the game's picture shows (without pink as well).

# uaRO Graphics 3.0.11-beta.71 — what is new

- **The player's arrow on the minimap and the big map points where the character looks** (your «the player's icon
  does not turn… where the character looks»). The angle is the game's own — the one it turns the character's sprite
  by: walking, hitting, turning. The light cone stays: it shows where the camera looks.
- **A way on the map.** A click on the minimap or the big map lays a way there over the walkable cells: a gold line
  on the map and footprints on the ground. Shift+click adds one more point — the way goes through them all in turn
  (up to 8). Under the minimap — «Goal · x, y» and the cells left; ✕ there or a right click on the map takes the way
  away. Reached — «You are there». The way shows in «Tracking» too.
- The card of the map a portal leads to now opens with Ctrl+click on the portal on the big map (a plain click: the
  way to it).
- To check: walk and turn — the arrow follows the character; click on the minimap — the way and the footprints. In
  the log: `minimap: the character faces`, `route: a way set on`.

# uaRO Graphics 3.0.11-beta.70 — what is new

- **«Tracking»** — a panel left of the buffs' column (the concept: «keep in view — what to pin (a quest, a way, an
  item to collect), the player chooses»). It holds what you pinned:
  - a quest — the «Track» button at the end of its card in «Reference» (it turns «✓ Tracked»): what to bring, how
    many and whom to give it, no counters (as you decided);
  - the way the new minimap leads («Lead on the minimap»): where next and where the move is;
  - an item to collect — «Track collecting» in an item's card: who drops it most and how likely.
- ▾ folds the panel to the names, ✕ at the top hides it until you pin something new; a click on a name opens its
  card, the ✕ by a name lets it go. What is pinned is kept between games.
- A new switch «Tracking» in the app's «Interface» section and in the game's console (on with the new minimap).
- To check: Ctrl+F → «Quests» → any quest → «Track» at the bottom of the card; the panel left of the buffs.

# uaRO Graphics 3.0.11-beta.69 — what is new

- **The state of «Reference»'s data** (the concept, question 21: the data's states and «Details» by a click). The
  chips at the window's bottom now tell it all:
  - «Data pack · date · server …», and when the pack is over 14 days old — «may be out of date»;
  - «Live · N in the game · X ago» (fresh), «Live data · 3 h ago · may be out of date» (the link gone, an orange dot)
    or «no link» (none came yet).
- A click on a chip opens the page «Reference data»: what the pack holds (on your PC, works without a link), which
  server it is from and how fresh; what the live data is (the site's market, MVP kills, players on the maps) and its
  state. Stale live data is not shown in the cards; the page says so.
- To check: in «Reference» click a chip at the bottom left; close the app for an hour — the live data turns «may be
  out of date».

# uaRO Graphics 3.0.11-beta.68 — what is new

- **«Your bar»** (the concept: «Your bar: Fire Bolt ×2…»; your answer to question 9 — «a great idea, needed»). The
  skills on your bar (all the game's 27 cells) against the monster's element:
  - in a monster's card in «Reference», in «Damage by element», under the element chips:
    «Your bar (element multiplier): Fire Bolt ×2 · Soul Strike ×1.75 · Storm Gust ×1.5 · Jupitel Thunder ×1»;
  - in the target card's tip at the top: «Your bar: …» (the first three).
- The skills' elements are the server's own (db/skill_db.txt), the multipliers its element table. It is the element's
  effect, not a forecast of the damage. Skills without damage (Blessing) and with the weapon's element (Double Strafe)
  are not shown; a skill that heals the monster says «heals it».
- To check: put Fire Bolt and Cold Bolt on the bar, open Poring's card (water 1) — «Fire Bolt ×0.5 · Cold Bolt
  ×0.25»; point at the target card in a fight.

# uaRO Graphics 3.0.11-beta.67 — what is new

- **«Text size»** — a new choice in the app's «Interface» section and in «The new interface» of the game's console
  (the concept: «the text scale — a setting of its own, 80–150 %»): 80, 90, 100, 115, 130 or 150 %. The windows you
  read in — the chat, «Reference», «Items», «Status» — grow or shrink together with their words, on top of «Interface
  size»; the panels, icons and the minimap stay as they were. It changes at once, no restart.
- «Reference» now lays itself out again at once when «Interface size» or «Text size» changes (before, a new value
  reached the open window only after the game's resolution changed).
- **A small screen:** when the «Reference» window is narrow, the sections' rail on the left folds to its icons (as
  before), and now pointing at an icon shows the section's name in a tip beside it.
- To check: in the game's console «Text size» 130 % — the chat and «Reference» larger, the minimap and buffs the
  same; the game in a 1024 × 768 window — in «Reference» point at a section's icon on the left.

# uaRO Graphics 3.0.11-beta.66 — what is new

- **The new interface writes in IBM Plex Sans** — the face all the concept's mock-ups are drawn in («the font —
  IBM Plex Sans built in… Segoe UI — the Windows fallback»). The font is built into the driver (regular and semibold,
  IBM's files unchanged, SIL OFL 1.1 — in `uaro-graphics\licenses`) and installs nothing into Windows: only the game
  has it. Its digits are all one width, so timers and numbers do not jump.
- A word with a sign Plex has not (★, ●, ▾, ✕, ⚠) is written in Segoe UI as before. If Windows will not take the
  font, the whole interface stays in Segoe UI.
- The boxes of tips, chips and labels are now worked out from Plex's own widths: words are not cut with an ellipsis.
- To check: open «Reference», the chat, «Items» — the Plex face (round «a», «e», even digits); in the log at the
  start: `interface: the words in IBM Plex Sans`. If it says `could not be added` — send a report.

# uaRO Graphics 3.0.11-beta.65 — what is new

- **«Light where you can walk (an experiment)»** — a new switch in the app's «Interface» section and in «The new
  interface» of the game's console (the concept: «the highlight of where one can walk — an experiment, off»; off by
  default). The new minimap and its big map dim everything you cannot reach on foot from where you stand: other parts
  of the map — the river's other bank (as on prt_fild04), floors, islands, ledges. The data is the server's own maps
  (.gat), the same the ways are found over; people, monsters and traps are not counted.
- The highlight is worked out once a part of the map: a step changes nothing, only a teleport, a portal or another
  map. Until the game knows your cell (right after entering a map) there is none.
- To check: switch it on, stand on prt_fild04 by the river, open the big map — the other bank is darker; teleport —
  the highlight follows you. In the log: `minimap: where one can walk on prt_fild04`.

# uaRO Graphics 3.0.11-beta.64 — what is new

- **The world map on Ctrl+M** (as in the concept's mock-up). Ctrl+M opens «Reference» right on the world map, Ctrl+M
  again closes it. The game itself does not use this key (its M works with Alt only), and in the chat Ctrl+M does not
  send the line.
- **A way into a dungeon — its maps at the world map's bottom left** (mock-up v19_route, «Glast Heim dungeon»). When
  a way leads into a dungeon that is not on the world map, a box shows in the map's corner: the way in on the world
  map and then each map of the dungeon up to the target with its minimap and name (e.g. Glast Heim → St. Abbey →
  Churchyard), gold arrows between them, the target highlighted. A click on a map in the box opens its card.
- Under the «World map» caption — how many locations and dungeons it has («184 locations · 128 dungeons»).
- To check: Ctrl+M in the game; in the card of Glast Heim Churchyard — «On the world map ›»: the box at the
  bottom left, a click on St. Abbey. In the log: `reference: Ctrl+M - the world map opened`.

# uaRO Graphics 3.0.11-beta.63 — what is new

- **Debuffs no longer run off the screen** (the concept, «Edge cases»: «debuffs are seen at any scale»). Before,
  when there were more debuffs than the buffs' column held (a big scale, a small window), they ran down past the
  edge. Now those that do not fit open a second column on the left — for debuffs only; the buffs' row likewise opens
  a second row under it.
- **A new choice «Buffs that do not fit»** (the «Interface» section in the app, «The new interface» in the game's
  console) — the concept's question 17: «Under «N more»» (the default: the extra ones hide under the button, a click
  shows them all with names) or «In a second column» (nothing hidden: the extra ones stand in a column on the left,
  in the row — in a second row).
- The «N more» button is now the column's whole width and 28 pixels high, as in the concept.
- To check: in the game's console set the interface size to 130 % and take many buffs — with «Under «N more»» the
  extra ones are under the button, with «In a second column» a second column on the left. In the log:
  `buffs: 1 further columns`.

# uaRO Graphics 3.0.11-beta.62 — what is new

- **Fixed: on cards of Direct3D 11 alone (a Radeon HD 6570 and the like) the app shows the effects set the autopilot
  really picked**, not the one by the card's memory — in the «Game» page's line and in «Auto: …» on the Effects page.

# uaRO Graphics 3.0.11-beta.61 — what is new

- **How the first game went — seen on the site.** On a new install (the game never started there) the app tells
  uaro.kiev.ua once, with the install's anonymous mark, how its first game went: the card's class and Direct3D, the
  safe mode or not, whether the game got to a map, its frames there and the effects set, how it ended (fine or by an
  error) and how many minutes it lasted. No name, no login, no address. This shows why newcomers leave after their
  first start.
- The tip of «Send anonymous settings statistics» says so too.
- To check: on a fresh client play once and close the game — the site's install statistics get its first game (once
  the site is updated).

# uaRO Graphics 3.0.11-beta.60 — what is new

- **A client packed with another computer's settings sets itself up for yours.** When uaro-graphics.ini records
  another computer (the packer's) and the game was never started on this one, the app picks the settings for your card
  and screen silently — no «The graphics card or the monitor changed…» question. When the game did run here and you
  changed the card or the monitor, it asks, as before.
- **No questions before the first game.** The anonymous statistics' and the automatic reports' questions now come only
  after you played once on this computer: a newcomer plays first.
- To check: on a freshly unpacked client the app's first start shows no question box; after the first game, at the
  app's next opening — the two questions, as before.

# uaRO Graphics 3.0.11-beta.59 — what is new

- **The safe mode now catches crashes in the first minutes too.** It used to come only when two starts in a row did not
  get to the picture. Now a game that closed by an error within 5 minutes of its first picture counts the same — at the
  login, at the character choice, at a map's loading: such a fault would repeat every time, and the player would just
  leave. After two such starts in a row the next one runs on Direct3D 11 without effects, the neural network, DLSS and
  the 64-bit renderer; the «Game» page says so and offers «Normal mode».
- **A new version tries the normal mode again**: a safe mode turned on by an older version's faults goes by itself
  after the update — its cause may be mended. If the faults come back, so does the safe mode.
- To check: nothing to do; if the game crashes at its start, after two crashes the third start should run in the safe
  mode.

# uaRO Graphics 3.0.11-beta.58 — what is new

- **The effects' autopilot.** The «Auto» effects set is now picked for your graphics card by how it really draws, not
  only by its memory — over the whole ladder «No effects» → «Low» → «Medium» → «High». A minute on a map with the game at
  its full frames and the card less than half busy — the next set on trial for 3 minutes: too much — the set before
  comes back. A card not judged yet that holds the game back half a minute — a set lower. The target: a smooth 60
  frames. What was picked is kept for this card; a new card or monitor — picked again. Each step shows on the screen
  for 10 seconds.
- **«Keep the frame rate» is 60 on every card**, the strong ones too (it was 45, and off on the strong ones).
- **A new install** gets «Auto» and 60 frames. **One set up before** goes over to «Auto» once, when its set is the one
  the automatic setup gave it; a set you picked stays yours.
- The «Game» page has a line «Effects for this graphics card: «…» · aiming at 60 frames a second» with «Change».
- To check: the Effects page should say «Auto: …», the «Game» page the new line. Your computer went over to «Auto»
  too: with the render at 200 % in Prontera the autopilot may lower the set to «Medium» — then pick «High» by hand and it
  stays.

# uaRO Graphics 3.0.11-beta.57 — what is new

- **The new interface should stop stuttering at each step** (reports R-9PP79T «the graphics slow down with the new
  interface» and R-T942DZ). The log line of beta.55 showed why: at 4K the minimap was painted again at each step of
  the character and each turn of the camera — 15.6 ms on the game's thread every time (108 times in 30 s in Prontera,
  183 on prt_fild08); the character block 13–19 ms at each change of HP, the chat up to 30 ms. The shapes were 2–3 ms
  of it, the rest the words: Windows drew all the words again on a canvas as big as the whole picture.
- Now each word is drawn once and kept, and the paintings after take it as it is. The minimap's map is worked out
  twice as fast (the same picture to the pixel), the minimap is painted again at most 30 times a second, and the
  interface's pictures no longer take new memory at each painting.
- The log's line `interface: pictures painted in 30 s` now shows the words' share (`words …`) and how many words were
  drawn anew and how many taken as kept.
- To check: walk round Prontera with the new interface — the motion should be even; send a report — the log will show
  what the minimap costs now.

# uaRO Graphics 3.0.11-beta.56 — what is new

- **A count of installs.** The update check (at every start of the game and of the app) now tells uaro.kiev.ua every
  12 hours only that this install is there: the install's random number (uaro-graphics\install-id.txt), the version
  and the channel. No settings, no play times, no address — only to know how many installs there are and on which
  versions. This is apart from the choice «Send anonymous statistics of the settings» — its tip now says so (the
  «Diagnostics» page).
- To check: once the site's part is in place, the admin's statistics summary has an «installs» block.

# uaRO Graphics 3.0.11-beta.55 — what is new

- **The world's dimming under windows is off by default** (your word: «sometimes the interface has to stay open in
  the game, it gets in the way»). «Items», «Status» and «Reference» can now stay open while playing — the world does
  not darken, the character block does not shrink. The switch «Dim the world under windows» stays in the «Interface»
  section — for those who like it.
- **The big windows' shadow is made five times faster**: opening «Status» for the first time at 4K used to hold the
  game for 46 ms, «Reference» for 79 ms; now 9 and 17.
- **The log tells what the new interface costs** (report R-9PP79T: «the graphics slow down with the new
  interface»): every 30 s a line `interface: pictures painted in 30 s - ...` — which part was painted again how often
  and how many milliseconds it took; a long painting (over 8 ms) is told at once. On R-9PP79T: in Prontera the card is
  busy 22 ms a frame (13–15 on another map) — the town's effects at an inner picture of 7678×4319 (4K and the render at
  200 %), while the game's thread, where the interface is painted, takes only 4.4 ms a frame.
- To check: send a report when it slows down again — the log will show whether it is the interface.

# uaRO Graphics 3.0.11-beta.54 — what is new

- **The new interface no longer stutters in a fight at 4K with the render scale at 200 %** (your report R-PVZH5M:
  «the graphics slow down a little when you hit monsters»). The panels were painted at the inner picture's size
  (7678×4319) — the character block was 1488×1200 points, painted again at each HP change. Now the new panels and
  windows are painted at the screen's size and shown exactly twice as large: four times fewer points to paint and to
  send to the renderer (the same look). The words' fonts are no longer made anew at each painting.
- **A small screen: everything fits** (report R-JK5455: «took a small screen and not all the interface's parts
  fitted»). A new choice «Interface size» (the «Interface» section in the app, «The new interface» in the game's
  console): «Auto» — full size from a game of 1280×720 up, smaller on a smaller one (896×672 — 70 %), but never so small
  its words get hard to read; or 70, 85, 100, 115, 130 %. «Reference», «Items», «Status» now fit a small screen.
- To check: the same game at 896×672 — all panels smaller, «Reference» fits; 4K with the render at 200 % — a fight
  without stutters (the log's «the game's thread busy ... ms a frame» every 30 s should not grow in a fight as before).

# uaRO Graphics 3.0.11-beta.53 — what is new

- **The game's old window no longer flashes** (your word: «when you open the inventory the old one flashes for a
  moment»). The game opens its own «Items» window and draws it at once — the driver used to hide it only at that
  frame's end, so it showed through for a moment. Now the game's replaced windows («Items», «Status», the hotkeys'
  bar, Basic Info, the old chat) are hidden right before the game draws its windows — they never reach a frame. The
  same on another map, when the game shows its windows again.
- To check: Alt+E and Alt+A several times in a row — the old windows do not show through; in the log at the start
  `windows draw: the game's replaced windows hidden before it draws its windows`.

# uaRO Graphics 3.0.11-beta.52 — what is new

- **«Cast over the hero»: safer.** When a cast ends the game takes its old gauge away itself; the driver no longer
  touches it after that (before, turning «Cast over the hero» off right after a cast could show a gauge that was
  gone). Only the live gauge is shown back.

# uaRO Graphics 3.0.11-beta.51 — what is new

- **Tips by the concept's rule: after 0.4 s.** In the character block (the windows' buttons, the rings' numbers), the
  buffs and «Items» a tip now shows once the cursor has stayed 0.4 s — not at once on every pass of the mouse (as on
  the hotbar already). The highlight under the cursor is at once, as before.
- **The character block's buttons press**: for a moment 1 point down and darker, as the hotbar's cells.
- To check: sweep the mouse over the buffs and the block's buttons — the tips do not flicker; stay — the tip shows.

# uaRO Graphics 3.0.11-beta.50 — what is new

- **Red screen edges at low HP — if wanted** (the concept: «the screen's edge — by choice, off»). A new slider «Red
  edges at low HP» (the «Interface» section in the app, «The new interface» in the game's console): 0 — none (the
  default), up to 100 % — how strong. While HP is under a quarter the world's edges turn red (smoothly, 0.18 s), the
  panels and windows over them stay clean; HP back — the edges fade.
- To check: in the game's console set 60 %, take hits until HP is under a quarter — red edges; heal — gone. In the
  log: `low HP edge: the screen's edges red`.

# uaRO Graphics 3.0.11-beta.49 — what is new

- **Open windows, as in the concept.** While «Items», «Status» or «Reference» is open, the world behind them darkens
  smoothly (by 35 %); the game's windows, the new interface's panels and the words over the heroes stay as they are.
  The full character block shrinks to its compact look meanwhile and comes back when the windows close.
- **Windows cast a shadow.** «Items», «Status» and «Reference» cast a soft shadow — easier to tell from the panels.
- A new switch «Dim the world under windows» (the «Interface» section in the app, «The new interface» in the game's
  console); on by itself with the new minimap.
- To check: Alt+E — the world behind «Items» darkens, the character block goes compact, the minimap, the buffs and
  the hotbar do not darken; close it — all comes back. In the log: `dim: the world dimmed under the windows`.

# uaRO Graphics 3.0.11-beta.48 — what is new

- **The cast over the hero in the new look** (the concept; your word on version 4: «the cast over the hero as a strip,
  as in the old client, but not like the old one»). While the character casts, over them a dark pill with the skill's
  icon and name and the seconds left («Fire Bolt 0.8 s»), a gold bar under it — in the place of the game's old gauge
  (that one hidden). What is cast comes from the server's answer, where and how long from the game itself, so the
  pill follows the hero and goes with the cast (an interrupted one too).
- A new switch «Cast over the hero» (the «Interface» section in the app, «The new interface» in the game's console);
  on by itself with the new minimap.
- To check: cast a skill with a cast time (Fire Bolt, Heal at a level with a cast) — the pill with its name and
  seconds over the hero, the gold bar; no old gauge; an interrupted cast — the pill goes. In the log: `cast: the
  character casts the skill ... for ... ms`, `cast: the game's gauge at ... hidden for the label`.

# uaRO Graphics 3.0.11-beta.47 — what is new

- **The new interface's opacity, as in the concept.** The panels' ground — the character block, the minimap's bar and
  list, the buffs, the party, the target's card, the hotbar — is now 80 % (it was 86–92 %), the windows' «Items»,
  «Status» and «Reference» 90 %. Both can be changed: «Panels' opacity» (50–100 %) and «Windows' opacity» (80–100 %) —
  in the app's «Interface» section and in the game's console; the change shows at once. The chat follows the panels
  but is never more see-through than 80 %, so that its channels' colours read. The words, icons and tips stay whole.
- To check: in the game's console move «Panels' opacity» to 50 % — the world shows through the character block, the
  buffs, the hotbar; to 100 % — the grounds solid; «Windows' opacity» — the same for «Items» and «Reference».

# uaRO Graphics 3.0.11-beta.46 — what is new

- **The new interface moves, as in the concept.** The target's card shows and goes smoothly (0.12 s, 4 points); the
  character block changes its look smoothly after a click on the portrait (0.16 s); at a new level (Base or Job) the
  block's edge flares gold once (0.6 s); the «! Low HP · N %» warning slides in from above (0.18 s) and fades
  (0.24 s). What matters in a fight — HP, SP, the hotbar cells' states — changes at once, without motion.
- **Low HP: the portrait glows red** (in the block with bars; the rings have their own glow).
- A new switch «Interface motion» (the «Interface» section in the app, «The new interface» in the game's console): off — everything at
  once.
- To check: pick a monster and walk away — the card comes and goes smoothly; a click on the portrait — the look
  changes smoothly; a new level — the gold flare (in the log `character: a new level - Base ..., Job ...: the block's
  edge flares`); HP under a quarter — the red glow round the portrait and the warning sliding in.

# uaRO Graphics 3.0.11-beta.45 — what is new

- **«Hotbar»: the cells' states, as in the concept.**
  - A skill the game waits a target for (its key pressed, the game waits for a click on a monster or the ground) has
    a gold frame.
  - Too little SP — the icon grey, a blue «SP» plate; its tip «No SP · needs 24, you have 12». The price is the
    server's table's for the level on the bar (for the level learned — what the server itself sent).
  - Forbidden on this map — a crossed circle, the reason in its tip: Fly Wing and Teleport where teleporting is off;
    Butterfly Wing where there is no way back to the save point; branches, portals, Ice Wall; the guilds' war maps.
    The rules are the server code's, the maps' flags the reference's (no pack downloaded — no such state).
  - A press (a click or the key F1–F9, the battle mode's letters) — the cell goes 1 point down and darker for a
    moment.
  - Its tip shows once the cursor has stayed on a cell for 0.4 s.
- No cooldown (the dark sector) yet: the server does not tell the client its skills' delays, and guessing them from
  DEX, Bragi and cards would show what is not so.
- To check: press a skill with a target (Double Strafe, Heal) — the frame while the game waits for the click; spend
  the SP — the «SP» plate and the tip's numbers; inside Prontera's houses (prt_in), where teleporting is off — Fly Wing crossed out. The log:
  `hotbar: SP prices`, `hotbar: the game waits for the target of the skill ...`, `hotbar: on <map> ... «не тут»`.

# uaRO Graphics 3.0.11-beta.44 — what is new

- **«Items»: a use is seen.** A double click on an item now answers in its cell: a gold ring opens out of it, «−1»
  rises over it («worn» for gear), and when the last one is used its icon does not vanish at once but fades. The
  effect on the character (a potion's sparks, the green numbers) is drawn by the game itself on the server's answer —
  as from the old window or the F keys.
- A drag begins once the cursor has gone 10 points (it was 6) — a double click no longer turns into a drag through a
  shaking hand.
- To check: a double click on a potion — the ring and «−1» over the cell, the potion's effect on the character; on
  the last one — its icon fades; on a weapon — «worn». If an item vanishes with no effect on the character, send a
  report: the log tells whether it was a use (`items: a double click on #... - used`) or a throw on the ground
  (`items: #... dragged and let go`).

# uaRO Graphics 3.0.11-beta.43 — what is new

- **«Items»: the clicks.** The close button, the double click, the drag and the right click did not work. The new
  windows, the hotbar and the chat now take a click where the game's cursor stands (where it is seen and where the
  cell lights up), not from the point of Windows' message.
- **Dragging out of «Items» rewritten.** A press on an item began the game's own drag at once, but the game never saw
  the button come up and the item stuck to the cursor. Now the item follows the cursor, and when the button comes up
  the game itself puts it where the cursor is: onto the hotbar, into the storage, on the ground.
- The log tells where the first presses land and what came of them — if something still does not work, send a report
  from the game.
- To check: the close button closes «Items»; a double click on a potion — drunk; a potion dragged onto the hotbar; a
  right click — the item's card. The log's lines `items: a press ... part N`, `items: its close - ...`,
  `items: a double click on #...`, `items: #... dragged and let go - ...`, `items: #...'s card ...`.

# uaRO Graphics 3.0.11-beta.42 — what is new

- **The windows «Items» and «Status» (stage 8 of the new interface).** They open as before (Alt+E, Alt+A, the
  character block's buttons) in the new look; the game's old windows are hidden.
- «Items»: the tabs «Items», «Gear», «Other», a search by the name, a grid of eight cells a row with the counts and
  the refines, the weight and the zeny under it. A double click uses or wears, a press drags (onto the hotbar, the
  storage, the ground), a right click shows the item's card. Items from the storage, the cart and the gear are let
  go right over the window.
- «Status»: base, bonus, total and the next point's price, a «+» where the free points pay for it; the battle's
  numbers beside (ATK, MATK, HIT, FLEE, CRIT, DEF, MDEF, ASPD, the cast's time); the line under them tells about the
  stat pointed at: its price and what a point gives by the game's formulas.
- Both windows move by their titles and keep their places.
- «Interface» has «Items» and «Status»; with the new minimap they are on by themselves.
- To check: Alt+E — the new window; a double click on a potion — drunk, on a weapon — worn; a potion dragged onto
  the hotbar; a right click — the card; the storage: an item dragged into «Items». Alt+A — «+» adds a point. The
  log's lines `items: the game's «Речі» window hidden ...`, `items: a double click on #501 ... - used`,
  `stats: STR +1 asked as the game's own «+»`.

# uaRO Graphics 3.0.11-beta.41 — what is new

- **The chat (stage 8 of the new interface).** At the bottom left — a new chat in place of the old window: the tabs
  «All», «Party», «Guild», «Whisper», «Trade», «Battle», each channel its colour, a tab's count of new lines. «Trade» —
  those near selling or buying; «Battle» — the character's hits and those on it, kills with their experience, what
  was picked up (kills and pick-ups in «All» too).
- Type as before: the game itself takes the words (its old chat window works on, off the screen), the new chat shows
  them in its row. Tab or a click on the channel goes round: all, the party, the guild, a whisper (then type the
  name); a click on a name in the chat whispers to them, «×» takes the whisper away.
- With no new lines the chat dims to its last three. F10 — its height, Alt+F10 — away until you type; the wheel —
  the history.
- An item let go over the new chat does not fall on the ground, as over the other new parts.
- «Interface» has «Chat»; with the new minimap it is on by itself.
- To check: lines near, the party's, the guild's, whispers and announcements in their tabs and colours; the typed
  words and the caret in the chat's row, Enter sends; Tab changes the channel (a party message goes to the party); a
  click on a name whispers; the battle in «Battle». The log's lines `chat: the game's lines read ...`,
  `chat: a line from ...`, `chat: the game's chat window (...) moved off the screen`, `chat: the game's input read
  - ...`.

# uaRO Graphics 3.0.11-beta.40 — what is new

- **The hotbar (stage 8 of the new interface).** At the bottom middle — the game's hotkeys bar redrawn: nine cells
  F1-F9 of the row shown with the row's number (F12 goes round the rows as before). Each cell — its icon, its key, a
  skill's level, an item's count in the bag; none left — the icon dimmed and «0». Pointed at — the name in a tip, a
  click — used as its key would. In the game's battle mode (/battlemode) — all three rows with the letters Q, A, Z.
- A skill from the skills window and an item from the bag can be dragged right onto a cell of the new bar. To take
  one away or move it — the pencil on the right: the game's own bar shows, the tick beside it — back.
- An item let go over any new part of the interface (the minimap, the character block, the buffs…) no longer falls
  on the ground.
- With the chat window at the bottom left, the bar moves to the right of it.
- «Interface» has «Hotbar»; with the new minimap it is on by itself.
- To check: the cells show what the old bar did; a click on a skill and on a potion — as the F key; F12 — another
  row; a potion dragged from the bag onto a cell — it is there; the pencil — the old bar shows, the tick — it hides.
  The log's lines `hotbar: the game's slots - ...`, `hotbar: the cell F1 (...) used through the game's bar` and
  `hotbar: a drop on the cell ...`.

# uaRO Graphics 3.0.11-beta.39 — what is new

- **The party (stage 7 of the new interface).** At the top left under the character block — the party's members:
  the name (a crown on the leader), the HP per cent and a bar under the name. Low HP (under 30 %) — red with «!»,
  the row edged red. One on another map — its map instead of HP, one not in the game — «offline». The server sends
  no SP of the members.
- «Interface» has «Party»; with the new minimap it is on by itself.
- To check: in a party — the panel under the character block; a member near you — its HP changes, one gone to
  another map — its map's name. The log's line `party: its list - N others`.

# uaRO Graphics 3.0.11-beta.38 — what is new

- **The target card (stage 7 of the new interface).** The monster your character fights, in a short card at the
  top middle: its picture, name, level and element. Point at it — its race and size, HP and experience, the elements
  hurting it most and those healing it. A click opens its card in «Reference».
- The target is the one your character hits, and when you hit none — the one hitting you. The card goes when the
  monster dies or is out of sight, and after 12 seconds without hits. It stands under the game's windows when the
  hotkeys' bar is there.
- «Interface» has «Target card»; with the new minimap it is on by itself.
- To check: hit a monster — the card at the top; point at it, click it. The log's line `target: <name> (#<id>,
  Lv …) - the actor … the character hits`.

# uaRO Graphics 3.0.11-beta.37 — what is new

- **The buffs and the character block come with the new minimap.** The new minimap stands where the game draws its
  buff icons and covered them — no buffs could be seen. Now with the new minimap on, «Buffs on the character» is
  «Column» and «Character block» is «Bars», unless you chose otherwise in «Interface» (your choice stays).
- To check: with the new minimap the buffs stand in a column under it, the character block at the top left.

# uaRO Graphics 3.0.11-beta.36 — what is new

- **The character block (stage 7 of the new interface).** «Interface» has a new choice «Character block»: «Bars» or
  «Rings» — the block at the top left instead of the game's window with HP and SP; «As in the game» — the game's own
  window (the default).
- The full block: the portrait (your character's head, its hair colour), the name and class, the Base and Job
  levels, the weight (orange from 50 %), the zeny, the free points, HP and SP with their numbers, the experience with
  a tick each 10 % and the game's window buttons (stats, equipment, items, skills, friends, options — point at one to
  see its key).
- A click on the portrait: full → compact (bars or rings, as chosen) → an icon → full; the view is kept. Over the
  rings and the icon — the HP and SP numbers and the levels.
- Low HP (under a quarter): red and a short warning «! Low HP · N %».
- To check: «Character block» → «Bars»; the game's window with HP and SP is gone, the block stands at the top left;
  click the portrait; the «Items» button opens and closes the items window. The log's lines `character: the game's
  Basic Info window hidden`, `character: the portrait - …spr` and `character: the Basic Info window's button 0x80
  pressed through the window`.

# uaRO Graphics 3.0.11-beta.35 — what is new

- **New buffs (stage 7 of the new interface).** «Interface» has a new choice «Buffs on the character»: «Column» —
  the buffs at the right edge under the minimap, «Row» — a row under it, «As in the game» — the game's own icons
  (the default). The game's icons are hidden then.
- Each icon in a ring of its time left, the time under it; under 10 seconds in orange. Point at one — its name and
  time.
- **Favourites:** a click on an icon puts it first and larger (a click again — back). Debuffs (Decrease AGI,
  Quagmire, Lex Aeterna, overweight…) stand last in red rings and are never hidden.
- «names» over the column opens the list «icon — name — time», «hide names» — back; the choice is kept. What does
  not fit goes into «N more» at the bottom (a click opens the list).
- To check: «Buffs on the character» → «Column», cast a few buffs — the game's icons are gone, the column stands
  under the minimap; click an icon, «names». The log's lines `buffs: the game's own status icons hidden for the
  column` and `buffs: icon … - effect\…tga`.

# uaRO Graphics 3.0.11-beta.34 — what is new

- **The way is no longer made again at every step.** «Reference» keeps the way it laid while you walk it (even when
  the game takes your character a little aside — first slantwise, then straight, as it walks) and lays a new one only
  when you are 5 cells off it or the goal is another. The minimap's line no longer jerks — it just gets shorter
  behind you.
- **The footprints stay where they are on the ground.** They were counted from the character's feet, and all of them
  jumped at each step; now from the way's end. The ones you come to fade out under your feet; a new way comes in
  softly and the old one fades out (the footprints on the same places stay as they were).
- **The footprints are blue**, with a dark blue edge — seen on grass, on snow and in dungeons alike.
- To check: «on the minimap ›» to a location and walk — the footprints do not jump, the minimap's line does not jerk.
  The log's lines `route: the way on … kept 10 steps so far` and, when you step off it, `route: the way on … made
  again from …`.

# uaRO Graphics 3.0.11-beta.33 — what is new

- **The cursor over «Reference» no longer sees the NPCs under the window.** The game looked for a character under the
  cursor through any window and kept the last monster or player you pointed at — so NPCs' names and the talk cursor
  showed under the window. Now the game sees no one under uaRO Graphics' parts (the reference, the minimap and its
  big map, the windows' panel, the answer card).
- **The reference's cross no longer walks your character.** The window shut on the press and the game read the
  button after that — a press on the ground. Now a press one of our parts took does not reach the game until the
  button is let go (the big map's «✕» and the windows' panel menu alike).
- Under the big map (since beta.31) the world no longer reacts to the cursor either — it was left off the list.
- **The way on the ground as footprints**: instead of the solid strip, left and right footprints in turn, toes toward
  the goal. «Interface» has a new choice «The way on the ground»: «Footprints», «A line» or «Off».
- To check: open the reference over an NPC and move the mouse — no names, no talk cursor; close it with the cross —
  the character stays; «on the minimap ›» to a location — footprints on the ground. The log's lines `world mouse:
  under «Довідник» the game pointed at an actor …` and `mouse: a press a part of the driver's took …`.

# uaRO Graphics 3.0.11-beta.32 — what is new

Small things of «Reference» found after the last beta:

- **Monsters** — the list in two: «On the maps» (by level, with a picture, a line «boss · Lv 12 · 5 maps») and «Met
  on no map (events', summons')» — the events' copies no longer mixed with the usual ones.
- **Tables** — the elements a real table now: columns with the elements' names, the cells in colours (over 100
  green, under red, below 0 it heals, violet) and a line under it. In a narrow window the names are shortened.
- **Locations** — the **towns** at last (they were not on the list at all): «Towns» with Kafra and the number of
  shops, then «With monsters». The guilds' castles and passages like Byalan Island are no longer taken for towns.
- **Cards** — in three: «Dropped by monsters», «Not dropped by monsters (events, quests)» and «Enchantments» (AGI+1,
  STR+2…), the numbers in order (AGI+2 before AGI+10). «Items → Card» opens the same list.
- **Pets** — each one's level (the three «Goblin» told apart), pictures, no «lure: #0».
- **MVP** — times over an hour in hours: «1 h 57 min» instead of «117 min»; pictures by the names.
- The counts in their words: «2 items», «7 kinds of monsters», «1 shop».
- A town's card says «town»; «How to get there» says where the way starts.
- To check: Ctrl+F → «Tables», «Locations», «Monsters», «Pets», «MVP», «Items → Card».

# uaRO Graphics 3.0.11-beta.31 — what is new

- **The new minimap — a new look** (as in the concept): a 220 px square with gold corners, a bar under it with the
  location's name and your cell, you a blue arrow where the camera looks with its view; portals blue rings, Kafra a
  pink «K». The map smoothed, no blocks.
- **The + and − buttons** (the map's bottom left) and the mouse wheel over it — from 40 cells to the whole location.
  The «⤢» button — **the map on the whole screen**: all of the location, the portals named («→ Prontera City»), the
  way, you; closed by «✕», Esc or «⤢» again; a click on a portal opens its location in «Reference».
- **The details under the map fold** («▾» at the bar's right), kept for the next games; the monsters in a box apart.
- **The way goes round obstacles**: the line to the portal along walkable cells (not straight through walls), gold.
- **The way drawn on the ground itself**: a gold strip from you to the portal; characters and walls before it hide it.
- «Lead on the minimap» in «Reference» no longer closes the window — the page says it is followed.
- To check: Ctrl+F → a location → «Lead»; on the minimap + / − / the wheel, «⤢», «▾»; the line on the ground; the
  log's `ground way:` and `minimap: the camera looks …` lines (the arrow the camera's way).

# uaRO Graphics 3.0.11-beta.30 — what is new

- **«Reference»: a «Home» button** (a house) in the header on every page but the first screen itself — the world
  map's too; the header's «Reference» title also leads to the first screen with the sections' icons. The sections'
  rail at the left starts with «Home» and has «World map» too.
- Elements, races and sizes of monsters by the game's own names in every language: Fire, Wind, Water, Undead…
- The cards' lists (drops, where to find, dropped by…) shown whole; «all N ›» only for lists longer than 20 rows.
  The search's groups alike: 20 a group, then «N more ›».
- To check: a monster's card → the house in the header and «Home» at the left → the first screen; «World map» from
  the rail; «Damage by element» in English; the drops without «all N ›».

# uaRO Graphics 3.0.11-beta.29 — what is new

- **The world map as in the concept — the fourth stage**: tabs «World map» / «Dungeons» on top, a green «N online»
  and the zoom's buttons (−, the whole map, +); the wheel zooms round the pointer, the mouse drags the map, a click
  without a drag opens the location's card. The maps' pictures are twice as sharp.
- On the map: green pills with the players' numbers, the towns' names on their tiles, «You are here · …», the card
  of the location under the pointer (its name, code and monsters' kinds, the players now).
- **The way on the map**: a location's card has «On the world map ›» in «How to get there». The map zooms on the way
  itself: a gold line, a Kafra teleport a pink dashed arc with its price, a gold pin at the end (a dungeon's at its
  way in, «… → dungeon»). At the right: «The way to …», from where, the ways (On foot / By Kafra — a click), the
  warning, the numbered steps, the legend, «✓ Lead on the minimap» and «Reset».
- A monster or an item «on the world map» — the maps listed with their numbers at the right, gold marks on the map.
- To check: Ctrl+F → «World map»: the wheel, dragging, the −/+ buttons; a location's card → «On the world map ›» →
  the way, «On foot» / «By Kafra», «Lead on the minimap».

# uaRO Graphics 3.0.11-beta.28 — what is new

- **«Reference» as in the concept — the third stage, the search results**: groups «MONSTERS · 7», «ITEMS · 89» with
  a note at the right («the commonest first», «the nearest first»); each row has its icon, its name with the found
  word on a gold ground, a line under it (the price and the shops, how many monsters drop it; the level and on how
  many maps; «town», «you are here») and at the right what it is («item», «monster», «location», «quest», «NPC»).
- A group shows its first 8, the rest behind «N more ›» (a click or Enter opens all, up to 100). The first result
  is picked at once — Enter opens it, the arrows go on.
- New: items by their service names — a group of their own («peco wing» finds Peco Peco Feather:
  Peco_Wing_Feather); monsters by their other kRO names («wander man» — Wanderer) once the site updates its pack.
  Nothing found — what to try.
- The search is faster: 1 ms a key instead of 16–18 ms.
- To check: Ctrl+F → «wing», «poring», «prontera», «xyz»: the groups, the gold marks of the word, «N more ›», Enter.

# uaRO Graphics 3.0.11-beta.27 — what is new

- **«Reference» as in the concept — the second stage, the cards**: the sections' rail at the left with icons and
  numbers (the page's lit, «All» the first screen); a card's head — its picture, name, tags (the level, the element
  in its colour, MVP) and a line under them.
- The main two blocks side by side: a monster's «Where to find» and «Drops», an item's «Dropped by» and «Where to
  buy», a location's «Who is here» and «How to get there» (on foot, by Kafra — moves and price); a quest's blocks
  the whole width. Names are links with a gold underline, the per cents and numbers gold.
- The other sections fold (▸/▾) with their summary on their line; «Damage by element» as coloured chips (green —
  more, red — heals); a way's steps as sections «Steps · On foot», «Steps · By Kafra». A location's picture is its
  minimap.
- To check: Ctrl+F → a monster, an item, a location: the rail at the left, two blocks side by side, a click on
  «Damage by element» folds it.

# uaRO Graphics 3.0.11-beta.26 — what is new

- **«Reference» as in the concept — the first stage**: a dark glass window; the title at the left, «‹ Back to …» and
  the way here («Monsters › Poring»), on a card the search with Ctrl+F and ☆ in the header; at the bottom the «Data
  pack» and «Live data» chips (a green dot: fresh, how many players are in the game) and the keys.
- The first screen: a big search in a gold frame, 10 tiles with icons and numbers — Monsters, Items, Locations,
  Quests, NPCs and services, MVP, Pets, Crafting, World map, Tables; «Opened lately» and «Starred» as cards with
  pictures side by side. New sections «NPCs and services» (Kafra and shops) and «Tables» (elements by defence
  level); «MVP» — the MVPs only.
- ☆ in a card's header stars it (instead of the «Star it ›» row).
- Next betas: the cards (the sections' rail, the header with the picture, blocks), the search's results, the world map.
- To check: Ctrl+F — the look of the concept's mock-up; a tile under the pointer — a gold frame; open Poring —
  «‹ Back to Reference», the way, ☆ stars it.

# uaRO Graphics 3.0.11-beta.25 — what is new

- **«Reference» without the cursor's jerks** (report R-77TCM7): when the cursor goes to another tile or row, the window
  is no longer painted again whole — only the old row and the new one, and only they go to the video card (at 4K with
  the interface at 200 % each such move painted the whole window for about 0.3 s). The same on «World map» (two maps
  and the row under the map), with the arrows ↑↓ and as the icons come.
- The whole window is painted many times faster too (at 4K 14 ms instead of 290), the words' fonts one per size.
- To check: Ctrl+F, move the cursor over the tiles, the search's rows and «World map» — the cursor does not hang, the
  FPS does not drop.

# uaRO Graphics 3.0.11-beta.24 — what is new

- **The cursor over our windows — as over the game's**: the world under «Reference», «New minimap (prototype)», «The
  windows' panel in the game» and the report answer's card no longer reacts to the cursor — no sword over a monster,
  no talk over an NPC, no names under it; the cursor is the arrow, as over the inventory. Clicks there never reached
  the world.
- To check: Ctrl+F, put «Reference» over monsters or NPCs and move the cursor over it — the arrow stays, nothing under
  the window lights up; outside it all is as before. The log uaro-graphics.log has «world mouse» lines.

# uaRO Graphics 3.0.11-beta.23 — what is new

- **«Reference» from the pad**: uaRO Pad's menu (Start → buttons) has a new action «Reference (uaRO Graphics)» — any
  button may have it; a press opens «Reference», another closes it (with «Reference (prototype) - Ctrl+F» on). Then
  the D-pad, A, B, Start, LB/RB drive it (of beta.21).
- To check: Start → the buttons' tab → on a free button (R3, say) «Reference (uaRO Graphics)» → save → press it in the
  game.

# uaRO Graphics 3.0.11-beta.22 — what is new

- **Search by number**: a number in «Reference» finds the item and the monster of that id («601» — Fly Wing, «1002»
  — Poring), as @iteminfo and @mobinfo call them.
- An item's card tells what an NPC pays for it («sells for 30 z», half its price).
- The pictures and icons «Reference» keeps in memory are 2000 at most — a long browse takes none of the game's memory.
- To check: Ctrl+F → «601» — «Number 601: Fly Wing»; the Fly Wing card — «weight 1 · price 60 z · sells for 30 z».

# uaRO Graphics 3.0.11-beta.21 — what is new

- **«Reference» by the arrows and the pad**: ↑ and ↓ pick a row (lit as the one under the pointer), Enter opens the
  picked one (none picked: the first found); PgUp/PgDn scroll.
- While «Reference» is open the gamepad (uaRO Pad) drives it: the D-pad picks a row, A opens it, B goes back (on the
  first page closes it), Start closes it, LB/RB scroll; the character stands meanwhile. «Reference» opens by Ctrl+F or
  a click under the minimap for now.
- To check: Ctrl+F → «poring» → ↓ ↓ → Enter opens the picked row; with the pad — the D-pad and A.

# uaRO Graphics 3.0.11-beta.20 — what is new

- **«Bosses and MVPs» — the hunters' board**: the bosses' list has, beside the respawn, a players' timer when someone
  opened it to all on the site: «in 23 min» or «should be up» (after the site's update).
- **An item on the world map**: an item's card has «On the world map ›» under «Dropped by»: where to farm it — the
  maps of the monsters dropping it, and how many of them live there.
- «Reference»'s bottom line tells how fresh the site's live data is («the site's live data 2 min ago»).
- To check: Ctrl+F → «jellopy» → «On the world map ›» — Prontera's fields with numbers; the tile «Bosses and MVPs».

# uaRO Graphics 3.0.11-beta.19 — what is new

- **A monster on the world map**: a monster's card has «On the world map ›» under «Where to find»: the world map with
  gold frames on every map it lives on and how many live there; under it the dungeons and rooms with it (not on the
  world map itself). Pointing at a map tells its name and «Poring ×70», a click opens its card with «How to get there».
- To check: Ctrl+F → «poring» → «On the world map ›» — frames on the fields of Prontera, Geffen and Payon, under it
  «Dungeons and rooms: Labyrinth Forest ×5 …».

# uaRO Graphics 3.0.11-beta.18 — what is new

- **«Starred» and «Opened lately» on «Reference»'s first screen**: under the tiles — the cards marked with a star and
  the 8 opened last (monsters, items, maps, quests); a click opens the card. Under each card's name — «Star it ›»,
  and in a starred one «★ Starred» (a click takes it out). Both lists stay between games
  (uaro-graphics\reference-favourites.txt, reference-recent.txt).
- To check: Ctrl+F → «poring» → «Star it ›» → Esc, Ctrl+F — the first screen has «Starred: Poring» and «Opened lately:
  Poring».

# uaRO Graphics 3.0.11-beta.17 — what is new

- **The cards' sections fold by your choice**: a click on a section's heading («Where to find», «What it drops»,
  «Damage by element», «How to get there», «NPCs» and so on) folds it — in every card at once, and so it stays in the
  next games (uaro-graphics\reference-folded.txt); a folded heading has «›», a click unfolds it.
- To check: Ctrl+F → «poring» → a click on «Damage by element» — the section folds; «baphomet» — folded there too;
  after the game's restart — the same.

# uaRO Graphics 3.0.11-beta.16 — what is new

- **Quests in «Reference»** (after the site's update — a new quests.json and the quests' file; the app takes them at
  the game's start, only when something changed on the site): the tile «Quests · 60» on the first screen — by kinds
  (job changes, headgear, access to towns, events, other); a quest's card — its description, «Who gives it» (the NPC
  and the map, «How to get there» from there), «What it takes» (the items with icons and how many), «Steps» and
  «Reward» — in the interface's language (no translation: English); long text breaks at words.
- An item's card has «For quests»: the quests that take it, and how many; the search finds quests by their titles.
- To check (after the site's update): Ctrl+F → the «Quests» tile → «Ayothaya Access»: 9 steps, «What it takes —
  Engagement Ring ×1»; «Engagement Ring» → «For quests».

# uaRO Graphics 3.0.11-beta.15 — what is new

- **Item icons in «Reference»'s lists**: each item has its icon from the client before it («What it drops», the
  search, the sections, the shops, the crafting); the icons of the rows in view come a few a frame — long lists never
  stall.
- **NPCs by their names**: «kafra», «healer», «sailor» — the group «NPCs: N», the nearest to your map first, with the
  map, the cell and «moves: N»; a click opens the map's card (with «How to get there» and «on the minimap ›»).
- **«The nearest Kafra» in a map's card**: the Kafra on this map («here · 146, 89») or the one the fewest moves away.
- The pictures and icons are looked for as the game itself does (its CFileMgr: DATA.INI's archives, zips too) — more
  are found than in beta.14.
- To check: Ctrl+F → «poring» → «What it drops» with icons; «kafra» — the nearest Kafra first; the prt_fild08 card —
  «The nearest Kafra: Prontera City · moves: 1».

# uaRO Graphics 3.0.11-beta.14 — what is new

- **Pictures in «Reference»**: a monster's card shows the monster as it stands in the game (the first frame of the
  client's own sprites, data\sprite\몬스터), an item's card its icon (data\texture\…\item, by the client's
  idnum2itemresnametable.txt). Beside the picture — HP, element, race, size and experience.
- A monster's sprite name comes from the server's database (mob_db) — the site gives it in the pack after its update
  (the same one as the timers and the market); till then from the monster's name (some two thirds are found so).
- To check: Ctrl+F → «poring» — a pink Poring left of «HP 50 · Water 1 · …»; «baphomet» — Baphomet; «fly wing» — the
  wing. The log has the lines `reference: Poring's picture - data\sprite\…\poring.spr` and `… item icons named …`.

# uaRO Graphics 3.0.11-beta.13 — what is new

- **«Damage by element» in a monster's card**: how much per cent of an attack of each element it takes — by the
  server's table (its db/attr_fix.txt), the most first; 150 % and more in gold; when an attack heals the monster (on
  this server it does, attribute_recover) — «heals 25 %». It shows which skills and weapons to take on it.
- To check: Ctrl+F → «poring» (Water 1) — first «Wind 175 %»; «all 10 ›» — down to «Water 25 %».

# uaRO Graphics 3.0.11-beta.12 — what is new

- **The site's MVP timers and market in the game** (after the site's update — a new page /api/public/game/live.json;
  the app takes it with the players now, every 2 minutes):
  - a boss's card — «Players' timers»: those the players opened to all (public) on the «MVP timers» page — the map,
    «in 73 min» or «should be up» (in gold) and «killed 47 min ago»; no players' names;
  - the list under the new minimap: in the line of a boss someone noted on this map — how long yet (light blue) or
    «up»;
  - an item's card — «On the site's market»: from what price (a lot by the piece: one piece's), how many lots and
    pieces.
- **How far**: «Where to find» and «Where to buy» say how many moves from your map on foot, and when Kafra makes it
  fewer «moves: 2 · Kafra»; in «Where to buy» of the cheapest the nearest first.
- To check: Ctrl+F → «fly wing» — «Where to buy» with «moves: N»; after the site's update — «On the site's market» for
  an item on the market; a timer opened to all on the site — in the boss's card.

# uaRO Graphics 3.0.11-beta.11 — what is new

- **The world map in «Reference»** (the first screen's tenth tile): the world of the maps' own minimaps, as on the site
  (the layout from uaro.kiev.ua, the pictures the client's own minimaps):
  - your map in a gold frame, you a white dot on it;
  - the way followed («on the minimap ›» of beta.9) a light blue line from map to map;
  - the numbers on the maps — players now (of beta.10);
  - point at a map — its name, its kinds of monsters, players under the map; a click opens its card (with «How to get
    there»);
  - «Dungeons ›» — the dungeons by groups, as on the site, with the same numbers.
- To check: Ctrl+F → «World map»: the gold frame where you are; with «on the minimap ›» on — a light blue line to the
  target; a click on Prontera opens its card. The log has the line
  `reference: the world map - 184 maps on the sheet, …`.
- The Ukrainian words «впускає» are «випадає» now.

# uaRO Graphics 3.0.11-beta.10 — what is new

- **Players now** — from the site's public «Online» page: while the game runs, the app (its game watcher) asks
  uaro.kiev.ua every 2 minutes how many players are on each map and keeps the numbers alone — no names
  (uaro-graphics\reference\online.json). With the new minimap or «Reference» on:
  - a map's card says «players now: 5»;
  - the first screen of «Reference» says «In the game now: 104»;
  - when the site does not answer for 10 minutes, no numbers are shown.
- To check: Ctrl+F — the first screen has «In the game now: …» (the site's number); the card of a map with players
  now (the site: Rankings → Online) says «players now: N».

# uaRO Graphics 3.0.11-beta.9 — what is new

- **The way followed on the new minimap**: in a map's card («Reference», Ctrl+F) the rows «On foot» and «By Kafra»
  have «on the minimap ›» (with «New minimap (prototype)» on). A click closes the window and the minimap shows:
  - a light blue line from you to the next move and a ring on it; a move past the minimap's edge — the line to the
    edge toward it;
  - two lines under the map: where the move leads and how many are left («→ Payon Town · 2 left»), and where it is
    («Kafra 146, 86 · 1,200 z», «exit 233, 16»);
  - the way is made again wherever you go (a teleport, a death, another exit, a tower's stairs); there — «You are
    there» for 8 seconds, and the way ends.
  A click on the way's lines opens the target's card, where «followed · stop ›» ends it.
- A click on a monster in the list under the minimap opens its card («and N more» — the map's card). The notes of
  beta.7 told of it, but it did not work.
- To check: on prt_fild08 Ctrl+F → «pay_dun00» → «By Kafra … on the minimap ›»: the line leads to the north exit to
  prontera, under the map «→ Prontera City · 4 left»; in Prontera — to the Kafra by the south gate. The log has the
  lines `route: …`.

# uaRO Graphics 3.0.11-beta.8 — what is new

- **«How to get there» in a map's card** (the «Reference» window, Ctrl+F): the way from the map you stand on:
  - «On foot» — the free moves (the maps' exits, the stairs within a map, NPCs that take you for free): how many moves
    and where each is (exit 156, 22);
  - «By Kafra» — when a Kafra teleport (or a paid NPC) makes the way shorter: the moves and the price (Kafra 146, 86 ·
    1,200 z);
  - a move with a condition (a quest, an item) only when there is no other way, and marked «a condition»; an NPC that
    leads now here, now there (Eden's way out, the Clock Tower's random warps) is never a step;
  - the way goes only where one really gets: the parts of one map (the Geffen tower's floors, que_ng's two guilds)
    are joined by its stairs alone — by the server's maps cell by cell (a new file uaro-graphics\walkable.bin, 1.3 MB);
  - a map's name in a step opens its card; a long way shows 5 steps and «all N ›».
- To check: Ctrl+F → «pay_dun00» → the Payon Cave card: «On foot» 7 moves by prt_fild08 and moc_fild01, «By Kafra» 3
  moves · 1,200 z; from Geffen to «gef_dun00» 5 moves, three of them the tower's stairs. The log has the line
  `reference: the walkable parts of 697 maps`.

# uaRO Graphics 3.0.11-beta.7 — what is new

- **The game's reference in the game** («Interface» → «Reference (prototype) - Ctrl+F», off by default): Ctrl+F opens
  a window over the game with uaro.kiev.ua's database (what 3.0.11-beta.5 fetches when the game starts):
  - search: type a part of a name — monsters, items, maps, shops; letters come in Latin with any keyboard layout (the
    names in the database are English), Enter opens the first found;
  - a monster's card: level, HP, element, race, size, experience; «Where to find» — your map first, in gold, and how
    many live there; «What it drops» with the chances; the pet (lure, egg, food);
  - an item's card: «Dropped by» (chance, level, where it lives), «Where to buy» (NPC, map, price), crafting;
  - a map's card: «Who is here», «Exits», NPCs and shops; the map's name from the client's mapnametable;
  - long sections show 5 lines and «all N ›»;
  - a name in a card opens its own card; Esc or Ctrl+F closes it, Alt+← or the mouse's back button goes back, the
    wheel and PgUp/PgDn scroll;
  - a click on a monster in the list under the new minimap opens its card.
- To check: Ctrl+F → «poring» → Poring: «Where to find» starts with your map (on prt_fild08 «you are here · ×100»);
  «What it drops» has Jellopy 70 %; «all 8 ›» opens the whole list; a click on Jellopy opens the item, Alt+← comes
  back. While the window is open, keys and clicks inside it do not reach the game.

# uaRO Graphics 3.0.11-beta.6 — what is new

- **Hints on the new minimap** («Interface» → three new switches under «New minimap (prototype)»), from the game's
  reference (3.0.11-beta.5 fetches it from uaro.kiev.ua when the game starts):
  - NPC marks: dots in the colours of the site's map (Kafra pink, healers green, shops blue…); the pointer over a dot
    names the NPC on the line under the map;
  - warps: red rings where a warp leads to another map; the pointer over a ring says where («→ Prontera Field
    (prt_fild07)»);
  - monsters under the minimap: who lives on the map and how many the server keeps there; ★ — a boss and its respawn
    after death (Satan Morroc on moc_fild20: 120 min).
  Each switch works at once, in the console (~) too.
- To check: on prt_fild08 the list says Poring 100, Lunatic 40, Pupa 20, Drops 10, and the rings stand at the map's
  edges; the pointer over the ring by Prontera's gate shows «→ Prontera City (prontera)». The
  log has the line `reference: the pack of 2026-10-23 …`.

# uaRO Graphics 3.0.11-beta.5 — what is new

- **The game's reference comes from uaro.kiev.ua.** The site now takes its game database (items, monsters, drops,
  spawns, warps, Kafra and NPC routes, shops, pets, crafting) straight from the server's repository, the way the server
  loads it, and keeps it up to date by itself. With the new minimap on, the game's start fetches that database as one
  pack (about 270 KB) into `uaro-graphics\reference\` — only when the site has a newer one. While the site is down,
  the copy there stays. The new interface's panels will read it.
- To check: start the game with «Interface» → «New minimap (prototype)» on, then open «Interface» in the app — the line
  under the switches says «Game reference: the server's data of 2026-10-23 (commit d68ea17e), 264 KB».

# uaRO Graphics 3.0.11-beta.4 — what is new

- **Ctrl+Tab switches the new minimap** («Interface» → «New minimap (prototype)») the way the game switched its own:
  opaque → see-through → hidden → opaque again. Hidden, it takes no clicks. The game does not get that Ctrl+Tab, so
  the old minimap does not flash.
- To check: press Ctrl+Tab three times — the minimap turns see-through, goes, comes back. The log has the lines
  `minimap: Ctrl+Tab`.

# uaRO Graphics 3.0.11-beta.3 — what is new

- **Its own item «Interface» in the app.** It is in every mode, «Simple» too, right after «Effects». It holds the
  switches of the new interface's parts; one so far — «New minimap (prototype)». Each works at once, in a running game
  too.
- In the game the switch is in the console (~) as before: the section «The new interface» under the graphics test's
  row.
- To check: the app → «Interface» → «New minimap (prototype)» — in the game the old minimap goes, the new one with the
  character's cell stands in its corner.

# uaRO Graphics 3.0.11-beta.2 — what is new

- **The switch «New minimap (prototype)» shows now.** In 3.0.11-beta.1 no page listed it.
  - In the game: the console (the key ~ left of 1) — the section «The new interface» right under the graphics test's
    row; Space turns it on and off.
  - In the app: «Game and chat» → the section «The new interface» (in the «Detailed» mode).
- To check: turn it on in the console — the old minimap goes, the new one with the character's cell stands in its
  corner; compare with `/where`.

# uaRO Graphics 3.0.11-beta.1 — what is new

- **A prototype of the new minimap (the new interface's first step).** Turned on in «Game» → «New minimap
  (prototype)» (`[interface] minimap = 1`), off by default. The game's own minimap hides the way Ctrl+Tab hides it, and
  a new one stands in its corner: the map round the character, the character as a white dot, the map's name and the
  character's cell under it. A click on the new minimap does not walk the character. Off — the game's minimap is back.
- It checks three things: the old minimap is gone and takes no clicks; the cell under the map is the one `/where`
  says; a click on the new minimap does not move the character. The log tells what happened (lines `minimap:`).
- To check: turn it on, walk round Prontera and a field, type `/where` and compare with the cell under the minimap;
  click the minimap; go to another map; press Ctrl+Tab; turn it off — the old minimap is back. Send a report from the
  game.

# uaRO Graphics 3.0.10 — what is new

«Reports» came alive: screenshots in the conversation, new answers without switching, notifications even while the app
is minimised.

- **Screenshots in the conversation.** The thumbnails show again — no grey boxes and «No connection to uaro.kiev.ua …
  404». A thumbnail is the small JPEG, so it comes at once; a click opens the screenshot in its original quality.
- **New answers come by themselves.** While «Reports» is open the app asks the site every 15 seconds — and at once when
  the window comes back to the front; the open conversation follows without switching to another report. What you are
  writing and the screenshots attached stay. «Refresh» reads the open conversation too.
- **The yellow bar «The developer waits for your answer»** reads in any window: it grows with its words up to three
  lines, longer ones end with «…» (all of them are in the conversation above).
- **The app learns of answers while minimised.** Every 5 minutes it asks the site: the number on «Reports» and the strip
  «An answer to the report» follow. When the game is not running and the app is not in front, a Windows notification
  tells of a new answer; a click opens the conversation. While the game runs the game tells, as before. Turned off in
  «Game» → «Notifications» → «Answers to reports».

# uaRO Graphics 3.0.10-beta.3 — what is new

- **The app learns of answers while minimised too.** While the app is open (minimised or behind other windows too) it
  asks the site every 5 minutes for answers to your reports: the number on «Reports» and the orange strip «An answer to
  the report» follow.
- **A Windows notification of a new answer** when the game is not running and the app is not in front: «Answer to the
  report R-XXXXXX: <status>» with the developer's words. A click on it (or on the icon by the clock) opens that
  conversation; the app's taskbar button flashes. While the game runs the game tells of it as before (its card or its
  notification). Turned off in the same place: «Game» → «Notifications» → «Answers to reports»; its sound — «Sound of
  notifications».
- Only a computer that has sent a report asks.
- To check: close «Reports», minimise the app (no game running) — after the developer answers, the notification comes
  within 5 minutes; a click opens the conversation.

# uaRO Graphics 3.0.10-beta.2 — what is new

- **«Reports» sees new answers by itself.** While the window is open the app asks the site every 15 seconds — and at
  once when the window comes back to the front. The open conversation is read again as soon as something new is in it
  (an answer, a status): no more switching to another report and back. It reads only while the window is in front —
  else the answer would turn «read» and the game would not show its card. «Refresh» reads the open conversation too.
  What you are writing and the screenshots attached stay.
- **The yellow bar «The developer waits for your answer» reads in any window.** It grows with its words — up to three
  lines of the window's width; longer words end at a word with «…» (all of them are in the conversation above). The
  signature «— команда uaRO» stays out of the bar.
- To check: open a conversation, let the developer answer — the answer comes in ~15 s by itself; in a small window the
  bar with the question reads.

# uaRO Graphics 3.0.10-beta.1 — what is new

- **«Reports»: screenshots in the conversation.** The thumbnails show again (fixed on the site — 3.0.9 too): no grey
  boxes and «No connection to uaro.kiev.ua … 404». A thumbnail is the small JPEG, so it comes at once; a click opens
  the screenshot in its original quality (the PNG, when the app sent it).
- To check: «Reports» → a report with a screenshot — the thumbnail comes at once; a click opens it in «Photos».

# uaRO Graphics 3.0.9 — what is new

New support: a report's answer shows in the game, «Reports» as a messenger, a new report made simpler. And «World
resolution» is 100 % for everyone now.

- **A report's answer shows where you are.** When the developer answers, you learn of it at once:
  - **in the game** — a card «Answer to the report» at the top centre with the status and the developer's words.
    «Open» (or Ctrl+Shift+F10 while the card shows) opens that report; «Later» hides the card until the next game
    start;
  - **while the game is behind other windows or minimised** — a Windows notification, as for whispers. Turned off in
    «Game» → «Notifications» → «Answers to reports»;
  - **in the app** (and on the page «Play» that uaRO.exe opens) — an orange strip «An answer to the report» with
    «Open».
  The game asks for answers at its start and then every 10 minutes, only on a computer that has sent a report.
- **«Reports» as a messenger.** On the left the reports as cards with the tabs «Open» / «Closed» (an orange dot: a new
  answer). On the right the report's status and path: Sent → Read → Needs information → Being fixed → Fixed → Closed.
  The conversation as bubbles, screenshots as thumbnails in their message; when the developer waits for your answer —
  an orange bar with the question. «Send · Ctrl+Enter», «Paste · Ctrl+V».
- **«New report» made simpler.** One question «What happened?» with an example inside the field. The game's
  screenshot (Ctrl+Shift+F10) is attached already and shown as a picture; «+ Game screenshot…» shows the screenshots
  of the week as tiles, and a file or Ctrl+V can be added too (6 at most). After sending — «Thank you! The report is
  sent» with its code; without internet — «Kept - it goes by itself».
- **«World resolution» is 100 % for everyone.** The automatic setup gave old cards (Direct3D 11 without 12) 75 %: the
  world was drawn smaller and made larger, and the character looked smeared when the camera was far. Now 100 % on
  every card; a value under 100 % goes back to 100 % once. A share you choose yourself after this stays.
- **«Own ground»: no magenta strips.** Bright magenta strips ran along Izlude's bridge and wall — the places of
  textures the game makes see-through. They are see-through now, as in the game.
- **The wash under Berserk.** If the world washes out white under Berserk or after it, send a report from the game:
  the log now records what washes it.

# uaRO Graphics 3.0.9-beta.7 — what is new

- **«World resolution» is 100 % for everyone.** The automatic setup gave old cards (Direct3D 11 without 12) 75 %:
  the world was drawn smaller and made larger, and the character looked smeared when the camera was far.
  - Once, at the first game start (or when the settings app opens), a value under 100 % goes back to 100 %.
  - The automatic setup now gives 100 % on every card.
  - A share you choose yourself after this stays.
- To check: on an old card the settings app's page «Quality» shows «World resolution» 100 %; the log has
  «world_scale 75 -> 100 once».

# uaRO Graphics 3.0.9-beta.6 — what is new

- **«New report» — simpler and friendlier.**
  - One question «What happened?» with an example inside the field.
  - The game's screenshot (Ctrl+Shift+F10) is attached already and shown as a picture. No ticks: «+ Game
    screenshot…» shows the game's screenshots of the week as tiles, there are «Add a file…» and «Paste · Ctrl+V» too;
    «Remove» (or Delete) takes the chosen one off. 6 at most.
  - «What is it about?» — buttons you may leave; from the game «Graphics» is chosen already.
  - The long paragraph of what goes with the report is one line now; the whole text — under «What exactly?».
  - Ctrl+Enter sends.
  - After sending — «Thank you! The report is sent» with its code and «Open the conversation» / «Done». No
    internet — «Kept - it goes by itself».
- To check: Ctrl+Shift+F10 in the game — the screenshot is in the report already; «+ Game screenshot…» — tiles; after
  sending — «Thank you!».

# uaRO Graphics 3.0.9-beta.5 — what is new

- **«Reports» as a messenger.** What you wrote, what the developer answered and where the report is:
  - on the left — the reports as cards with the tabs «Open» / «Closed»: an orange dot for a new answer, the title,
    the status in colour, the time and the last message's start;
  - top right — the report's title, its status and its path: Sent → Read → Needs information → Being fixed → Fixed →
    Closed, the current step marked;
  - the conversation as bubbles: yours on the right, the developer's on the left; screenshots as thumbnails in their
    message (a click opens one); a status change as a thin line across;
  - when the developer waits for your answer — an orange bar with the question over the answer box;
  - «Send · Ctrl+Enter», «Paste · Ctrl+V»; «Add a screenshot…» opens in the game's ScreenShot folder.
- The status «New» is now «Sent» for you.
- To check: open «Reports» — cards in the list, bubbles in the conversation, the screenshots' thumbnails come and open
  on a click; Ctrl+Enter sends the answer.

# uaRO Graphics 3.0.9-beta.4 — what is new

- **A report's answer shows where you are.** When the developer answers, you learn of it at once:
  - **in the game** — a card «Answer to the report» at the top centre with the status and the developer's words.
    «Open» (or Ctrl+Shift+F10 while the card shows) opens that report in «Reports»; «Later» hides the card until the
    next game start;
  - **while the game is behind other windows or minimised** — a Windows notification, as for whispers. Turned off in
    «Game» → «Notifications» → «Answers to reports»;
  - **in the app** (and on the page «Play» that uaRO.exe opens) — an orange strip «An answer to the report» with
    «Open».
- The game asks for answers at its start and then every 10 minutes, only on a computer that has sent a report.
- To check: send a report, wait for an answer and start the game — the card shows; press «Open».

# uaRO Graphics 3.0.9-beta.3 — what is new

- **«Own ground»: no magenta strips.** Some ground textures have see-through places marked in magenta (in Izlude the
  bridge and the wall: 6-8 % of the texture). The game makes them see-through, while «Own ground» drew them magenta -
  bright magenta strips ran along Izlude's bridge and wall. Those places are now see-through, as in the game.
- To check: Izlude, the bridge by the water (as in report R-6JBVTG) - no magenta strips.

# uaRO Graphics 3.0.9-beta.2 — what is new

- **The log: a white wash over the world.** Once after Berserk the screen stayed white, in «pause» too, and the night
  did not darken it - the white lay over everything. The driver now writes into the log once every 10 s the game's draws
  that, with the interface, cover half the picture or more: how many, where the widest lies, its colour and how it
  blends.
- If the screen washes out again, just send a report from the game - the log will show what washes it.

# uaRO Graphics 3.0.9-beta.1 — what is new

- **The log: what the game washes the world with under Berserk.** Under Berserk the game floods the world with yellow
  light, and the driver's effects take it nearly to white. To make that wash soft rather than take it away, the driver
  writes into the log once every 10 s the widest of the game's draws that add light: where it lies, its colour, how it
  blends and how many of them cover half the picture or more.
- To check: on moc_fild11 turn Berserk on, stand some 15 s and send a report from the game; then leave Berserk and send
  another.

# uaRO Graphics 3.0.8 — what is new

More notifications while the game is behind: an attack, a buff ending, the party, the chat, the connection, HP and SP
back.

- **New notifications while the game is behind.** In the configurator, page «Game», section «Notifications», under
  «Notifications while the game is behind» - new boxes, all on by default:
  - «An attack on the character» - when the character comes under attack (misses too): once, again only after half
    a minute without hits. While it goes on, the game's taskbar button flashes.
  - «A buff ending» - «Seconds before a buff ends» (10 s by default) before one of the character's buffs runs out -
    its name (Blessing, Increase AGI, potions, food). Short buffs (under half a minute) are not told.
  - «A death in the party» and «Low HP in the party» - of one of the party near; the threshold «Party HP threshold»
    (30 % by default).
  - «The party's chat» and «The guild's chat» - what the others write (not your own).
  - «Your name in the chat» - when someone near writes the character's name in the common chat.
  - «The connection to the server lost» - when the server closed the connection.
  - «HP and SP back» - when HP and SP are both back after a real fall (sitting, a rest); back to how much - «Back to»
    (50-100 %, 100 % by default).
  - «Notification sound» - Windows' own sound with each notification; off - silent.
- **Several game windows.** Each tells of its own character (an attack, buffs, the name, the connection, HP and SP);
  the party's and the guild's news the main window alone - not one notification from each helper.
- **The log explains the notifications.** For each event it shows why no notification came (the game in front, its box
  off, not the main window) or that Windows took or refused it, and whether Windows shows notifications at all now -
  a help with a report sent from the game.

# uaRO Graphics 3.0.7-beta.8 — what is new

- **Notifications: the log tells what happens to them.** When the game goes behind, the log says so. For each event
  (an attack, a buff ending, HP and SP...) it shows why no notification came - the game in front, its box off, not the
  main window - or that Windows took or refused it, and whether Windows shows notifications at all now (a full-screen
  program, the presentation mode and the like).
- To check: minimise the game (Alt+Tab), wait for an event (a monster's attack, HP and SP back, a whisper) and send a
  report from the game.

# uaRO Graphics 3.0.7-beta.7 — what is new

- **The notification «HP and SP back».** While the game is behind and the character's HP and SP are both back (sitting,
  a rest), a notification comes - only after HP or SP fell well; once, again after the next fall. In the
  configurator: page «Game», section «Notifications» - «HP and SP back» (on by default) and «Back to» (50-100 %, 100 %
  by default - full HP and SP).
- To check: spend SP and take a few hits, sit down, minimise the game - a notification once HP and SP are full.

# uaRO Graphics 3.0.7-beta.6 — what is new

- **New notifications while the game is behind.** In the configurator, page «Game», section «Notifications», under
  «Notifications while the game is behind» - new boxes, all on by default:
  - «An attack on the character» - when the character comes under attack (misses too): once, again only after half
    a minute without hits. While it goes on, the game's taskbar button flashes.
  - «A buff ending» - «Seconds before a buff ends» (10 s by default) before one of the character's buffs runs out -
    its name (Blessing, Increase AGI, potions, food). Short buffs (under half a minute) are not told.
  - «A death in the party» and «Low HP in the party» - of one of the party near; the threshold «Party HP threshold»
    (30 % by default).
  - «The party's chat» and «The guild's chat» - what the others write (not your own).
  - «Your name in the chat» - when someone near writes the character's name in the common chat.
  - «The connection to the server lost» - when the server closed the connection.
  - «Notification sound» - Windows' own sound with each notification; off - silent.
- With several game windows each tells of its own character (an attack, buffs, the name, the connection); the
  party's and the guild's news the main window alone - not one notification from each helper.
- To check: minimise the game and let a monster attack the character; cast Blessing and minimise - a notification
  10 s before it ends; write to the party or the guild from another character; change the thresholds in the
  configurator.

# uaRO Graphics 3.0.7 — what is new

Walls and the sun as in the game, fireflies as lamps, and no balls of light in the air: fixes from the reports on
c_tower2.

- **«Own ground»: walls without stripes.** The walls running along one of the map's axes lay the other way round:
  their texture and the map's light on them were turned round along the wall, and vertical stripes ran along
  c_tower2's long walls. The walls now lie as the game lays them, their patterns going on across the joins.
- **«Own ground»: walls lit as in the game.** The walls took the light of the floor beside them, while the game lights
  each wall by its own way to the map's sun. On c_tower2 the walls turned from the sun were 1.6-1.8 times as bright as
  the original. Now as in the game; the floor as before.
- **The map's sun on the same side as in the game.** The driver turned the sun from the map's file not as the game
  does. On some 200 maps where the sun's longitude is not 45, «Shadows from the sun and fire», «Materials: stone,
  brick, bark» and «Hills without steps of light» fell and were lit from the wrong side - on 79 maps the opposite way.
  Now as uaRO.exe works it out and as the maps' own painted shadows fall.
- **Hidden lights: light without the orbs.** The balls of light over the hidden lights (Manuk, Splendide, Nidhoggur's
  nest, the towers) are not drawn; their light on the ground, the walls and the characters stays. The orbs come back
  with «Orbs in the air» under «Hidden lights of the maps» (off by default).
- **The maps' fireflies light like lamps.** Each of the maps' fireflies - those the game shows and the yellow ones it
  never showed - lights the ground, the walls and the characters around its place in a colour of its own: warm
  yellow, yellow-green, green, amber or pale mint. The fireflies themselves are not seen - «Fireflies in the air»
  under «The maps' fireflies» brings them back (off by default). How strong the light is: «Fireflies' brightness».
- **No balls of light in the air.** «Glowing air at fire and lamps» glowed in a ball around each lamp of the map's file
  (they often hang where no lamp is seen) and around the hidden lights and the fireflies. The switch is now «Glowing
  air at fire»: the air glows at torches, burning skills, lava and portals. The balls around lamps, lights and
  fireflies are «At lamps and lights» under it (off by default).

# uaRO Graphics 3.0.7-beta.5 — what is new

- **No balls of light in the air.** The balls on c_tower2 were «Glowing air at fire and lamps»: the air glowed in a
  ball around each lamp of the map's file, and those lamps often hang where no lamp is seen (the map's makers baked the
  map's light with them) - the same around the hidden lights and the fireflies. The switch is now «Glowing air at
  fire»: the air glows at torches, burning skills, lava and portals. The balls around lamps, lights and fireflies are a
  switch of their own under it, «At lamps and lights», off by default. The lamps' light itself on the ground, the walls
  and the characters as before.
- To check: c_tower2 with «Glowing air at fire» - no grey balls in the dark over the walls any more; the air at the
  torches glows as before.

# uaRO Graphics 3.0.7-beta.4 — what is new

- **The map's sun on the same side as in the game.** The driver turned the sun from the map's file not as the game
  does: the longitude's sine and cosine were swapped. On the 722 maps with longitude 45 that is the same thing, but on
  some 200 others «Shadows from the sun and fire» fell, «Materials: stone, brick, bark» and «Hills without steps of
  light» were lit from the wrong side, and «Houses and trees far off» got a rough light - on 79 maps (longitude 315:
  pvp, job_sword1, force_map ...) the opposite way. The sun now goes as uaRO.exe turns it - the way the maps' own
  painted shadows fall (checked on amatsu, alberta, prt_monk, niflheim, louyang, job_sword1, ecl_fild01).
- To check: Amatsu, Alberta, Niflheim or Louyang - a character's shadow falls the way the shadows of houses and trees
  painted on the ground do.

# uaRO Graphics 3.0.7-beta.3 — what is new

- **Walls lit as in the game.** «Own ground» gave the walls the light of the floor beside them. The game lights each
  wall by its own way to the map's sun, so in the game the walls turned from the sun are darker. On c_tower2 our walls
  were 1.6-1.8 times as bright as the original. The walls now get the same light as in the game (as uaRO.exe works it
  out), the floor as before. After entering a map the log has a line on how many of the game's walls matched ours.
- **The maps' fireflies light like lamps.** Each of the maps' fireflies - those the game shows and the yellow ones it
  never showed - now lights the ground, the walls and the characters around its place like a lamp, in a colour of its
  own: warm yellow, yellow-green, green, amber or pale mint. The fireflies themselves are no longer seen in the air;
  they come back with a new switch «Fireflies in the air» under «The maps' fireflies» (off by default). How strong the
  light is - «Fireflies' brightness».
- To check: c_tower2 - the walls as dark as in the original (the Pause key turns the improvements off and on); Byalan
  (iz_dun00-02), Glast Heim, the Ant Hell or Nidhoggur's nest - soft coloured pools of light in place of the dots; with
  «Fireflies in the air» the fireflies fly again.

# uaRO Graphics 3.0.7-beta.2 — what is new

- **Walls without stripes.** «Own ground» laid every wall running along one of the map's two axes the other way round:
  its texture and the map's light on it were turned round along the wall. On c_tower2 each piece of a wall therefore
  brightened the other way to the next one - vertical stripes along the long walls. The walls now lie as the game lays
  them: the light runs smoothly along a wall as in the original, the textures' patterns go on across the joins. The
  same on gl_church, prt_church, payon_in01 and the other maps with walls.
- To check: c_tower2 (the Clock Tower's 2nd floor) - the long walls without stripes, the light along them smooth as
  without our graphics; the walls in gl_church or prt_church as well.

# uaRO Graphics 3.0.7-beta.1 — what is new

- **«Hidden lights of the maps»: light without orbs.** The balls of light floating over the hidden lights (Manuk,
  Splendide, Nidhoggur's nest, the towers) are no longer drawn; the light they throw on the ground, the walls and the
  characters stays. The orbs come back with a new switch «Orbs in the air» under «Hidden lights of the maps», off by
  default. Splendide's pools of light and the towers' dust as before.
- To check: Manuk or man_fild02 with «Hidden lights of the maps» - no orbs, soft light round them; with «Orbs in the
  air» - the orbs as before.

# uaRO Graphics 3.0.6 — what is new

Fixes from the players' reports: the world black after an hour of play, a hole on the stairs, washed-out sand. Also a
more exact «Graphics test» and less work in the game's thread.

- **The world black after an hour of play (Direct3D 12).** Whoever stood on one map when another hour from the game's
  start went by saw a black world (only the interface and the pollen in the air) until the next map. The effects'
  clock began its count anew every hour, and «Smooth map change» took the map for not begun yet. Now that clock never
  starts anew. With it, on the hour's turn the steps in the sand and the snow no longer freeze and the flocks of birds
  and butterflies no longer stay away for an hour.
- **«Own ground»: no holes on stairs.** On the fortress stairs in Comodo a round patch of rock showed over the top
  steps: the smooth ground bulged over the steep slope under the stairs. Now, where a model stands on the ground
  (stairs, bridges, floors, walls), the ground rises no higher than the game's cells; the rest is as smooth as before.
- **«Bloom of bright places» no longer washes out pale sand.** By day on beaches the sand went nearly white: it is a
  little over the bloom's threshold, so the whole beach glowed. Now only what is clearly brighter than what is round
  it glows: fire, lamps and glints as before, even pale sand, snow or a white wall in the sun not.
- **«Graphics test»:**
  - waits until a set has loaded (the own ground, its pictures, the AI textures), at most 30 s. On weak computers
    «High» was measured while loading;
  - an effect's cost within the card's noise shows as «≈0», not as a minus;
  - the dearest part of the frame with DLSS or DLAA shows with its whole name.
- **Less work in the game's thread (Direct3D 12, DLSS).** The frame for the 64-bit renderer is written 8 times as
  fast and read twice as fast.
- **A closer measurement.** «Measure where the game spends its time» counts the driver's functions whole, with what
  they call.
- **The forest beams' shader** is compiled at once at the optimisation level that works, without a failed attempt at
  the first start.

# uaRO Graphics 3.0.6-beta.5 — what is new

- **«Bloom of bright places» no longer washes out pale sand.** On cmd_fild02's beach by day the sand went nearly
  white: its brightness of 0.90-0.95 is over the bloom's threshold of 0.88, so the whole beach glowed. Now a place
  glows only when it is clearly brighter than what is round it. Fire, lamps and glints over darker ground glow as
  before; an even bright surface (sand, snow, a white wall in the sun) does not. On the shots: the washed places on the
  beach from 40 % of the picture down to 1.7 %, the fire pillar in the dungeon glowing just as it did.
- To check: the same beach in Comodo by day on «High»; and a look that fire and lamps at night glow as before.

# uaRO Graphics 3.0.6-beta.4 — what is new

- **«Own ground»: no hole on the stairs in Comodo.** On the fortress stairs of cmd_fild02 a round patch of rock showed
  over the top steps. The stairs lie on a steep slope, and the smooth ground bulged 0.2-0.3 over the game's cells at
  the slope's top edge, while the steps lie only 0.1 over them. Now, where a model stands on the ground (stairs,
  bridges, floors, walls), the smooth ground rises no higher than the game's cells, and the stairs look as in the game
  itself. The rest of the ground is as smooth as before. Checked offline on the real map: the ground now covers the
  steps exactly where the game's cells do.
- The log now says for each map how many cells lie under models (the line «own ground: … cells under models»).
- To check: cmd_fild02, the same stairs from the same angle - no hole; and a look at other stairs and bridges.

# uaRO Graphics 3.0.6-beta.3 — what is new

- **«Graphics test» waits until a set has loaded.** On a player's FirePro W5130M laptop «High» was measured while its
  own ground and its pictures were still loading: that took 5-7 s and the test waited 3 s, hence 34 fps and stutter.
  Now a set is measured once its renderer says all is made (the own ground, its pictures, the AI textures of what is
  seen), a second after that. At most 30 s: past that it is measured as it is, and the log says so.
- **Effects' costs in the test: «≈0» in place of a minus.** A difference within the card's noise (such as «Ground
  relief» -0.3 on a computer held back by its processor) now shows as «≈0».
- **Less work in the game's thread.** The frame for the 64-bit renderer (Direct3D 12, DLSS) is written by kinds at
  once: written 8 times as fast, read twice as fast; the draws are recorded without a needless zeroing. On weak
  processors about half a millisecond of every frame.
- **A closer measurement.** «Measure where the game spends its time» now counts the driver's functions whole, with what
  they call (memcpy and the like). The next test shows where to cut next.
- **The forest beams' shader** is compiled at once at the optimisation level that works: level 3 fails inside the
  Windows compiler on every computer. The same code, without a failed attempt at the first start.
- To check: the «Graphics test» on Direct3D 12. The log may say «measured from N s» for «High», and the table's «High»
  should be free of the loading's stutter.

# uaRO Graphics 3.0.6-beta.2 — what is new

- **The world black after an hour of play (Direct3D 12).** A player minimised the game, went to the browser, came
  back - the world black, only the interface and the pollen in the air seen; after a portal all was right again. The
  cause: Direct3D 12's effects' clock began its count anew every hour, and «Smooth map change» measured the time since
  the map was entered by it - past such a turn it came out «not yet», and the world stayed dark until the next map.
  Minimising had nothing to do with it: this happened to everyone standing on one map when another hour from the
  game's start went by. Now that clock never starts anew. With it, on the same hour's turn the steps in the sand and
  the snow no longer freeze, new flocks of birds and butterflies no longer stay away for an hour, and torches gone out
  no longer stay in memory unseen.
- To check: Direct3D 12, any set of effects with the «Game fixes» group on (as it is by default), come to a map and
  play on it for over an hour from the game's start - the world should not go black.

# uaRO Graphics 3.0.6-beta.1 — what is new

- **«Graphics test»: the dearest part of the frame with its whole name.** On graphics cards with DLSS or DLAA the column
  «the dearest on the card» showed a stub «the world made larger) 3.34» in place of «the picture put together (DLSS,
  TAA, the world made larger) 3.34»: that part's name holds commas, and the table cut the line at the commas
  (R-7JQ6XX). Now the name is whole.
- To check: on a computer with DLSS or DLAA run the «Graphics test» - the table's first part should read «the picture
  put together (DLSS, TAA, the world made larger)».

# uaRO Graphics 3.0.5 — what is new

The heart of this release is speed on weak computers. On a laptop with a Radeon 610M (1080p, Direct3D 11) the
«Medium» set now runs at 60 frames a second instead of 36, and «High» at 28 instead of 15; under Direct3D 12 at 43
instead of 27 and 26 instead of 16. Also a «Graphics test» that measures and sends its result by itself, reports'
screenshots in full quality and the way back from a beta to the release.

- **Effects lighter for the graphics card.** The same picture with the needless work taken out. Direct3D 11 and
  Direct3D 12. Milliseconds of a Radeon 610M's frame, before → now:
  - Grass: 10.2 → 1.8. A pass of its own marks where a blade's root can stand at all, and the picture is cut into
    8×8 squares: the grass does no work where no blade can come from - on paving, roads, sand. The blades are the same.
  - Weather: 2.55 → 0.33. Pollen, snow, rain, leaves and dust are drawn as small squares around the particles instead
    of a pass over the whole screen. Sand is as it was.
  - «Footprints in snow and sand» (dust under the feet on grass): 4.1 → 0.2. Each point looks only at the steps near
    it.
  - «Grass and road seams without steps» on «High»: 7.0 → 4.2. Points where the neighbouring texture does not show are
    dropped at once. Far off, where a ground cell is under 24 points on the screen, the seams fade out smoothly.
  - «Ground relief»: 1.6 → 0.8 and «Map light without steps»: 1.1 → 0.4 - fewer texture reads. Nearly the same look,
    a little softer where the ground is seen at a sharp angle.
  - «Shadows in corners and at walls»: 3.3 → 3.0 - the scene's depth is worked out once instead of twenty times.
  - Night: 2.0 → 1.6 - no copy of the whole picture when no torch light is in the frame.
- **Less processor work under Direct3D 11.** On «High» the driver spent 50-66 ms of a frame, now about 10:
  - the strips of «Grass and road seams without steps» are made once and kept in the card's memory instead of being
    built by the processor every frame;
  - the 32-bit game looked up the seams' and the hills' cells in tables that took only half of the key - hundreds
    one after another;
  - rounding went through a slow library function (the grass alone: 6.8 → 0.4 ms of the processor).
- **«World resolution» below 100 % (Direct3D 11): 6-7 ms a frame less.** The smaller world is made larger with one
  read instead of 45, and «Colour» is laid on in the same pass. The world is a little softer, the interface and text
  stay sharp. Old cards like the HD 6570 gain the most.
- **Direct3D 12: a shader for each state of drawing**, as in Direct3D 11. The login picture on a Radeon 610M is drawn
  4.5 times faster. The shaders are made on a thread of their own: the first start takes a minute or two, then they
  come from the cache. «Debugging» → «A shader for each state».
- **«Keep the frame rate»** turns the grass off at once when the frame rate cannot come back without it (before: last,
  after minutes of slowdown), and now sees the footprints too. An effect you turned on yourself is still left alone.
- **«Graphics test».** In the game's console (~ → «Graphics test») or with the button «Run the graphics test» on the
  «Game» page. Stand somewhere quiet and keep still: for 105 seconds the driver shows by itself «No effects», «Low»,
  «Medium», «Medium without relief», «Medium without smooth light», «High» and «Your settings», the camera going the
  same way each time. Then your camera and settings come back, and the table - FPS, the slowest tenth of the frames,
  the card's and the processor's work, the dearest parts of the frame, the cost of single effects - goes to the
  developers by itself as a «Graphics test» report. The last test shows on «Game». ~ stops the test, and then
  nothing is sent.
- **Frame profile.** «Debugging» → «Measure where the game spends its time» now cuts every frame into parts: the world,
  the interface, each effect, the picture into the window - with the card's and the processor's time. Every 10
  seconds the log gets «profile:» lines.
- **Reports' screenshots in full quality.** After a report the app sends the originals as PNG (until now a JPEG cut
  down to 1600 points), and the site shows them in place of the smaller ones.
- **From a beta back to the release.** The channel «Release» chosen and a beta installed - the app offers «Back to the
  release», even when the release is older than the beta; the game asks the same before the login. The settings stay,
  and the beta is kept as the previous version.

# uaRO Graphics 3.0.5-beta.24 - what's new

- **Lighter «Ground relief».** The test showed its cost on «Medium»: 1.6 ms of a Radeon 610M's frame under Direct3D 11
  and 2.8 ms under Direct3D 12. It took four taps of the ground's texture round the pixel, each with full anisotropic
  filtering. Now three plain ones: the pixel itself and a step right and down. The relief is nearly the same: half a
  texel aside and a little softer where the ground is seen at a slant.
- **Lighter «Map light without steps».** Its cost was 1.1 and 1.4 ms. The lightmap's blur took nine taps; now four - a
  blur as wide, only its shape a little different. It helps «Low» too.
- Direct3D 11 and Direct3D 12.
- To check on the laptop: the «Graphics test» under Direct3D 11 from the same spot - the line «The card's cost within
  «Medium»» had «Ground relief» 1.6, «Map light without steps» 1.1; by eye - the ground's cracks and pebbles and the
  torches' pools of light as before.

# uaRO Graphics 3.0.5-beta.23 - what's new

- **The «Graphics test» measures «Medium» for weaker cards.** In place of «High without hills» and «High without
  materials» (their cost is known: 3.2 and 3.6 ms of a Radeon 610M's frame) the test now goes through «Medium without
  relief» and «Medium without smooth light». Under the table - «The card's cost within «Medium», ms» for «Ground relief»
  and «Map light without steps» and, as before, «The card's cost within «High»» for the seams and the reflections. It
  shows what of «Medium» is dearest for cards weaker than this laptop.
- To check: the «Graphics test» under Direct3D 11 from the same spot by the entrance of prt_fild08.

# uaRO Graphics 3.0.5-beta.22 - what's new

- **«Grass and road seams without steps» fade out far off.** On a field with many grass-to-road edges the seams cost
  9.7 ms of a Radeon 610M's frame on «High» - as much where the camera is far and the cells on the screen are tiny and
  the wandering line hardly shows. Now when a cell takes under 24 pixels of the picture the seams fade out, and under
  12 they are gone: the edge there is the game's own. Nearer, everything as before. The faded pixels are left at once,
  so far off the seams cost next to nothing. Direct3D 11 and Direct3D 12 (and the own ground's seams).
- To check on the laptop: the «Graphics test» under Direct3D 11 from the same spot - the seams on «High» were 9.7 ms;
  by eye - grass and road edges up close as before, and with the camera far back whether steps far off catch the eye.

# uaRO Graphics 3.0.5-beta.21 - what's new

- **Lighter soft shadows.** The shadows are worked out into a mask half the picture's size, and for each pixel of the
  mask the scene's depth was read and worked out anew about twenty times: for the surface's slope and in two blurs.
  Now a small pass of its own works out each mask pixel's depth once, and the shadows and the blur take it ready. Up
  to 1080p the shadows are the same; at 4K the neighbours for the slope are taken a little wider - hardly to be seen.
  And the screen's edge: in the outermost column and row the slope could come out undefined - now the neighbour on the
  other side is taken there. Direct3D 11 and Direct3D 12.
- To check on the laptop: the «Graphics test» under Direct3D 11 from the same spot - «soft shadows» on «Medium» were
  3.34 ms; by eye - the shadows under trees, walls and by houses as before.

# uaRO Graphics 3.0.5-beta.20 - what's new

- **No grass work where no blade can come from.** The grass pass went over every pixel of the screen - a town's paving,
  a road, the sand, with no roots there. Now the picture is cut into 8x8 squares: two small passes mark which squares
  can reach a marked root (down by a blade's length, aside by the sway at its most), and the grass leaves the other
  squares' pixels at once. The same blades. Towns and roads gain the most; on a thick meadow hardly anything changes.
- **No needless copy of the picture at night.** The night pass copied the whole picture for the torches' light every
  time - even with no light in view. Now the light's pass makes the copy itself, only when it has something to light.
  The same look.
- Both under Direct3D 11 and Direct3D 12.
- To check on the laptop: the «Graphics test» under Direct3D 11 in Prontera (a town - that is where the grass shows)
  and, if handy, once more by the entrance of prt_fild08; by eye - the grass on a field and the flowers as before.

# uaRO Graphics 3.0.5-beta.19 - what's new

- **Weather as small quads.** Pollen, snow, rain, leaves and dust were drawn by a pass over the whole screen: each of
  two million pixels looked for its cell and particle in three layers - even where there are none. Now each particle
  is a small quad over where it reaches, and only the pixels near particles work. The same look (the same function
  works the particle out); only where two layers' particles overlap they blend one after the other rather than as a
  sum - not to be seen. Sand stays a pass over the screen: it has a haze over the whole picture. Direct3D 11 and
  Direct3D 12.
- To check on the laptop: the «Graphics test» under Direct3D 11 from the same spot - «weather» on «Medium» was 2.37
  ms; and by eye the pollen over a field, and snow, rain, falling leaves if they come, should look as before.

# uaRO Graphics 3.0.5-beta.18 - what's new

- **Lighter weather.** The pollen over a field cost 2.55 ms of a Radeon 610M's frame on «Medium»: every pixel of the
  screen worked out its particle in each of three layers whole - sines, cosines and an exp - though the particles are
  small and sparse and nearly every pixel is far from them. Now a pixel first checks whether its particle reaches it
  (its size and its own motion at their worst), and a far one goes on to the next layer at once. The look is the same:
  a far particle gave under a ten-thousandth (a leaf, exactly nothing). The same for snow, rain, leaves, sand and dust.
- To check on the laptop: the «Graphics test» under Direct3D 11 from the same spot - «weather» on «Medium» was 2.55
  ms; and by eye the pollen (on prt_fild08) and snow or rain if they come - as before.

# uaRO Graphics 3.0.5-beta.17 - what's new

- **Grass: beta.16's slowdown fixed.** The test showed the grass 0.9 ms dearer in beta.16, not cheaper: the roots' mark
  got cheaper as it should (0.6 ms), but the blades' shader got heavier. Two ways were left in it - the new one and the
  old one as a fallback - and the old cards' shader compiler seems to have worked each root out both ways. Now the
  shader has one way; when the roots' mark cannot be made (only with no video memory at all) the grass is not drawn.
  The same blades.
- **The test's log now says where it was taken.** The «Graphics test» start line gives where the character stands (the
  world's coordinates) - to compare tests from one spot: on the same prt_fild08 the seams cost 4.2 ms at one place and
  9.7 ms at another.
- To check on the laptop: the «Graphics test» under Direct3D 11 from the same spot as the last test (right by the
  entrance from Prontera) - the grass with «grass's roots» was 5.2 ms in beta.15 and 6.1 ms in beta.16.

# uaRO Graphics 3.0.5-beta.16 - what's new

- **Lighter grass.** Two pieces of needless work gone, the same blades:
  - the mark «a blade's root can stand here» was made for every row of the picture, though under Direct3D 11 (and
    with the grass thinned) the roots are taken from every second row only - now it is half as big;
  - for each marked root the grass went through all of its tests again (green ground around it, not under water, the
    map's own ground) for every pixel above it, though the mark had passed them - now it takes only what the blade
    itself needs.
- **A more exact measurement.** The grass is two parts in the measurement now: «grass's roots» (the roots' mark) and
  «grass» (the blades). Under Direct3D 11 the «living ground» is a part of its own - its time went into the soft
  shadows until now («soft shadows 5.6» on «High», the shadows themselves about 3.3 ms).
- To check on the laptop: the «Graphics test» under Direct3D 11 - the grass on «Medium» was 5.4 ms; and by eye: the
  grass and the flowers on a field should look as before.

# uaRO Graphics 3.0.5-beta.15 - what's new

- **«Grass and road seams without steps» lighter for the video card.** The test on a Radeon 610M laptop showed the seams
  the dearest of the four on «High» (7.0 ms a frame; hills 3.1, materials 2.9, reflections 0.7). A seam's strip is half
  a cell wide, and each of its pixels went through the whole ground shader - textures, materials, hills - though over
  most of the strip the noise leaves the cell its own texture and the strip draws nothing there. Now the shader first
  works out whether the neighbour's texture shows at the pixel, and stops when it does not. The look is the same: those
  pixels were drawn fully transparent before too. Direct3D 11 and Direct3D 12.
- To check on the laptop: the «Graphics test» under Direct3D 11 - «Grass and road seams without steps» in the line
  under the table was 7.0 ms; and by eye the edges of grass and roads on prt_fild08 should look as before.

# uaRO Graphics 3.0.5-beta.14 - what's new

- **The «Graphics test» shows what exactly in «High» is dear for the video card.** After «High» the game goes the same
  camera path twice more: «High without hills» and «High without materials» - the test now takes 105 seconds instead
  of 75. Under the table comes a line «The card's cost within «High», ms» for four effects: «Grass and road seams
  without steps», «Hills without steps of light», «Materials: stone, brick, bark», «World reflections in water». The
  hills and the materials are drawn inside the ground and the houses themselves, so their cost shows only as the
  difference with and without them; the seams and the reflections are measured on their own.
- **A more exact measurement.** Under Direct3D 11 the seams' strips on the card and the world's reflections in water
  are parts of their own now, not a part of «the game's world». Under Direct3D 12 DLSS's time is no longer counted into
  the last effect before it: on the main computer «weather 3.10» on «High» and «colour 3.06» on «Low» were really DLSS
  Quality (2.8 ms at 4K).
- To check on the laptop: the «Graphics test» under Direct3D 11 - the line under the table shows which of the four
  costs most.

# uaRO Graphics 3.0.5-beta.13 - what's new

- **«Grass and road seams without steps» on the video card (Direct3D 11).** The strips along the ground's cell edges were
  still built, placed and turned by the camera on the processor every frame - on a Radeon 610M laptop that was most of
  the game's work on «High». Now each piece of a strip is made once, in the world's places, and kept in the card's
  memory; every frame the processor only says which pieces are in view and the fog at their cells' corners, and a
  shader puts the camera on them. The look is the same.
- **Found why the earlier store of strips (beta.9-beta.11) gave almost nothing.** It knew an edge by the numbers of the
  cells' draws, and the game numbers them anew every frame the view moves. Now an edge is known by its cell's place.
  The fix works under Direct3D 12 too, and where the card could not take the pieces (the processor makes the strips
  then, as before).
- In «Measure where the game spends its time» the strips are a part of their own now: «ground seams' strips».
- To check on the laptop: the «Graphics test» under Direct3D 11 (on «High» the game's work was ~49 ms); and by eye - the
  edges of grass and roads on a field (prt_fild08) should look as before, without flicker and without strips out of
  place while the camera moves and turns.

# uaRO Graphics 3.0.5-beta.12 - what's new

- **Lighter grass.** Every pixel of the screen looked for a blade's root up to 16 pixels below it and for each candidate
  read the depth and the colour, worked out its place in the world and several tests - nine candidates of ten fell out
  for reasons of the root's own (not green ground, no blade at that spot, far from its middle and so on). Now a pass of
  its own marks once a frame where a root can stand at all, and the grass looks further only at the marked ones. The
  same tests in one function, so the blades are the same. Direct3D 11 and Direct3D 12.
- To check: the «Graphics test» on the laptop under Direct3D 12 and Direct3D 11 - the grass on «Medium» was 9-10 ms; and
  by eye: the grass and the flowers on a field should look as before.

# uaRO Graphics 3.0.5-beta.11 - what's new

- **Direct3D 11 on «High»: found what ate the processor (R-455GWU, 66 ms a frame).** The grass and road seams and «Hills
  without steps of light» looked their cells and strips up in tables keyed by two numbers (a cell's x and z, a group and
  a texture). In the 32-bit game the standard table takes the low half of a key alone, so all the cells of one row or
  all the strips of one texture fell into one slot and were searched one by one among hundreds. Now the whole key is
  mixed. The 64-bit Direct3D 12 renderer never had this.
- To check: the «Graphics test» under Direct3D 11 - the processor on «High» should be far lighter than 66 ms.

# uaRO Graphics 3.0.5-beta.10 - what's new

- **Fixed: under Direct3D 12 the game lost the graphics card at the end of the «Graphics test» (R-GGKYDV).** With
  «Measure where the game spends its time» off, the test turns the measuring on for its run and off at its end. Turning
  it off, the driver freed at once a buffer the card was still writing the last frame into. Now the buffer goes once
  that frame is drawn. The same could happen turning the measuring off by hand in a game.
- The «Graphics test» also records what the game's processor time goes to, so the report shows what loads the processor
  on each set (63 ms a frame on «High» under Direct3D 11).
- «Grass and road seams without steps»: less memory work while the camera zooms out and in - the seams that left the
  picture are kept a little longer.
- To check: the «Graphics test» again under Direct3D 12 and Direct3D 11. No lost graphics card at the end. The last
  Direct3D 11 test had «World resolution» at 100 %, the one before at 75 %; for comparing, keep 100 %.

# uaRO Graphics 3.0.5-beta.9 - what's new

- **«Footprints in snow and sand» (dust under the feet on grass) tens of times lighter.** Until now every pixel of the
  screen went through all 64 last steps - 4 ms of a Radeon 610M's frame already on «Low». Now the screen is cut into
  squares of 64 pixels and a pixel goes through its square's steps alone; where there are none, nothing is worked out.
  The picture is the same. Direct3D 11 and Direct3D 12.
- **Grass under Direct3D 11 without needless processor work.** The trodden grass's grid rounded 49 thousand values a
  frame through a slow library function - a fifth of the game's processor work on a Radeon 610M laptop on «Medium». The
  rounding is our own now, with the same answers; likewise for «Hills without steps of light» and the ground's grid
  under the grass.
- **«Grass and road seams without steps» are not built anew every frame.** While the camera moves, each seam whose cells
  did not change is taken again - only its place and fog from the frame - the same as built anew. And no thousands of
  memory allocations a frame. On «High» under Direct3D 11 this was most of the processor's work.
- To check on the laptop:
  1. The «Graphics test» under Direct3D 11 and Direct3D 12 - to compare with the last ones (R-VMPQKX, R-XCHNKD).
  2. Footprints: walk over grass (dust) and, if you can, snow or sand - they should look as before.
  3. On «High» turn the camera over a field - the grass and road seams should hold still, no flicker.

# uaRO Graphics 3.0.5-beta.8 - what's new

- **«Graphics test»: the «Your settings» row now has the frame's dearest parts.** The test's last set lost its
  measurement (R-VMPQKX, R-XCHNKD); the table is whole now.
- **The frame profile under Direct3D 12 is more exact.** «Footprints in snow and sand» (dust under the feet on grass),
  the night, the skills' sparks, waterfalls and blindness's darkness have rows of their own now. Until now they went
  into the rows next to them: on «Low» the footprints were 3.9 ms of «the game's interface» on a Radeon 610M.
- **«Keep the frame rate» sees the footprints too.** When they are the dearest it may turn them off (4 ms of a frame on
  a Radeon 610M). What you turned on by hand it still leaves on.
- To check: the «Graphics test» once more on the laptop with Direct3D 12. The table should have the «Your settings» row
  with the frame's parts, and the footprints apart, as «footprints».

# uaRO Graphics 3.0.5-beta.7 - what's new

- **«Run the graphics test» on the «Play» page.** The settings app starts the game for the test itself, one window only.
  While you log in, the bottom of the game says «Graphics test: it begins 10 s after you enter a map». On the map a
  countdown follows, «Graphics test begins in N s: stand somewhere quiet and keep still. ~ cancels it», and then the
  test itself, as from the console. Under the button the last test is shown: when it was, the sets' table and whether it
  went to the developers (the report's code). The table refreshes by itself as soon as a test ends. While the game is
  running the button is greyed out: the test is then in the game's console (~ → «Graphics test»).
- «Help» now describes the graphics test.
- To check:
  1. Close the game, press «Run the graphics test» on «Play», enter a map and wait for the countdown and the test.
  2. After «Graphics test sent: R-…» the «Play» page should show the last test's table and «Sent to the developers:
     R-…».
  3. Start it from the button once more and press ~ during the countdown - «Graphics test cancelled» should show.

# uaRO Graphics 3.0.5-beta.6 - what's new

- **«Graphics test» in the game's console.** ~ → the second row «Graphics test: 75 s, the result goes to the developers»
  → Space or Enter. Stand somewhere quiet on a map and keep still: for 75 seconds the driver itself shows «No effects»,
  «Low», «Medium», «High» and your own settings in turn, 15 seconds each. Each time the camera turns once and zooms out
  and back in on the same path, so different computers' results can be compared. The frame profile runs all the while.
  Then your camera and your settings come back (the test never writes the settings file). The result is a short table:
  FPS, the slowest tenth of the frames, the card's work a frame and the frame's three dearest parts. The settings app
  sends it by itself as a «Graphics test» report, with no screenshots. The bottom of the game shows which set is on and
  how long is left; ~ stops the test, and then nothing is sent. The test does not switch the graphics API or «A shader
  for each state»: they need the game restarted, so they are only written into the report.
- To check:
  1. On a map (prt_fild08, say) run the test and wait for «Graphics test sent: R-…».
  2. The settings app's «Reports» should list it as «Graphics test» with the table on top.
  3. Run it again and press ~ halfway: «Graphics test stopped - nothing was sent» should show, and the camera comes
     back.

# uaRO Graphics 3.0.5-beta.5 — what is new

- **The shots in reports in full quality.** Until now the shots were made smaller, to 1600 points, and squeezed into
  JPEG (the site takes a request of 8 MB at most), and the sharpness could not be judged by them. Now after a report the
  settings app sends the originals too: the game's shots (Ctrl+Shift+F12, Ctrl+Shift+F10) as they are, PNG in full size;
  the ones pasted (Ctrl+V) or added as a file - into PNG with nothing lost. They go in parts of 1 MB right after the
  report, and the site shows them in the smaller ones' place. When an original does not go, the report keeps the smaller
  one, and the app says so.
- To check: send a report with a game's shot (Ctrl+Shift+F10) - after «Sending the shots in full quality…» the usual
  «Report … is sent».

# uaRO Graphics 3.0.5-beta.4 — what is new

- **Direct3D 11: the smaller world made larger plainly - 6-7 ms a frame less on a weak card.** With «World resolution»
  under 100 % the world is drawn smaller and made larger to the window's size. Making it larger with sharpening took 45
  texture reads for each point and cost 6-7 ms on a Radeon 610M - more than the smaller world saved (1.5-2 ms). Now one
  read, as in the super-light mode: the world a little softer, the interface and the text as sharp as they were. The
  player with the HD 6570 (the world at 75 %) should gain the most.
- **The colours in the same pass.** «Colour» with a smaller world is now laid on in the pass that makes it larger, not
  by a copy and a pass of its own - about 1 ms less again. The weather, the fireflies, the blindness and the glow, when
  they are drawn, lie over the colours as before.
- **The frame's profile shows the world made larger apart** («the world made larger») - it was put on the weather
  before.
- To check on the laptop: «Graphics API» → Direct3D 11, «World resolution» 75 %, the «Auto» set, measuring on - a minute
  on a field, a report. And whether the world became too soft.

# uaRO Graphics 3.0.5-beta.3 — what is new

- **Direct3D 12: a shader for each state of the drawing.** As in Direct3D 11 since 2.5.16: each state of the game's
  drawing (textures, transparency, fog, the interface) gets a small shader made for it in place of one big shader for
  everything. On a laptop with a Radeon 610M the login picture cost 4.6 ms with Direct3D 12 and 0.85 ms with Direct3D 11
  - now Direct3D 12 should draw the world and the interface as quickly. The shaders are made on a thread of their own
  while the old one draws; the first start makes them for a minute or two, then they come from the cache. To compare:
  «Debugging» → «A shader for each state» (at the next start of the game).
- **«Keep the frame rate»: the grass not last when there is no way without it.** When the other effects together cannot
  bring the frame rate back with the grass thinned (on a Radeon 610M the grass 20 ms, all the rest 13), the grass goes
  at once, not after two and a half minutes at 15-20 frames. Otherwise as before: the grass last. An effect you turned
  on yourself it still leaves alone.
- **The frame measured more exactly.** The draw the interface begins with (the login picture) is no longer put on the
  effect before it («lights and torches»).
- To check on the laptop (measuring on, Direct3D 12):
  1. Start the game, stay a minute at the login and in Prontera or on a field - «pipelines with the draw shaders of the
     game's own states» in the log - send a report.
  2. «Debugging» → untick «A shader for each state», start again, the same places and a report once more - to compare
     before and after.

# uaRO Graphics 3.0.5-beta.2 — what is new

- **The frame measured, to find what slows it.** «Debugging» → «Measure where the game spends its time» now also cuts
  each frame into its parts: the game's world, its interface, each effect, the world made larger, the picture into the
  window, the clears, the game's textures written. For each part - the card's and the processor's time. Every 10 seconds
  «profile:» lines come into the log: how many frames, the average frame and the slowest tenth of the frames, the
  frame's parts from the dearest, the waits apart (the frame limit, Present, the card). Works with Direct3D 12 and
  Direct3D 11.
- To check on the laptop with a Radeon 610M:
  1. Turn «Measure where the game spends its time» on and play 1–2 minutes in Prontera and on a field, then send a
     report (Ctrl+Shift+F10).
  2. «Graphics» → «What draws the game» → «Graphics API» → «Direct3D 11 (older graphics cards, test)», start the game
     again, the same places and a report once more: this is how the game runs for the player with an HD 6570.
  3. Put «Graphics API» back as it was and turn the measuring off: it costs a little speed.

# uaRO Graphics 3.0.5-beta.1 — what is new

- **From a beta back to the release.** The channel «Release» chosen and a beta installed - the settings app now offers
  to go back to the release, even when it is older than the beta. On «Play» and «Service» - the button «Back to the
  release 3.0.4» (on «Play» also the big «Back to the release and play»), in the status line «back to the release 3.0.4
  possible». The game asks before the login the same way: «The channel is «Release»: back to the release 3.0.4?» - «Go
  back» or «Stay on the beta». The settings stay, and the beta is kept as the previous version: «Back to the previous
  version» brings it back.
- Before, «Release» offered only a newer version, and the beta stayed until the next release. Nothing changed on the
  channel «Beta (testing)».
- To check: «Play» → the channel «Beta (testing)», update to 3.0.5-beta.1; then the channel «Release» - «Back to the
  release 3.0.4» shows. Close the settings app and start the game - the question before the login. Go back - 3.0.4 is
  installed, and «Service» → «Back to the previous version (3.0.5-beta.1)» brings the beta back.

# uaRO Graphics 3.0.4 — what is new

The heart of this release is the own ground: uaRO Graphics draws the map's ground itself, and far off the houses,
trees and water too. Also the time left on the buff icons, notifications while the game is behind, a free camera in
buildings, a choice of the sound output device and grass on weak graphics cards.

- **Own ground.** uaRO Graphics draws the map's ground from the map's file in place of the game: «Effects» → «Ground
  and walls» → «Own ground». It is a part of «Remaster»: on under «Effects quality» «High» and under «Auto» with a
  strong card; under «Custom» - as you set it. Direct3D 12 only.
  - Smooth hills: the cells split into finer ones, the heights on a smooth curve; ledges and walls as in the game.
  - The ground's textures in full colour (24 bits in place of 16), no shimmer far off. With «Grass and road seams
    without steps» the edges of grass, road and sand wind instead of following the cells' squares.
  - Past the edge where the game cuts the world (1500 units from the camera): the ground, «Houses and trees far off»,
    the water and the lava with the game's waves; with «Modern water» the water far off is as near.
  - «Hills without steps of light» lights the slopes all over the own ground, near and far; «Map light without steps»
    works on it too.
  - The colour and the fog come from the game's own picture, so the light is the game's. While the camera stands
    still, the ready picture of the ground is drawn again.
  - In the 32-bit game, with too little memory left, the driver lets go of the houses far off first, then of the
    smooth ground.
  - Known: in Alberta with the camera out all the way, a band across the ship may still show.
- **The camera** («Game and chat» → «Camera»).
  - «Free camera in buildings» (on): in inns, shops, guilds and castles the camera turns, tilts and zooms out as
    outdoors.
  - «Camera no further out than the character shows» (on): the camera stops at 1400, since past 1500 the game draws
    no character, monsters or effects. Unticked, the game's own limit is back.
  - Photo mode - Ctrl+Alt+F12: the game's interface, the cursor, the windows' panel and the frame counter go, the
    world stays for a shot (Ctrl+Shift+F12). The same keys bring everything back.
- **Time left on the buff icons.** Each icon at the screen's right edge shows how long the buff lasts: «45», «4:32»,
  «29m» - white with a dark rim at the icon's foot. The durations come from the server's own data, the level from the
  cast itself, another player's too; a recast starts the count again. «Game and chat» → «Buffs» → «Time left on the
  buff icons».
- **Notifications while the game is behind.** The game behind other windows or minimised, and someone whispers,
  invites you to a party or a guild, offers a trade, asks to be your friend, or the character dies: the game's
  taskbar button flashes and Windows shows who and what. A click on the notification brings the game back. Players
  you ignore send none. «Game and chat» → «Notifications»: a box for each kind.
- **Several game windows.** Another window's card on the windows' panel turns red when its character has died and
  orange while it is hit; a blue dot - it got a whisper. «Game windows started by «Play»» (1–8): «Play» starts the
  game that many times, one every second and a half; no logins or passwords are kept.
- **The sound output device.** «Sound» → «Sound through uaRO Graphics» → «Sound output device» (in the Simple mode
  «Main» → «Other»): «As in Windows» or any connected one - headphones, speakers, the monitor. It acts at once, in a
  running game too. The chosen device turned off - the sound goes as in Windows, and back to it when it is there again.
- **Sounds of the world** (off by default). Almost 26 thousand sounds the game's makers placed on 563 maps: birds,
  brooks, waves, the wind, drops in caves. The old client loaded them and never played them. Now they sound by their
  places, louder nearer. «Sound» → «Sound through uaRO Graphics» → «Sounds of the world».
- **Grass on weak graphics cards.**
  - A card weak by its memory (under 4 GB of its own, or Direct3D 11 only) is now judged by its speed. When for a
    minute on a map it draws the weak set in half a frame at most, «Auto» gives it the medium set with the grass on
    trial - a line on the screen tells. Three minutes without it holding the game back - the set stays; else the weak
    set is back, and the trial is not made again.
  - On such cards the grass is lighter: half the work, a little sparser.
  - «Keep the frame rate» thins the grass first, then turns the other effects off by their cost, the grass last, and
    says on the screen what it turned off. An effect you turned on yourself during the game it leaves alone.
- **Trees no longer change as the camera turns.** A tree's shade from «Trees of different shades» and the autumn
  patches from «Autumn and winter crowns» stay on the tree itself, from any side.
- **Paving without a straight line of colour.** «Materials: stone, brick, bark» lays its patches without broken edges
  (Alberta's pier had a straight edge of yellowish against bluish), and white and grey paving gets no bluish or
  yellowish patches.
- **The game's crash on the way to moscovia fixed.** The game's client drew the character's shadow during a map's
  load before it had a game mode and crashed; now such a frame draws no shadow.
- **The «Graphics card» row always shows** («Graphics» → «What draws the game»), grey with a single card.
- **Linux and the Steam Deck (to be tried).** The driver tells Wine, Proton and the Steam Deck. Under Wine the
  64-bit renderer (HD textures, DLSS) does not start, the game draws with its own renderer; to try it:
  `[renderer] bridge_wine = 1`. The guide - docs/guides/steam-deck.uk.md.

# uaRO Graphics 3.0.4-beta.36 — what is new

- **A buff's timer starts anew when it is cast again (R-C8NYXS).** Renewing a buff, the server sends its icon first
  and the cast after it (uAthena: `clif_skill_nodamage(..., sc_start(...))`). The driver waited for the cast before
  the icon and took the icon sent again for a map change: the time went on. Now a cast just after its skill's icon
  gives the icon its time anew from the icon's moment - and by the skill's level (the longest was taken before).
- **The times on the icons themselves.** The game puts an icon with its middle at the layout's place; the times were
  put as if it were its top left: 16 pixels down and right - on the next icon and under the screen's edge. Now the time
  is on the foot of its icon, white in a dark rim, no black box.
- To check: Blessing and Increase AGI - a time on each icon; cast one again a minute on - its time full again.

# uaRO Graphics 3.0.4-beta.35 — what is new

- **Grass on weak video cards.** Two things turned it off there: «Auto» took every card with under 4 GB of memory of
  its own as weak (integrated graphics - a Radeon 780M shows 417 MB), and «Keep the frame rate» turned off the grass
  first, the costliest (R-EQDYNE: 4.94 ms on a 780M). Now:
  - **The card is judged by its speed.** A card weak by its memory that draws the weak set on a map for a minute with
    its work at most half of a frame gets the medium set (the grass with it) on trial - a line on the screen says
    so. Three minutes without the card holding the game back: the set stays for it from then on; else the weak set
    comes back, not tried again. «Auto» only; the verdict in uaro-graphics\card-level.txt, the Effects page shows the
    same level.
  - **Lighter grass.** On cards weak by their memory the blades' roots are looked for on every second row: half the
    work, the grass a little sparser.
  - **«Keep the frame rate»** thins the grass first (when it costs a millisecond or more), then turns the other
    effects off by their cost, the grass itself last.
- To check on a strong card: Debugging -> «Take the card as weak», the «Auto» set, restart the game and be on a map
  for a minute - the line «The card keeps up...»; three minutes later «the medium set kept» in the log. Then untick it.

# uaRO Graphics 3.0.4-beta.34 — what is new

- **beta.33's turn of the models now really goes (R-37HNEB, prontera).** In beta.33 the game's model triangles were
  to be looked at in turn, but the turn went back to the first every picture: a picture starts both at the present
  and at the game's clearing of the screen right after it - and the second start, with no model between the two,
  reset the turn. Hence «noted 360600» in the log again - the same 600 every picture - and 0 % matched. Now a picture
  with none of the game's models leaves the turn as it was.
- The log («own models», a map's 600th picture) now shows the turn going: how often all of the game's models were
  looked at and how many a picture holds.
- To check: Prontera - stand a minute and send a report.

# uaRO Graphics 3.0.4-beta.33 — what is new

- **The far houses' and trees' colours from all of the game's models (R-5ZC9JX, prontera).** The own ground learns its
  models' colours from the game: up to 600 triangles a picture. But they were the same first 600 every time: while
  the camera stood, it learnt from those alone. A minute in Prontera, none of them matched ours - the far models'
  colours learnt not at all (left approximate). Now the triangles go in turn: the next picture the next 600, and in a
  few pictures all of the models round about are seen.
- The picture drawn again (beta.32) in the same report: a minute still in Prontera - 100 % of the pictures drawn
  again (was 0 %), the processor 2.8 ms a picture instead of 3.6. Now that the colours are learnt again, the picture
  will be made anew once a second - about 1 ms once in 60 pictures.
- To check: Prontera - stand a minute and send a report (the log: the share drawn again and how many of the game's
  model triangles lie «where ours lie»).

# uaRO Graphics 3.0.4-beta.32 — what is new

- **The own ground draws its picture again in towns too (R-Q3W727, prontera).** While the camera stands, the own
  ground draws its last picture again instead of making it anew. But a minute still in Prontera made 3571 pictures of
  3600 anew: the far houses' and trees' colours are learnt from the game, and the game lights its moving models anew
  every picture - the cherry trees sway, the clock's hands and the signs turn. Now such colours reach the far models
  at most once a second (they stand still - no difference to see), the picture drawn again meanwhile. Prontera,
  standing: about 1 ms of the processor a picture less expected (of ~3.6 ms). The same picture.
- To check: Prontera by the cherry trees - stand a minute still and send a report (the log tells the share of
  pictures drawn again).

# uaRO Graphics 3.0.4-beta.31 — what is new

- **A straight line across the paving and an odd tint (R-KZBBTN, alberta).** «Materials: stone, brick, bark» lays
  large patches of lighter and darker, warmer and colder over the ground so that its tiles' repeats do not show. Their
  noise was worked out through the sine of large numbers: a point of the noise's lattice reached from the cell on its
  left and from the cell on its right got two values once the video card's compiler had it - and a patch broke along
  a straight line (on Alberta's quay: yellowish on one side, bluish on the other). Now the noise is worked out in
  whole numbers and breaks nowhere; the same for the patches on the trees' crowns. The patches' pattern is new.
- **White and grey paving without bluish and yellowish patches.** The patches make warmer and colder only ground with
  a colour of its own - grass, earth, sand, caves (as before); white and grey stone only a little lighter or darker.
- To check: alberta by the ship, the camera close - the paving even, no straight edge of colour; prt_fild08 - the
  grass in patches as before; prontera - the paving without noticeable blue or yellow.

# uaRO Graphics 3.0.4-beta.30 — what is new

- **The hills' light on the own ground costs less.** The slope and the light were worked out for each point of the
  ground (73 ns a point: a map's first picture ~3 ms dearer). Now the light is worked out once a corner of the cells
  when it moves, and a point takes it between its cell's corners: 41 ns a point with all the rest, the first picture
  only ~1 ms dearer. The same picture.
- To check: as for beta.26 - the hills have volume near and far; maps start without a noticeable delay.

# uaRO Graphics 3.0.4-beta.29 — what is new

- **A smooth change from the smooth ground to the plain one.** Near, the own ground is smooth (a fine mesh); further
  off it is plain, cell by cell; pieces of ground change between them somewhere past 1000 units from the camera. The
  smooth surface and the plain one part by up to a unit on slopes, so as you walked the slopes further off jumped a
  little when a piece changed. Now over the last quarter of the way to the change the smooth ground settles onto the
  plain one, and a piece changes onto it already: no jump, no crack at the seam. Offline on prt_fild08: the seam
  level, no cracks.
- To check: prt_fild08, walk up and down the slopes looking at the hills mid-screen - no jumps.

# uaRO Graphics 3.0.4-beta.28 — what is new

- **No hitch when the sun moves.** The hills' light on the own ground (beta.26) is baked into its colours and made
  again when the sun of the day's clock moves. All of the ground in view was made again in one picture (~10 ms) - a
  hitch could show every minute or two. Now eight pieces of ground a picture: the light hardly changes meanwhile, no
  seams.
- To check: with "The sun moves by the clock" and "Hills without steps of light" on, stand on prt_fild08 for a few
  minutes - no
  hitch every minute.

# uaRO Graphics 3.0.4-beta.27 — what is new

- **The own ground shows right after a map's start.** It waited for all its pictures to reach the graphics card,
  the textures of the houses and water far off among them: on big maps the game's ground showed for 0.6-0.8 s, then
  gave way to the own ground at once. Now the own ground is drawn as soon as its own textures and light are up; the
  houses and the water far off join it when their pictures are up.
- **The log: why the own ground's picture is made anew.** In the reports the picture was drawn again with the camera
  still 0 % of the time. Now once a minute the log has a line: how many pictures were taken as made, and what changed
  in the others (the camera, the ground's colours, the fog, the hills' light, the models). Stand still for a minute and
  send a report - the cause will show.
- All 921 maps of the client checked offline with the new parts (the slopes for the hills' light, the water): all
  read.
- To check: go into several maps (prontera, prt_fild08, alberta) - no visible change of the ground a second after
  the start; on one, stand still for a minute and send a report.

# uaRO Graphics 3.0.4-beta.26 — what is new

- **"Own ground" is now a part of the "Remaster" mode** (you chose "Own ground into Remaster"). The name lost
  "(prototype)", a new hint. It is on under "Effects quality" "High", and under "Auto" with a strong card (it was there
  since beta.10 already - now on purpose). With it the houses, trees and water far off. Under "Custom" - as you set it.
- **"Map light without steps" on the own ground too.** The smoothing of the baked light's blotches in a cell knew only
  the game's 256×256 atlas; the own ground's atlas is wider, and the own ground went without it. Now the size comes
  from the texture itself.
- **"Hills without steps of light" all over the own ground (the own world's light).** It lights the slopes by the
  map's sun, but took the slope from the cells the game drew: past its far edge the own ground was flat. Now the own
  ground works out the same light by its own smooth slopes - near and far; the same sun, strength and night. The
  light is baked into the vertices' colours, so a picture costs no more. Offline on prt_fild08: the slopes have
  volume, no steps.
- To check: prt_fild08 with the camera pulled out all the way - the hills far off have their sunny and shaded sides as
  the near ones do; with "Hills without steps of light" and without it.

# uaRO Graphics 3.0.4-beta.25 — what is new

- **A check of the cut ship** (report R-GXHFX8, "the ship still comes out cut"). The beta.24 fix took away only part of
  the cause, and from my own camera angles the cut does not come out offline. Rather than ship another beta on a
  guess, the driver now writes the game's camera (its matrix and the picture's size) into the log with every shot and
  report (Ctrl+Shift+F12, Ctrl+Shift+F10). With it I can make your picture again offline exactly. Nothing is drawn
  differently.
- What to do: at the same place where the ship is cut, send a report (Ctrl+Shift+F10).

# uaRO Graphics 3.0.4-beta.24 — what is new

- **No band across the ship with the camera pulled out all the way** (report R-PTAMG4, "a band is seen on the ship").
  The game draws its water and most of the ship's faces (all whose textures have see-through places: sails, deck) in
  one list, far to near, without writing depth. The water far off was drawn in the middle of that list - at the
  game's first water cell. The deck's faces that came earlier in the list lay under it, and a band of water ran across
  the deck. Now the water far off is drawn at the very start of that list, and the whole ship lies over it. The game's
  water picture and waves the driver takes by looking ahead in the picture's record. Offline the band of water on the
  deck came out again, and with the fix it is gone.
- To check (with "Own ground (prototype)"): Alberta, the camera pulled out all the way, the ship near the top edge -
  the deck whole; and the water as before (no strip, no line).

# uaRO Graphics 3.0.4-beta.23 — what is new

- **No line on the water, no cut roofs near the far edge** (report R-W7A5XC, "the strip got smaller; also noticed the
  roofs of the buildings disappear along the same line"). Both were first made again offline on Alberta with the
  driver's own arithmetic, and both are gone with the fix.
  - **The line on the water was the game's waves.** The game lifts and lowers its water at the cells' corners by up
    to 0.6 units (its waves), while the water far off was flat. Where they met came short black gaps and overlaps,
    cell by cell. Now the water far off has the same waves: the driver reads their phase every picture from the
    game's first water cell, and the sine from the game's own table.
  - **The cut roofs were the own ground's depth.** A triangle of the own ground across 1490 had the game's depth at
    some corners and the squeezed one at others, and the graphics card carried it straight between them - nearer than
    it really was. A quay's tall wall behind a house came out over its roof and over the water at its foot. Now such
    triangles are cut at 1490, and each piece has one depth.
- To check (with "Own ground (prototype)"): Alberta with the camera pulled out all the way, turned every way - no line on
  the water, no holes in the roofs near the top edge.

# uaRO Graphics 3.0.4-beta.22 — what is new

- **No black strip on the water any more** (report R-ZZN7GY, "the black strip on the water is still there"). The
  beta.21 fix worked (the log has "214 of the game's 1365 parts ... raised"), but the strip stayed as thick as before -
  so the cause was elsewhere, and it was mine. Past 1490 units the own ground writes its depth squeezed, since past the
  game's edge (1500) the depth buffer has no room. Because of that, all the game draws between 1490 and 1500 came out
  "behind" the own ground beyond it in the depth buffer, though it was really nearer. The game's water there hid
  behind the black bed, and the water far off began only at 1500 - hence the strip. Now the water and the houses far
  off are drawn from 1490, at the same depth as the own ground. Offline, on Alberta with the same arithmetic, the strip
  came out exactly as in your picture, and with this fix it is gone. It also fixes what the picture does not show:
  houses and ships that cross that distance lost a band of their hull.
- To check (with "Own ground (prototype)"): `@warp alberta 193 146`, pull the camera out all the way and turn it to
  the sea - no strip, the ship whole; the same on izlude.

# uaRO Graphics 3.0.4-beta.21 — what is new

- **No black strip between the game's water and the water far off** (report R-UBT67Q, "report with the water at the
  horizon"). In the picture from Alberta the sea to the horizon was water already, but a black strip ran across it
  where the game's water meets the water far off. The cause is in the game itself. It splits the map into parts and
  drops those whose box lies past its far edge, 1500. It takes the parts' boxes from the map's file (.rsw), and there
  they are made by the bed, without the water over it. In Alberta 193 of the 1024 parts are the sea's bed, 7 units
  under the water. When a part's bed was past the edge, the game dropped it with the water over it, though the water
  itself was still nearer. Nobody drew that water, and the black bed showed. Now, while "Own ground (prototype)" is
  on, the driver raises the top of such boxes to the water's surface. The game draws its water right up to its edge,
  and the water far off goes on from there. With the own ground off the game's boxes are put back as they were.
- To check (with "Own ground (prototype)"): `@warp alberta 193 146`, pull the camera out all the way and turn it to
  the sea - there should be no strip; the same on izlude. A report after a minute there: the log has the line
  "water: ... raised to its water's top".

# uaRO Graphics 3.0.4-beta.20 — what is new

- **Water and lava far off on the own ground.** The game lays its water a cell at a time with its ground, but only up
  to its far edge. Past it the own ground showed the bed bare: in Alberta, with the camera pulled out all the way, the
  sea near the horizon was black. Now the water goes on past the edge - the parts of the cells beyond it, at the same
  level. It uses the same picture, state and colour as the game's own water near; with "Modern water" it is drawn as
  the near water is. When none of the game's water is near, the driver uses its own copies of those pictures
  (water*.jpg), changing at the game's pace. Lava likewise (mag_dun01, mag_dun02).
- **The own ground on any map.** All 921 maps of the client checked offline: every ground and model file reads. Nine
  maps have no ground at all (ma_zif01-09) and stay as the game has them. In the 32-bit game (without DLSS and AI
  textures the drawing is in uaRO.exe itself, which has 2 GB of memory) on the heaviest maps (lighthalzen, ecl_fild01)
  the driver first lets the houses far off go, then the smooth ground, when too little memory is left. The log has a
  line with how much was left.
- **The camera still - the ground not made again.** In report R-35N5QN the picture drawn again never once happened:
  the count of the models' triangles grew every picture. Now only new colours count, and while the camera stands the
  own ground's picture is taken as made.
- To check (with "Own ground (prototype)"):
  - `@warp alberta 193 146`: pull the camera out all the way and turn it to the sea. The sea to the horizon should be
    water, with nothing black and no seam where the game's water ends. Look with "Modern water" on and off.
  - The same on izlude.
  - Prontera and a dungeon (pay_dun01, iz_dun00): anything strange.
  - After a minute on a map with water, send a report - the log has the lines "own ground: ...'s water" and
    "far water".

# uaRO Graphics 3.0.4-beta.19 — what is new

- **The autumn patches and the leaves' shades stay on the leaves** (report R-E3A6C2, "the trees' colour changes as I
  turn the map"). In the report's pictures other trees turned yellow at each turn of the camera. Which tree turns was
  set by the crown's place - fixed in beta.18. The autumn patches themselves, and the crown's lighter and darker
  clumps, were laid over the tree's box on the screen: as the camera turned, the leaves moved under them and the
  patches stood. Now they are on the leaves themselves (a place on their texture) and turn with the tree. The crown's
  volume and the rime on top stay by the camera - that is light.
- To check: on prt_fild08 with "Autumn and winter crowns" on, turn the camera round the trees - which trees are yellow,
  and
  where their patches are, should stay put.

# uaRO Graphics 3.0.4-beta.18 — what is new

- **The trees no longer change as the camera turns.** With "Trees of different shades" each tree has a shade of its
  own by its place in the world. That place was taken from the middle of the leaves' box on the screen at their mean
  depth, and as the camera turned it went round the crown and moved by up to 6 units. So the tree took the next shade,
  its brightness jumped by up to 15 %, and it looked as if the texture changed. Now the place is the middle of the
  leaves themselves in the world, the same from every side. Checked offline on prt_fild08's trees for eight turns of
  the camera.
- The far trees the driver draws (past 1500) each get their own shade by their place, as the near ones do. Before, one
  draw held many trees, and they had one shade between them and one "crown" for all.
- To check: turn the camera round the trees on prt_fild08 - the trees should not change their colour.

# uaRO Graphics 3.0.4-beta.17 — what is new

- **The camera no further out than the character shows** (report R-35N5QN). The game lets the camera go out to 2040
  but draws its world only to 1500 - further out the character, the monsters and the effects vanished. A new box,
  "Camera no further out than the character shows" (the "Game and chat" page, the "Camera" section, on by default),
  stops the camera at 1400. A camera already further out comes in at once. With the box off the game has its own
  limit back.
- The driver moves the limit in the game's memory itself: every place where the game limits the zoom (checked in
  uaRO.exe's code) takes it from one variable.
- To check: zoom out as far as it goes - the character should stay in sight; turn the box off - the camera goes out to
  2040 again.

# uaRO Graphics 3.0.4-beta.16 — what is new

- **The own ground does not vanish with the camera far out** (report R-35N5QN, "it goes dark when I zoom out to this
  place"). The game lets the camera go out to 2040 but draws its world only to 1500 units, so at some distance all of
  the ground in view lies past its edge and the game draws no cell at all. The own ground started at the game's first
  cell, so it did not start either and the screen went black. Now the own ground, the houses and the trees are drawn
  even when the game draws nothing.
- The game does not draw the characters, the monsters and the effects past 1500 itself, and the own ground can do
  nothing about it. How to fix that we discuss apart.
- Confirmed by report R-35N5QN: the driver's work a frame went down from 10.6-11.2 ms (beta.14) to 3.0-3.5 ms, and the
  game is held by the 60 frames a second limit again.

# uaRO Graphics 3.0.4-beta.15 — what is new

- **The own ground is much faster** (report R-N6FC5V: 7.7 ms of the processor a frame, up to 21 ms zoomed out). The
  main cause: the game gives a corner shared by four cells different colours, so the corner "changed" every frame and
  the colours of all the visible ground were made anew each frame. Now a corner takes its colour from one cell alone,
  and the colours are kept.
- Also faster:
  - the vertices' projection - one division a vertex instead of four;
  - while the camera stands still, the ready picture of the ground is drawn again;
  - the smooth ground and the transitions between textures only where a cell is 40 pixels or more on the screen
    (about 1000 units from the camera at 1080p). Further off the ground is the game's own - its cells are small there.
- Confirmed by report R-N6FC5V: the houses and trees far off stand exactly where the game's do - all 228,558 triangles
  matched (100 %).
- To check on prt_fild08 with the own ground, as last time (zoom out, walk about), and send a report. The 600th frame's
  `own ground: ...` line now shows how many corners change a frame (it should be about 0), the share of the frames
  drawn again, and the processor's time.

# uaRO Graphics 3.0.4-beta.14 — what is new

- **Houses and trees far off (prototype).** Under "Own ground (prototype)" there is a new switch, "Houses and trees far
  off". It is on by default and works only together with the own ground. uaRO Graphics reads the map's houses, trees
  and rocks from its files and draws them past the edge where the game cuts them off (1500 units from the camera). Now,
  with the camera far out, the own ground there is no longer bare. Nearer, the game draws its models itself, further
  off the driver does, and at the edge a model is cut exactly there.
- The models' colours are taken from how the game draws them near, as the own ground's colour is. Models the game has
  not drawn yet are lit the way the game lights those already seen.
- Checked offline on prt_fild08 and in Prontera from the client's files. All the models were read (23 and 65 files),
  and the trees stand next to their shadows baked into the map. On the processor, with the camera zoomed out the most,
  it takes 0.3-1.5 ms a frame.
- To check on prt_fild08 with the own ground: zoom the camera out the most and tilt it. The trees and Prontera's wall
  should be seen to the horizon, with no break at the edge. In about 10 seconds the log gets an
  `own models: ... (N %) where ours lie` line - how many of the game's model triangles matched ours. It should be about
  100 %, but for the models that move. Send a report with that line.

# uaRO Graphics 3.0.4-beta.13 — what is new

- **Own ground: the seams are no longer smeared.** Along each seam a strip covers the texture's dark rim, and in
  beta.12 it took one place of the texture over all its width, so smears of one column of texels ran along the seams.
  Now, as on the game's ground seams, the texture is only mirrored at the very edge there. Offline, on prt_fild08's
  textures, the smears are gone.
- **The own ground on newer maps too.** Grounds of versions 1.8 and 1.9 (88 of the client's maps, sva_god1 among
  them) are 1.7 with the map's water at the end of the file. The own ground now works on them.
- To check: the same place on prt_fild08 as in beta.12. If the seams between tiles of one texture (sand with sand)
  still look smeared, the cause is another one: there the next tile's texture is mirrored across the seam, as "Grass
  and road seams without steps" does on the game's ground. Then tell me - the blending between tiles of one texture
  goes, and stays only between different textures.

# uaRO Graphics 3.0.4-beta.12 — what is new

- **Own ground: soft texture transitions.** With "Own ground (prototype)" and "Grass and road seams without steps" on,
  the line between grass, road and sand on the own ground no longer runs along the cells' squares: it wanders, one
  texture reaching into the other, as on the game's ground with the seams. The strips are made of the ground's own
  points and lie exactly on it, so they do not blink. Seams between tiles of one texture are smoothed too. The tiled
  floors of dungeons and patches laid by hand (Prontera's paving) keep their straight edges. Far off, where the game no
  longer draws its ground, there are no transitions.
- **The own ground is faster.** Chunks under the camera that cannot be seen are no longer worked out, and the vertex
  colours are made again only when the game gives a new one. Measured on prt_fild08: 0.3 ms of the processor a frame
  with the usual camera, up to 1.6 ms at a middle distance with the transitions.
- To check on prt_fild08: turn both switches on and look at the edges of grass and road, on hills and by the steps;
  turn "Grass and road seams without steps" off - the edges are square again. In the log the `own ground: ...` line
  ends with "strips between the textures on".

# uaRO Graphics 3.0.4-beta.11 — what is new

- **"Keep the frame rate" says what it turned off** (report R-EQDYNE, "no grass"). When the game runs below the limit
  for a while because of the graphics card, it turns the costliest effect off for the rest of the game. Until now only
  the log said so, and the grass simply vanished. Now a line shows over the game for 10 seconds, for example "Grass is
  off for the rest of the game: 21 frames a second, under 45 (Keep the frame rate)".
- An effect the player turned on during the game (the console ~ or the settings app) is left alone for the rest of the
  game. One it had already turned off comes back when the player turns it on again.
- To check: on the "Frames" page set "Keep the frame rate" above what your card gives (200, say) and wait a minute -
  the line about the effect turned off shows. Then turn that effect off and on in the console - it comes back and is
  not turned off again.

# uaRO Graphics 3.0.4-beta.10 — what is new

- **Own ground (prototype).** The first step to drawing the world ourselves: uaRO Graphics draws the map's ground
  itself from the map's file instead of the game. The switch "Own ground (prototype)" is on the "Effects" page, group
  "Ground and walls"; off by default.
  - Smooth hills: each cell is cut into smaller ones (3×3, 2×2 in the 32-bit renderer) and the heights follow a smooth
    curve through the game's cell corners; steps and walls stay as the game has them.
  - The ground's textures in full colour (24 bits instead of the game's 16) with their own mipmaps, so they do not
    shimmer far off.
  - With the camera far out the ground goes on past the edge where the game cuts it off.
  - The vertex colours and the fog are taken from the game's own frame, so the light is the game's.
  - Direct3D 12 only; on Direct3D 11 the switch changes nothing.
  - To check on prt_fild08: turn it on and compare with Pause (before/after), zoom the camera out to the edge. Look for
    holes and seams, colours that differ from the game's, characters and houses standing in the ground rather than on
    it, and the FPS. The log has `own ground: ...` lines with the time a frame — send a report if something is off.

# uaRO Graphics 3.0.4-beta.9 — what is new

- **The game's crash on the way into moscovia fixed** (report R-VBDSCP). The game's own client drew a character's
  shadow while a map loaded, before it had a game mode, and fell. Now that frame simply has no shadow.

# uaRO Graphics 3.0.4-beta.8 — what is new

- **Linux and the Steam Deck (to be checked).** The driver tells Wine/Proton and the Steam Deck and writes it in the
  log. Under Wine the 64-bit renderer (HD textures, DLSS) does not start: its picture would have to cross processes
  and Wine has no DirectComposition, so instead of a black screen the game draws with its own renderer. To try the
  64-bit one: `[renderer] bridge_wine = 1`. The Steam Deck's gamepad works through uaRO Pad. A guide is in the
  repository, docs/guides/steam-deck.uk.md. A tester with Linux or a Steam Deck is needed.

# uaRO Graphics 3.0.4-beta.7 — what is new

- **Sounds of the world.** The game's makers placed almost 26 thousand sounds on 563 maps: birds in the fields,
  brooks, the sea's waves, the wind in the desert, insects, frogs, waterfalls, drops in the caves. Our old client
  loaded them and never played them. Now the driver plays them: each is heard near its place, louder as you come
  closer, from its side, with the place's echo. «Sound» → «Sound through uaRO Graphics» → «Sounds of the world» (off:
  turn it on to listen) and their volume.
- The sounds are the game's own files from data.grf; nothing new is downloaded.

# uaRO Graphics 3.0.4-beta.6 — what is new

- **Time left on the buff icons.** Each icon at the screen's right edge (Blessing, Increase AGI, Kyrie, a Sage's
  endows, potions...) shows how long its buff still lasts: «45», «4:32», «29m». The durations come from the server's
  own data and the level from the cast itself, another player's too. With the level unknown, the longest time counts.
  «Game and chat» → «Buffs» → «Time left on the buff icons».
- Where a time is off, tell me which buff and I will fix the table.

# uaRO Graphics 3.0.4-beta.5 — what is new

- **A free camera in buildings.** In inns, shops, guild halls and castles (101 maps) the game held the camera: it
  barely turned and did not zoom out. Now it turns, tilts and zooms out there as outdoors. «Game and chat» →
  «Camera» → «Free camera in buildings» (on).
- **The photo mode: Ctrl+Alt+F12.** The game's interface, the cursor, the windows' panel and the frame counter go -
  the world alone for a fine shot (Ctrl+Shift+F12). The same keys again bring everything back.
- Outdoors the game already lets the camera go very far (7 times the usual) and eases its zoom by itself - I checked
  that in the game's code, so nothing changed there.

# uaRO Graphics 3.0.4-beta.4 — what is new

- **The windows' panel shows how the helpers are.** On another game window's card: red - its character died (until
  it lives again or you go to that window), orange - it is being hit, a blue dot in the corner - someone whispered to
  it (until you go to that window). The HP bar turns red when health is low, as before.
- **A set of windows at one press.** «Game and chat» → «Game windows started by «Play»» (1-8): the settings app's
  «Play» starts the game that many times, one every second and a half. You log in to each yourself - no login or
  password is kept.

# uaRO Graphics 3.0.4-beta.3 — what is new

- **Notifications while the game is behind.** The game window behind others or minimised, and someone whispers,
  invites you to a party or a guild, offers a trade, asks to be your friend, or the character dies: the game's
  taskbar button flashes and Windows shows a notification - who and what. A click on it brings the game back. With
  several game windows each tells of its own. «Game and chat» → «Notifications»: a box for each kind.
- Players you ignore in the game send no notifications.

# uaRO Graphics 3.0.4-beta.2 — what is new

- **The «Graphics card» row is always seen.** «Graphics» → «What draws the game» → «Graphics card» shows which card
  draws the game when the computer has just one too - the row is grey then, there is nothing to choose. Before, it
  came only with several cards.

# uaRO Graphics 3.0.4-beta.1 — what is new

- **A choice of the sound output device.** «Sound» → «Sound through uaRO Graphics» → «Sound output device» (in the
  Simple mode on the «Main» page, «Other»): «As in Windows» or any device connected - headphones, speakers, the
  monitor. Applies at once, in a running game too: the music and the sounds move to the chosen device.
- **The chosen device turned off - the sound goes on.** While the headphones are not connected the sound plays as in
  Windows, and when they are back the game moves it to them by itself.
- **A device gone during the game** - the game opens its sound again on the device there is.
- **The log tells where the sound plays:** the device's name and the devices Windows sees.

# uaRO Graphics 3.0.3 — what is new

This release is for playing in several windows at once: the main character and the helpers.

- **The windows' panel in the game.** With two game windows or more open, a panel shows in the game's picture itself:
  a card for each window with its number, the character's name, levels and HP and SP bars (red under 25 %, as in the
  game). A click on a card brings that window to the front; the current one is blue. The game does not get that click,
  and the game's cursor stays over the panel.
  - Drag the panel by its top row. The button by the cross lays the cards in a row or a column. The cross or
    Ctrl+Alt+0 hides the panel, Ctrl+Alt+0 shows it again.
  - The «Icons» layout: small tiles with the numbers and HP/SP bars, the name in a tip under the mouse.
  - A right click on the panel opens its menu: layout, size, opacity, the other windows' music and sounds, the other
    windows light, «Hide the panel».
  - The settings are in «Game and chat» → «The windows' panel» and in the game's console (~).
- **Keys that switch the windows.** Ctrl+Tab goes to the window you played in before. Ctrl+Alt+1…8 goes to the window
  of that number (the order the windows were started in). The game does not get these keys. Set in «Game and chat» →
  «Several game windows» → «Window keys».
- **Switching without a blink.** A full-screen game no longer minimises when you go to another game window. To the
  desktop or another program it still minimises, as before. A game in a window gets a sharp picture ready first (0.4 s
  at most) and comes to the front with DLSS and the effects.
- **The other game windows light and quiet.** The windows you are not playing in draw without effects, HD pictures
  and DLSS, at half the resolution: a window you can see at 5 frames a second, one covered by another game window or
  minimised at 2. Their music and sounds fade out, on the full screen too. The switches: «Graphics» → «The other game
  windows light», «Sound» → «Music in the other game windows» and «Sounds in the other game windows».
- **The character in the window's title:** name, levels, experience, HP, SP, weight from 50 %, map and the client's
  name, for example Lena · 99/70 · 45,2 % · HP 85 % · SP 40 % · prt_fild08 - uaRO HD.
- **Each character's window at its own place.** Drag a framed window, and when that character enters the game again,
  the window goes there by itself.
- **Each window has its own logs** (`uaro-graphics-2.log` and so on), and a report from the game takes its own
  window's logs.
- **The frame counter and the console (~)** hide as soon as the game window goes behind, instead of staying over the
  other window.

# uaRO Graphics 3.0.3-beta.12 — what is new

- **The game's cursor really over the windows' panel.** In 3.0.3-beta.11 I looked for the cause in the wrong place: the
  game hands its frame to the graphics card before it is shown, and the panel was added after that, with no cursor
  left in the frame. Now the panel goes into the frame together with the cursor, and the cursor is drawn last.
- **The other game windows' sounds fade out on the full screen too.** A full-screen game behind draws no frames, so
  neither «Sounds in the other game windows» nor «Volume in the background» worked for it. Now a window behind makes
  itself quieter by itself, without frames.
- **The background windows' HP and SP on the panel keep up,** even on the full screen.

# uaRO Graphics 3.0.3-beta.11 — what is new

- **The game's cursor over the windows' panel.** On maps with effects (such as prontera) the cursor ended up under
  the panel and looked dark through it. Now it is always over the panel, the tip and the menu.
- **A new panel layout, «Icons».** Small tiles with the windows' numbers and HP/SP bars: the panel takes several
  times less room. The character's name and levels show in a tip when the mouse is over a tile.
  - To turn it on: right-click the panel → «Layout» → «Icons». Or «The panel's layout» in the settings or the console
    (~).
  - Drag the panel by the dots on its left.
- **The frame counter no longer freezes.** When a full-screen game went behind, its counter stayed over the other
  window with its last numbers. Now it hides at once, as the console (~) does.

# uaRO Graphics 3.0.3-beta.10 — what is new

- **The windows' panel has a menu on the right mouse button.** Right-click the panel and a menu opens by the cursor.
  In it you can change:
  - the layout: «In a row» or «In a column»;
  - the size: 75, 100, 125 or 150 %;
  - the opacity: 60, 70, 80, 90 or 100 %;
  - «Music in the other game windows», «Sounds in the other game windows», «The other game windows light»;
  - «Hide the panel» (Ctrl+Alt+0 shows it again).
- A change works at once and is kept in the settings; the menu stays open. It closes on a click off it, on Esc (the
  game's own menu does not open then), or when you go to another window.

# uaRO Graphics 3.0.3-beta.9 — what is new

- **No blink on the full screen.** On its full screen the game minimised its own window as soon as it was not in
  front and maximised it again - the blink when switching. Now it does not minimise when you go to another game
  window, and the switch is instant. To the desktop or another program it still minimises, as before.
- **A sharp picture at once.** A game in a window gets its whole picture ready behind the current one first (0.4 s at
  most) and comes to the front sharp, with DLSS and the effects. On the full screen windows behind draw nothing at
  all, so they are shown at once.
- **Lighter background windows.** One seen behind: 5 frames a second instead of 10; one covered by another game
  window, or minimised: 2. A minimised one used to run its loop about 60 times a second.
- **The other game windows' sounds fade out** as their music does: only the window you play in is heard. The row
  «Sounds in the other game windows» on «Sound».
- **The game's cursor stays over the windows' panel** instead of turning into Windows' arrow.

# uaRO Graphics 3.0.3-beta.8 — what is new

- **The windows' panel is in the game's picture itself** now, not a small window over it: it shows in a recording of
  the screen too. A click on a card still brings that window to the front, and the game does not get the click.
  Over the panel Windows' own arrow shows.
- **A button beside its cross** lays the cards in a row or a column, right in the game.
- **The panel's own settings:** «Game and chat» has a section of its own, «The windows' panel» - «The windows' panel
  in the game», «The panel's layout», «The panel's size» and the new «The panel's opacity». The same rows are in the
  game's console (the ~ key): the panel goes on or off without leaving the game.

# uaRO Graphics 3.0.3-beta.7 — what is new

- **The windows' panel in the game.** With two or more game windows open, a panel shows over the game: a card per
  window - its number, the character's name, the levels and thin HP and SP bars (red under 25 %, as in the game).
  A click on a card brings that window to the front; the current one is blue. The game does not get the click: the
  character goes nowhere.
- Drag the panel by its top row wherever you like. Its place is one for all the windows and kept from the nearest
  edge, so it stands the same in a small window and a big one. Its cross or Ctrl+Alt+0 hides it, Ctrl+Alt+0 shows
  it again.
- The panel grows with «Interface size». The rows «The windows' panel in the game», «The panel's layout» (in a row
  or a column) and «The panel's size» are on «Game and chat» → «Several game windows».

# uaRO Graphics 3.0.3-beta.6 — what is new

- **Keys that switch the game windows.** Ctrl+Tab goes back to the game window you played in before, and the next
  Ctrl+Tab back again. Ctrl+Alt+1...8 goes straight to the window of that number: the order the windows were started
  in, the first one 1. A minimised window is restored. The game does not get these keys: Ctrl+Alt+1 sends no chat
  macro. The row «Window keys» («Game and chat» → «Several game windows»): «Ctrl+Tab and Ctrl+Alt+1...8», «Ctrl+Tab
  alone» or «Off».
- The panel of the windows in the game comes in the next beta.

# uaRO Graphics 3.0.3-beta.5 — what is new

- **The character in the window's title.** The game window's title and its taskbar button show more than the name
  now: the levels, the experience to the next level, HP and SP, the weight from 50 % and the map, with the client's
  name last: Lena · 99/70 · 45.2 % · HP 85 % · SP 40 % · prt_fild08 - uaRO HD. Before the game it is the client's
  name alone in place of «2007-05-07a kRO Sakexe». The switch is «The character in the window's title» now
  («Game and chat» → «Several game windows»).

# uaRO Graphics 3.0.3-beta.4 — what is new

- **The several windows' switches under a heading of their own.** «Game and chat» has «Several game windows» now,
  with «The character's name in the window's title» and «Each character's window at its own place». In
  3.0.3-beta.2 they stood under ROExt's and did not show at all without its work in the driver.
- A window going to its character's place can no longer draw a frame within another one: the check moved to the
  frame's start.

# uaRO Graphics 3.0.3-beta.3 — what is new

- **The other game windows light.** With several game windows open, the main one is the one in front (or the one you
  played in last while another program is in front). The others draw without effects, HD pictures and DLSS, at half
  the resolution and 10 frames a second: less work for the graphics card. The window you switch to becomes the main
  one at once and is whole again. Turned off on «Graphics» → «The other game windows light».
- **Music in the main window only.** The other game windows' music fades out; their sounds stay, as quiet as «Volume
  in the background» says. Music in every window again: «Sound» → «Music in the other game windows».
- With one game window nothing changed.

# uaRO Graphics 3.0.3-beta.2 — what is new

- **The character's name in the game window's title.** In the game the window's title and its taskbar button say
  whose window it is: "Lena — Ragnarok". With several game windows open it shows at once which is the priest's and
  which the sage's. At the login and the character select the title is the usual one. Turned off on «Game and chat»
  → «The character's name in the window's title».
- **Each character's window at its own place.** Drag a character's window where you want it: a second later the
  driver keeps the place, and when that character comes into the game again, its window goes there by itself. For a
  window with a frame (not «A borderless window locked in place»); a place on no monitor is skipped. The window's size
  is one for every window: it is the game's resolution. Turned off on «Game and chat» → «Each character's window at
  its own place».

# uaRO Graphics 3.0.3-beta.1 — what is new

- **Several game windows: each has its own logs.** A second game started beside the first emptied the first one's
  log, and then both wrote into one file mixed together. A window opened beside another now writes logs of its own:
  `uaro-graphics-2.log`, `uaro-render64-2.log`, `uaro-pad-2.log`, the third one's with `-3` and so on up to eight.
  The first line of such a log says which window it is.
- **A report from the game (Ctrl+Shift+F10) and the automatic one after a crash** take the logs of the window they
  came from. A report from the app also carries the other game windows' logs of the last 12 hours.
- The safe mode judges a failed start by the last game of the same window.

# uaRO Graphics 3.0.2 — what is new

- **The game opens and maps load many times sooner.** With HD textures the game's first picture comes 0.3 s after
  the start instead of 12-15 s: the texture pack opens alongside the game, and its shader is compiled at the first
  start alone. A map loads in 0.2-0.3 s instead of 3.7 s: the loading picture no longer waits for the monitor, and
  our find of the cell under the mouse is not checked against the game's own while the game sets the map up. The HD
  pictures are decoded by several threads, the characters' and monsters' frames first; `cps.dll` inflates the game's
  archives with libdeflate.
- **Help.** A new menu item «Help» in every mode: what the program does, the game's keys and what each function of
  each page does. The game's own options on «Game and chat», «Sound» and «Screen» got descriptions too.
- **The game's keys on «Main».** Ctrl+~ the effects console, Ctrl+Shift+F10 «New report» with a screenshot,
  Ctrl+Shift+F11 the frame counter, Ctrl+Shift+F12 a full-size screenshot, Pause or Scroll Lock every improvement off
  and on.
- **The important buttons are blue:** «Play», «Update and play», «Update now», «Apply», «Apply and play»,
  «Recommended settings», «Send» and the other main actions.
- **The client's graphics updates.** The administrator's new GRF archives come with the client's update into the
  `updates` folder and go over the game's archives; DATA.INI is not changed. What it holds shows on «Service» →
  «The client's graphics updates», where they can be turned off.

# uaRO Graphics 3.0.2-beta.15 — what is new

- **A quick map load on every entry.** Some loads took 2.6-2.7 s again. The game's new camera gets the old one's
  memory with the old matrix in it, and the driver took the loading picture for the world: then it waited for its
  present again. The driver now follows the game's own map start, so both of 3.0.2-beta.13's speed-ups and the
  measure work on every change of map. The checks of the cell under the mouse begin anew on every entry, the same
  map's too.
- **A load not measured, the log says why:** `map <map>: its loading not measured - …`.

# uaRO Graphics 3.0.2-beta.14 — what is new

- **A map's loading measured on the next entry too.** Maps load in 0.25 s now, and the same map's next entry was
  taken for a moment without the world (under 0.3 s) and not written. Now whatever shows the game's loading picture
  is a load: the lines `map <map>: loaded in …` come for every entry.

# uaRO Graphics 3.0.2-beta.13 — what is new

- **Maps load sooner.** 3.0.2-beta.12's measure split `prt_fild08`'s loading (3.7 s) in two halves, and both are
  gone now:
  - the game draws its loading picture anew for each per cent of the map's models - 102 times, and each was shown
    after a wait of 16.5 ms (1.7 s in all). While a map loads it is shown at once now, with no frame limit and no
    wait, and goes to the 64-bit renderer at most 10 times a second;
  - our quick find of the cell under the mouse was checked against the game's own 240 times on each new map, and the
    game's own takes 7.6 ms (1.8 s in all). Now 30 checks, and the map's first picks, while the game still sets it
    up, look for no cell at all: the game's own answer then is none as well.
- **A fuller measure.** The block `map <map>` now gives the loading pictures (and how many were shown) apart from
  the game's frames, and how many times the cell under the mouse was looked for and what its checks took. A new step
  of the game's: `the cell under the mouse`.

# uaRO Graphics 3.0.2-beta.12 — what is new

- **A map's loading measured.** Each map's loading is now measured by itself, from its loading picture to the first
  frame of its world. `uaro-graphics\logs\uaro-graphics.log` gets a block of lines `map <map>: loaded in … s`: the
  frames shown meanwhile, where the game's thread was (Windows' functions - `Sleep`, `ReadFile` and so on; the game's
  steps - its files read from the GRFs, the textures' pictures, the world and its near models, a frame shown, the
  server's packets), how long the game's thread waited for the 64-bit renderer and what `cps.dll` inflated. The
  measuring speeds nothing up: it shows what to speed up next. Inflating with libdeflate (3.0.2-beta.11) left
  `prt_fild08`'s loading almost as it was (3.75 → 3.69 s), so the time goes elsewhere.
- **The pack's pictures: the frame's length.** The line `AI textures: the pack's pictures` now gives, beside the wait
  for the frame, how long that frame lasted (`its frame lasted`): whether a picture waited for a long frame of the
  game's (it loads a window's icons in one frame) or for our code.

# uaRO Graphics 3.0.2-beta.11 — what is new

- **Maps load sooner.** Our `cps.dll`, through which the game inflates every file of its GRF archives, now runs on
  libdeflate: on the client's files it inflates several times as fast as the version before, byte for byte the same.
  As before, it is turned off on «Service» → «The game's libraries».

# uaRO Graphics 3.0.2-beta.10 — what is new

- **HD textures on a map come sooner.** The pack's pictures are now decoded by several threads, half the processor's
  cores, not by one. The characters' and monsters' frames go first and no longer wait while a big ground texture is
  decoded. In `uaro-render64.log` the line `AI textures: on` says how many threads decode the pictures.

# uaRO Graphics 3.0.2-beta.9 — what is new

- **The game does not wait for the texture pack.** `uaro-textures.zip` now opens alongside the game: the game's window
  shows at once, and the HD textures come on as soon as the pack is ready (usually in 1-2 s). The textures, sprite
  frames and icons that showed up meanwhile get their HD pictures by themselves, with no map entered again. In
  `uaro-render64.log` - the lines `opens alongside the game` and `opened alongside the game, N s after the start`.

# uaRO Graphics 3.0.2-beta.8 — what is new

- **The game opens sooner with HD textures.** Before the game's first picture, five shaders of the HD textures were
  compiled on every start, four of them needed only by the neural network. Now the pack's pictures take one, compiled
  on the first start only: after that its code comes from `uaro-graphics\cache`. In `uaro-render64.log` - the line
  `AI textures: shaders - N kept from the last start`.

# uaRO Graphics 3.0.2-beta.7 — what is new

- **The HD textures' start measured.** Two new lines in `uaro-graphics\logs\uaro-render64.log`. `AI textures:
  started in … ms` says what each step of the HD textures' start takes: every shader, opening the pack, reading and
  parsing its index. `AI textures: the pack's pictures, ms on average (at most)` says where a picture of the pack
  spends its time: the queue, the read, WebP, the fitting, the wait for the frame; apart for sprite frames, icons,
  the world's textures and the interface's. The measurement speeds nothing up: it is for the next betas.

# uaRO Graphics 3.0.2-beta.6 — what is new

- **Help.** A new menu item «Help» in every mode: what the program does, the game's keys, then view by view what each
  page is for and what each of its functions does. The descriptions are the ones that show when the mouse stays over
  a row.
- **The game's options described.** The game's own options on «Game and chat» and «Sound» (/effect, /snap, /camera,
  whispers, the minimap and more), and «Show» and «Monitor» on «Screen», got descriptions: they had their names only.
- **Fixed:** in 3.0.2-beta.4 and beta.5 some texts stood in the wrong places: «Main» showed the keys' list instead of
  the heading «Keys in the game», the graphics updates' texts and the window place's hint were mixed up. Now the
  program does not build when a text is out of its place.

# uaRO Graphics 3.0.2-beta.5 — what is new

- **The client's graphics updates.** The administrator's new GRF archives come with the client's update from the
  site into the `updates` folder and go over the game's own archives: nothing is repacked, DATA.INI is not changed.
  The newer archive (by name) is over the older, single files in the `data` folder over everything as before. What
  the folder holds shows on «Service» → «The client's graphics updates», where it can be turned off. While there are
  no updates, nothing changes.

# uaRO Graphics 3.0.2-beta.4 — what is new

- **The game's keys in the app.** «Main» has a «Keys in the game» section: Ctrl+~ the effects console,
  Ctrl+Shift+F10 «New report» with a screenshot, Ctrl+Shift+F11 the frame counter, Ctrl+Shift+F12 a full-size
  screenshot, Pause or Scroll Lock every improvement off and on. Under «Play», a short line with the most needed.

# uaRO Graphics 3.0.2-beta.3 — what is new

- **«Recommended settings» blue too.** The button on the «Main» page is now blue, as the other important ones.

# uaRO Graphics 3.0.2-beta.2 — what is new

- **«Apply and play» blue too.** At the bottom of the app both buttons are blue now: «Apply» (when there is
  something to apply) and «Apply and play».

# uaRO Graphics 3.0.2-beta.1 — what is new

- **The important buttons are blue.** The app marks its main actions in blue: «Play» and «Update and play» on
  «Play», «Update now» on «Play», «Main» and «Service», «Update» in the bottom strip and in the update window before
  the game, «Apply» (when there is something to apply), «Send» in «Reports», «Make the pack» and «Fix». Under the
  mouse a button darkens a little, pressed more. In Windows' high contrast they stay as they were. Check how they
  look and that their text reads well.

# uaRO Graphics 3.0.1 — what is new

- **uaRO HD next to the usual uaRO.** Each client has its own shortcut («uaRO», «uaRO HD») and its own line in
  Windows' «Apps», and the game's new files come only to the client they were put out for. Palettes.grf and
  dinput8.ini you changed yourself stay yours. An unpacked full client gets its shortcut at the app's first start.
- **Updates on «Play».** It now has «Update channel» («Release» or «Beta (testing)»), «Check for updates» and
  «Update now» - it updates and opens the app again, the game does not start. The check's answer shows right after
  the app starts.
- **Our own game libraries in place of the old ones of the 2000s.** The archives' compression (cps.dll), JPEG
  pictures (ijl15.dll), the game's sound (mss32.dll, through XAudio2 - on Windows 10 and later) and ROExt's work (the
  window, mouse buttons, keys - now in the driver, dinput8.dll not needed); Bink video without RAD's code
  (binkw32.dll). The originals are in `uaro-graphics\original`. If something sounds or behaves differently than
  before, turn that library off: «Service» → «The game's libraries» («Detailed» mode) - and write to «Reports».
- **Grass on the flower beds.** The round flower beds by Prontera's fountain and the same ones on other maps grow
  grass like the ground around them.

# uaRO Graphics 3.0.1-beta.9 — what is new

- **The update check's answer at once.** The app starts its checks (the update, «Play»'s news, the client's patch,
  the reports) before it shows the page, and their answer appears on the page and in the bottom strip the moment it
  comes, not a few seconds later. Check how soon after the app starts «Update now» or «… is the newest version» shows.

# uaRO Graphics 3.0.1-beta.8 — what is new

- **ROExt's work in the driver.** The game's window, mouse buttons, keys, freeing the processor in the background
  and the text's code page are now done by the driver itself. ROExt's dinput8.dll went to `uaro-graphics\original`,
  and the settings of its dinput8.ini into uaro-graphics.ini. To turn it off: «Service» → «The game's libraries» →
  «Window, mouse and keys: ROExt's work in the driver (dinput8.dll)». The settings themselves are on «Game and chat»
  («Detailed» mode). Check: the game's window (its size and place as before), changing the window's size on the
  «Screen» page, the side mouse buttons if you put skills on them, Ctrl+W and Alt+F4 if you turned them on in
  dinput8.ini, and that a minimized game takes less of the processor.

# uaRO Graphics 3.0.1-beta.7 — what is new

- **Our own mss32.dll: the game's sound.** The game's effects and music are now played by our library through
  Windows' XAudio2 in place of RAD's Miles Sound System 6. It is put in on Windows 10 and later only; older Windows
  keep the original. To turn it off: «Service» → «The game's libraries» → «Game sound: ours (mss32.dll)». Check the
  music on a few maps (and after going from map to map), hits and spells in a fight, NPCs' and towns' sounds, the
  effects' and music's volume in the game's settings, and setup.exe's 3D sound if you have it on. If the sound is
  gone, crackles, comes from the wrong side or its volume is clearly different, write to «Reports».

# uaRO Graphics 3.0.1-beta.6 — what is new

- **Our own ijl15.dll.** The game's JPEG pictures (the loading screens) are read and its Print Screen shots written
  by our library in place of Intel JPEG Library 1.5 of 2000. To turn it off: «Service» → «The game's libraries» →
  «JPEG pictures: ours (ijl15.dll)». Check the loading screens and a Print Screen shot with the «uaro.kiev.ua» mark.

# uaRO Graphics 3.0.1-beta.5 — what is new

- **The game's libraries, our own in place of the old ones.** uaRO Graphics puts its own cps.dll in the game's
  folder - the compression the game reads its archives through (a current zlib in place of the 2000s' one) - and a
  binkw32.dll without RAD Game Tools' code (the game has no Bink video). The game's own files wait in
  uaro-graphics\original and come back when a library is turned off («Service» → «The game's libraries», the
  Detailed mode) or uaRO Graphics is removed. It changes while the game is closed.

# uaRO Graphics 3.0.1-beta.4 — what is new

- **«Update now» on «Game».** When a newer version is out, «Update now» shows beside «Check for updates»: the update
  installs and the settings app opens again - the game does not start. «Update and play» works as before.

# uaRO Graphics 3.0.1-beta.3 — what is new

- **Grass on the flower beds.** The round flower beds by Prontera's fountain (and the same beds on other maps) now
  grow grass like the grassy ground round them (report R-SA3W7Y). A bed is a model whose top stands a little over the
  map's ground, and the grass grew on the ground itself only.

# uaRO Graphics 3.0.1-beta.2 — what is new

- **The update channel and the check on «Game».** The first page's «Updates» part now has «Update channel» («Release»
  or «Beta (testing)») and the «Check for updates» button, in every mode of the program. A changed channel asks its
  branch at once; when a check fails, the reason shows there too.

# uaRO Graphics 3.0.1-beta.1 — what is new

- **uaRO HD next to the usual uaRO.** The new full client «uaRO HD» lives in a folder of its own. Each client now has
  its own desktop shortcut («uaRO» and «uaRO HD») and its own line in Windows' Apps («uaRO Graphics» and «uaRO Graphics
  (uaRO HD)»). A second copy never takes the first one's shortcut or line, and a removal takes away only its own line.
- **Client updates, each its own.** New game files come only to the client they were put up for. A Palettes.grf or a
  dinput8.ini you changed yourself stays yours. A file taken off the update goes from your folder too, when the update
  itself put it there and you did not change it. While uaRO Studio or the renderer of this folder is open, the game's
  files are not changed; the other client's game is no obstacle.
- **A shortcut for an unpacked client.** A full client unpacked from its archive gets its shortcut and its line in
  Apps at the program's first start.

# uaRO Graphics 3.0.0 — what is new

- **The launcher «Play».** The «uaRO» shortcut opens the server's news, the online count and «Play». A new version or
  new game files - «Update and play» does it all.
- **No .bat.** Installing, updating, going back to the previous version and removing - in the app, with a progress
  window. Updates come from GitHub by themselves; for new builds before everyone - Service - «Update channel» - «Beta».
- **Sets itself up.** On a new install the settings are picked for your graphics card and screen; integrated graphics
  get the super-light mode. Your present settings stay. Two failed starts in a row - the safe mode by itself.
- **«Reports».** Ctrl+Shift+F10 in the game or «Reports» in the app: a description, screenshots - and the developers'
  answer comes there too. The app asks whether to send a report by itself when the game crashes.
- **The gamepad inside.** Xbox and PlayStation work at once; no separate files any more.
- **A clean game folder.** All of ours is in the `uaro-graphics` folder.
- **Nothing extra.** DLSS rows on NVIDIA RTX only; the effects' console on Ctrl + the key left of 1 («ё» and the
  apostrophe typed in the chat); Pause says the improvements are off. HDR goes on by hand only.
- **Paving without dark lines.** On Aldebaran's square and other maps with such tiles no dark lines and smudges run
  along the tiles' edges any more.

# uaRO Graphics 3.0.0-beta.3 — what is new

- **Paving without dark lines.** On Aldebaran's square and other maps with such tiles no dark lines and smudges run
  along the tiles' edges any more (report R-45AV5H). The effect «Grass and road seams without steps» drew them: it took
  the invisible walls of no height between tiles for the floor and spread the wall's texture and its shadow along the
  edges.

# uaRO Graphics 3.0.0-beta.2 — what is new

- **Two update channels.** Updates come from GitHub: «Release» for every player, «Beta» - the testers' builds, before
  everyone. A beta build puts itself on «Beta»; switch it on Service - «Update channel» (in the Detailed and
  Developer modes). «Could not check» on the Service page is gone.
- **HDR off by default.** The auto setup no longer turns it on; turn it on with the «HDR» row on the Main page.
- **A report when the game crashes.** At the first start the app asks whether to send the developers a report when
  the game crashes («Yes» preselected). You can change your mind on the Diagnostics page.
- **If you have 3.0.0-beta.1:** install this version by hand once - the previous one looked for updates on the site.
  Updates come by themselves after that.

# uaRO Graphics 3.0.0-beta.1 — what is new

- **The launcher «Play».** The «uaRO» shortcut opens the server's news, the online count and «Play». A new version or
  new game files - «Update and play» does it all.
- **No .bat.** Installing, updating, going back to the previous version and removing - in the app, with a progress
  window.
- **Sets itself up.** The settings are picked for your graphics card and screen; integrated graphics get the
  super-light mode. Two failed starts in a row - the safe mode by itself.
- **«Reports».** Ctrl+Shift+F10 in the game or «Reports» in the app: a description, screenshots - and the developers'
  answer comes there too.
- **The gamepad inside.** Xbox and PlayStation work at once; no separate files any more.
- **A clean game folder.** All of ours is in the `uaro-graphics` folder.
- **Nothing extra.** DLSS rows on NVIDIA RTX only; the effects' console on Ctrl + the key left of 1 («ё» and the
  apostrophe typed in the chat); Pause says the improvements are off.

# uaRO Graphics 2.8.0 — what to check

- **Our files are out of the game's folder.** Everything of uaRO Graphics is in the `uaro-graphics` folder now: the
  settings (`uaro-graphics\uaro-graphics.ini`), the logs (`logs`), the shader cache (`cache`), the texture pack
  (`textures\uaro-textures.zip`), the settings' copies (`backup`), the client's dgVoodoo (`dgvoodoo`), the notes of
  «Що нового» (`notes`). The root keeps, of ours, `ddraw.dll`, `uaro-graphics.exe` and with the pad `dinput.dll`; the
  .bat files go in the next phase, the pad's files when the pad moves inside uaRO Graphics. `uaro-flags.grf` is the
  client's file now and stays where it is.
- **The move is automatic.** At the first start of the game or the settings app on 2.8.0 the old files move; the
  settings, the copies and the pack are kept. A file that cannot move (a game under Program Files, a pack being built)
  keeps working where it is.
- **Logs.** The previous game's log stays beside it (`uaro-graphics.prev.log`) and goes into a report. A log over
  32 MiB starts a new part; the game's start stays in `uaro-graphics.1.log`.
- What to check after updating from 2.7.58 with «Оновити зараз», starting the settings app and the game:
  1. the root has none of our `uaro-graphics.ini`, logs, `uaro-graphics-pipelines.txt`, `uaro-textures.zip`,
     `uaro-graphics-backup`, `dgvoodoo`, `TEST.*.md`;
  2. the settings are the same (quality, effects, the pad);
  3. HD textures are there, uaRO Studio sees the pack;
  4. `uaro-graphics\logs` has the logs, after the game's second start also `uaro-graphics.prev.log`;
  5. «Що нового в 2.8.0» opens, «Повідомити про проблему» sends a report;
  6. «Сервіс» restores a copy of the settings made before the update;
  7. uninstall.bat puts dgVoodoo back.

# uaRO Graphics 2.7.58 — to check

- **Guild emblems without beige blotches at night** (report R-JSBTFQ). "Windows at night" looks for panes in the
  textures - an even dark patch on a lighter wall - and lights them warm at night (one in three left dark, hence the
  blocks). The emblem's black rider on its light field looked like a window to it. Now panes are looked for only in
  pictures of 64 texels and more: an emblem is 24 in a texture of 32, the houses' walls 128 and more. The houses'
  windows glow as before.
- To check: at night and at dawn the emblems on the guild flags look as by day, only darker; the houses' windows
  still glow at night.

# uaRO Graphics 2.7.57 — to check

- **Straight lines on the floor gone** (R-NSHM8K: Prontera's paving, R-ENZVF2: a road in a field). "Grass and road
  joins without steps" took a texture's tile repeating (its edge at 1 and its start at 0 - the same place) for a jump
  and laid its mirrored strip over every fourth cell's edge - straight lines across the stones and along the roads.
  A repeat is no seam now.
- **Mats and patches rectangular again** (R-8YJSCR). The same effect frayed with its noise the edges of the tatami,
  the paving's squares and the beds laid straight. Now the driver finds in the map's file the patches laid by hand
  (one texture filling a whole rectangle) and keeps their edge straight, as the game does. The edges of meadows and
  roads are blended as before.
- **Pictures keep their colours at night** (R-ZPEV4V: guild emblems). A vivid colour - an emblem, a flag, a dress,
  flowers - keeps 85 % of its colour under a pale silver at night and darkens as all does; ground, stone and walls as
  before.
- **The portal glows at night** (R-6YHMQK). The night filter darkened the portal's white light as it does white stone;
  now the warp's light is three times stronger at night and gives the portal and what is around it their day's
  colours back (never brighter than by day).
- **The grass on the paving no longer blinks as you walk** (R-QPMDP4). A blade needs a green patch round its root, not
  a speck in a crack; the meadows keep their grass.
- To check: no straight lines on Prontera's paving, the tatami and patches rectangular, no line along a field's road;
  at night emblems and clothes in colour, the portal glowing; the grass on the paving not blinking.

# uaRO Graphics 2.7.56 — to check

- **The portal is the game's own, at full resolution** (report R-9GC93H: the new and the old portal did not differ
  at all). 2.7.55 recorded the warp's real pictures: the game's ring is a white band at the ground and a crown of
  light spikes rising up the wall and turning blue. But the whole picture is 64 texels round a ring with 16 steps of
  alpha - hence the blur. Now it is drawn anew by the same shapes at full resolution:
  - the rings: a crisp white band at the ground and a crown of spikes (large, middle and fine), smooth edges, the
    spikes gently breathing;
  - the game's colours and brightness: white at the ground, blue spike tips;
  - the floor: the same white haze, smooth, without its steps, a little less (80 %);
  - the sparks: the game's.
- **The warp's pictures are no longer written to the log** - they are in.
- To check: at the door in Prontera by day and by night, crisp white rings with a crown of spikes instead of a blur,
  recognisably the same portal. Compare by unticking and ticking "New portal".

# uaRO Graphics 2.7.55 — to check

- **The portal neater and closer to the game's** (report R-C6HFHG: make it neater and more like the original). In
  2.7.54 the doorway filled with lilac, the walls' tops burned in a bright line, thin arcs stuck out onto the street.
  Now the rings and the floor come from the game's own pictures again, only cleaned:
  - the ring's picture averaged round the ring - no streaks or noise, its colour and alpha down the wall as the
    game's (white at the ground, bluish lavender higher up);
  - a fine bright circle where each wall meets the ground - the original's main feature, sharper;
  - the walls' tops fade softly, less glare (the walls 85 %, the floor 75 % of the game's);
  - the sparks are the game's again, the light around ("Living portals") white lavender.
- **The warp's pictures go into the log once** («portal capture: texture …» lines) - their alpha is in no copy on
  the server; with them the next comparisons are exact.
- To check: at the door in Prontera by day and by night the portal looks like the original - white circles and a soft
  haze, no lilac box, no streaks on the walls. If you do not like it, untick "New portal" - the game's own comes back.

# uaRO Graphics 2.7.54 — to check

- **The new portal, made on the facts, is on again** ("New portal" ticked by default). The driver no longer draws a
  portal of its own over the game's: the game's own draws of the warp (effect 321) are shaded anew where it draws
  them. Two portals, or one off its place, cannot happen now.
  - The rings are the game's: two sets of 4, flat at the edge, coming in to the middle and standing up as walls. Each
    wall now has a crisp bright line along its top, lilac light down it and a fine circle where it meets the ground.
  - The portal's floor is a lilac pool instead of the white haze: by day the paving under it is no longer burnt white.
  - The sparks are crisp stars.
  - All of it from the game's record (report R-FP2F4H) and the client's code: the rings' sizes, speed (0.05 a tick),
    tilt, fades and pictures.
- **The warp's light on what is around** ("Living portals"): a soft lilac light set exactly at the middle of the
  portal's floor. The air's swirl is gone - it blurred the rings.
- **The portals' record into the log is gone**: in 2.7.53 it blew the log up to 24 MB.
- To check: at the door in Prontera by day and by night - one portal where it should be, the rings sharp and coming
  in to the middle, the floor not white. The log has «portals: the warps shaded anew on …». If you do not like it,
  untick "New portal" - the game's own portal comes back.

# uaRO Graphics 2.7.53 — to check

- **The new portal is off** ("New portal" unticked by default). Everyone has the game's own portal again, not two.
  The glow and swirl of "Portals" now come only with the new portal.
- **The driver records the game's portals.** On the first three maps a portal is seen on, two seconds of its draws go
  into the log: every triangle, its depth, alpha, blending, and the portal's pictures themselves. The new portal will
  be made anew on these facts and checked before it is built.
- What to do: start the game, go straight to the door in Prontera, stand three seconds with the portal on the screen,
  take a screenshot and send a report.

# uaRO Graphics 2.7.52 — to check

- **A portal on a tower's roof and a light rectangle by the windmill.** 2.7.51 looked for the ground under a portal in
  the picture where it stands on the screen. For a portal behind the tower that place was the tower's roof, and the
  portal hung there. By the windmill its foot went into the building and its column of light was cut at the edge of the
  pass's box. Now its foot comes from the depth of the game's own draws: a fan of triangles on the ground round the
  portal's middle, its depth right. A portal behind the tower hides behind it, as the game's does. The pass's box is
  measured by the real column again.
- To check by the windmill and the tower in Prontera, where the shot was: the portals stand on the ground at the doors,
  none shows through the tower, no rectangles; they do not drift as the camera turns.

# uaRO Graphics 2.7.51 — to check

- **The new portal stood in the wrong place and "moved".** The game draws the exit portal (effect 321) as a flat
  picture over the screen, and its depth is not the world's. The driver took the portal's place from that depth and put
  it far past the screen's edge. Now its place is where the game draws the portal on the screen, and the shader finds
  the ground under it in the picture's own depth. Huge draws reaching past the screen no longer join a portal. The
  portal's light on the ground is in its place too.
- To check at the door in Prontera: one portal where the old one was, not drifting as the camera turns. The log has
  "portals: the new one drawn on … at X,Y" and the first lines "portals: a draw of effect 321 …" - what the game makes
  it of.

# uaRO Graphics 2.7.50 — to check

- **The new portal was still not drawn; the real cause is found.** The driver started each portal it noticed at
  strength 0 and in the same frame threw out everything not above 0, so no portal lived longer than a frame. The
  mistake is old: the former blue glow of "Portals" never worked because of it either. Now a portal stays, and the
  following is checked by a test.
- To check at the door in Prontera: the new portal should come (three waves of light to the middle). The log gets a
  line "portals: the new one drawn on …".

# uaRO Graphics 2.7.49 — to check

- **The new portal was not drawn** (2.7.48 left the game's portal out and the new one never came). Fixed:
  - the driver notes the game's portal even when the game draws it after the lights' pass;
  - the new portal stands on the ground the picture's depth shows, not only the map's file, so its walls do not sink
    under the ground.
- **A safeguard:** the game's portal is left out only while the new one was drawn the frame before. There is never no
  portal at all.
- To check at the door in Prontera. The log gets a line "portals: the new one drawn on …" with its place and its size
  on the screen.

# uaRO Graphics 2.7.48 — to check

- **New portal** ("New portal", a part of "Portals", on). The game's portal drawn anew: the same three waves of light
  closing in on the middle, the column of light and the sparks, but sharp and with body. The walls are brighter at
  the ground and melt upwards, their edges glow, a soft halo round them. The paving under the waves ripples as under
  water, a soft light in the middle, streams run up the column. The old blue glow of "Portals" under it is softer
  and lilac. Untick it and the game's own portal is back.
- To check at the door in Prontera and at a map's exit: the portal stands in its place, the waves go to the middle,
  houses and characters hide it. The log has a line "portals: the game's effect N drawn anew" telling which of the
  game's effects was replaced.

# uaRO Graphics 2.7.47 — to check

- **Lit things keep their own colours.** The light of fires, torches, lamps and skills took 70 % of its colour from
  the light itself and the rest from the picture the night had already washed grey. So a dark robe by a fire turned
  pale pink and the grass turned brown. Now a lit surface shows its day's colours, and the light only warms them. At
  night under the open sky a lit surface is no brighter than by day, so a character no longer fades. Torches in
  caves light as before.
- **Colours come back in the moon's beams** ("Moon beams"): 60 % of the colour in a beam's middle instead of 35 %, and
  the silver less blue. The brightness is the same.
- To check at night on a field: a character by a Fire Wall or a torch keeps their colours, only warmer. The grass by
  the fire is green-yellow, not brown. In a moon beam the colours show.

# uaRO Graphics 2.7.46 — to check

- **Skills solid again** ("Skills that do not blind"). It also softened the skills' pictures laid over the world - by
  their alpha: they add no light, they only went see-through. Now only what adds light is made softer. The night's
  softening (2.7.37) is gone - with 2.7.38's soft adding the skills do not burn white anyway.
- **"Glittering snow" and "Breath in the frost" removed** - after three rounds in Lutie they were hardly seen.
- To check at night: Fire Wall, Magnus and others - more solid and brighter, no white blots. Better set the "Skills"
  group's strength back from 190 % to 100 % - it also raises the skills' fire light on the characters.

# uaRO Graphics 2.7.44 — to check

- **Breath at the head**, not the feet: the puff rose 6.2 units over the feet - knee height; now 15 (at the face).
- To check in Lutie: stand - puffs of breath at the characters' heads.
- Still waiting: the snow's glints - by day under Weather - Clear press Ctrl+Shift+F9, a report, off again; puddles -
  under Rain on Prontera's paving and on Comodo's beach.

# uaRO Graphics 2.7.43 — to check

- **In the rain only puddles reflect** ("Wet ground and puddles"). The whole wet ground used to mirror, and big grey
  stains lay on the sand. Now the wet ground is only darker, and the puddles mirror - on hard ground only: paving,
  stone, trodden earth. Sand, grass and snow take the water in - no puddles on them. The puddles are smaller, the sky
  in them darker and more of it looking along.
- To check: Weather - Rain. On Comodo's beach dark wet sand, no puddles or reflections; in Prontera puddles on the
  paving with the characters reflected and the drops' rings.
- Still from 2.7.42: breath in Lutie; the snow's glints - with Ctrl+Shift+F9 ("X-ray of the AI pictures") by day under
  Clear, a report, then off.

# uaRO Graphics 2.7.42 — to check

- **Breath shows.** A puff rose at the character's head, but the character's own sprite is drawn nearer the camera
  and hid all of it but its rim over the snow, and grey-blue on white does not show. Now the puff is before its own
  character.
- **Glittering snow - a check.** Still no glints, though by the count there should be hundreds. To find where they are
  lost: in Lutie by day, Weather - Clear (no snowfall), turn on Ctrl+Shift+F9 ("X-ray of the AI pictures") - the snow
  the driver knows turns pinkish, red dots where the glints should be. Send a report and turn the X-ray off with the
  same Ctrl+Shift+F9.

# uaRO Graphics 2.7.41 — to check

- **"Glittering snow" shows.** The glints were under a pixel across and white on white snow - none was seen. Now a
  glint every step or two, a pixel or two across with a short four-pointed cross, tinted as light through ice - blue,
  gold, rose; it flashes and fades as you walk.
- **"Breath in the frost" easier to see**: a puff of a cold grey-blue, a little bigger and denser (near white, it was
  lost on the snow).
- To check in Lutie (xmas 150 180): walk over the snow - the glints; stand - puffs of breath at the characters.

# uaRO Graphics 2.7.40 — to check

- **One snow, not two.** Over Lutie both the game's snow and ours ("Precipitation") fell. Now where ours falls the
  game's is not drawn; under a roof and with "Precipitation" off it is as before.
- To check in Lutie (xmas 150 180): one snow; hold Tab on "Precipitation" - the game's snow comes in place of ours.
  There too: "Glittering snow", "Breath in the frost", footprints in the snow.

# uaRO Graphics 2.7.39 — to check

- **Quieter casting** ("Skills that do not blind"). While a skill is cast a funnel of light rises round the hero's
  feet - white as the game draws it. Now it is drawn at 40 % of "Skills' brightness" (at 50 %, 20 %) and no longer
  lights the hero and the ground, makes no glowing air and no halo - a faint pale funnel, not a white flash. The skills
  themselves as in 2.7.38.
- To check: cast by day and at night (Magnus, Heal, buffs) - no white haze round the hero.

# uaRO Graphics 2.7.38 — to check

- **Skills no longer burn to white** ("Skills that do not blind"). The game draws skills by adding their light to the
  picture: by day on Comodo's light sand even half a Magnus column came over 100 % - white (the game's own does the
  same with Pause). Now the skills' light is laid on softly: the lighter what is under it, the less it adds. On dark
  ground nearly as before; on light ground and where layers pile up - no white blots. All skills and casting.
- To check: Magnus Exorcismus, casting, buffs by day and at night - no white blots; tune "Skills' brightness" if wanted.

# uaRO Graphics 2.7.37 — to check

- **Calmer skills at night** ("Skills that do not blind"). At night the world goes dark and the skills did not -
  Magnus and the other big skills stood out as a bright blot. Now they soften with the night: in the whole night (and
  "Deep night") down to 60 % of "Skills' brightness" (at 50 %, 30 %), less in a light night, by day as before. The
  skills' halo and light soften with them.
- To check: Clock - Night 0:00 or Deep night 3:00, Magnus Exorcismus - calmer than on 2.7.36. Also in Comodo:
  Weather - Rain and Tab on "Wet ground and puddles"; during Magnus - Tab on "Sparks of skills".
- All skills and casting: in the effects console (~) under "Skills that do not blind" lower "Skills' brightness"
  with ← to 25-30 % and cast - say which value is calm: it becomes the default (80 % now). The report's log now
  shows at what % the skills are drawn and whether bright effects slip past it.

# uaRO Graphics 2.7.36 — to check

- **"Wet ground and puddles" easier to see.** In the rain hardly more than the reflection changed: the ground darkened
  by only 16 %, and a puddle showed only what stands close by - on open ground none was seen. Now the wet ground is
  darker (by 30 %), a puddle shows the grey sky (dark at night) where nothing close stands in it, and the drops' rings
  are thicker.
- To check in Comodo (comodo 187 135): Weather - Rain, hold Tab on "Wet ground and puddles" - the ground darkens
  plainly, puddles with the drops' rings show on the sand.
- **Calmer skills.** Magnus at night was a white blot: the game's columns pile up in its middle, and our effects
  added over them. Now skills give "Halo around fire and skills" 40 % of their light, the air about their lights
  ("Glowing air at fire and lamps") glows a third, and their light on the ground at night is 1.4 times the day's (it
  was 1.8). Torches and lamps are unchanged.
- To check: Magnus Exorcismus at night - less of a white blot. If still too bright, lower "Skills' brightness".

# uaRO Graphics 2.7.35 — to check

- **Splashes from steps in all the water one can walk in** ("Splashes from steps"). The drops used to fly up only
  where the bed lay no deeper than 3 units under the water, and in Comodo it lies 7 deep in the middle (up to 16): over
  nearly all the shallows there were none. The game has no swimming, one wades - now the splashes come down to two
  cells deep.
- **"Falling leaves" easier to see** (Weather - Falling leaves): the leaves twice as big (they were 7 by 4 pixels -
  orange specks) and a few more of them.
- **Bigger, darker footprints** ("Footprints in snow and sand"): a print half again as big, darker on snow and sand.
- To check in Comodo (comodo 187 135):
  - by day walk into the water to comodo 180 135 - splashes in the deeper water too;
  - Weather - Snow, walk about - the prints; Falling leaves - the leaves;
  - Weather - Rain and stay under it 2-3 minutes: "Wet ground and puddles", "Wet walls and roofs", "Thunderstorms";
  - Clock - Night 0:00, Magnus Exorcismus; while it stands hold Tab in turn on "Glowing air at fire and lamps",
    "Halo around fire and skills", "Fire of skills lights around" and press Pause - with which of them the white blot
    goes; look at the skill's sparks too ("Sparks of skills").

# uaRO Graphics 2.7.34 — to check

- **The birds no longer settle on the water.** A flock ("Birds") lands only where the map's ground stands above the
  water, and at the ground's own height rather than the hero's. In Alberta by the water the birds land on the shore
  and the pavement, never on the water.
- **The sun's glints on the water are calmer** ("The sun's glints on the water"). The bright sparks are half as strong
  and no longer all over the water but in patches that drift slowly with the waves. The broad soft sheen stays.
- To check in Alberta (alberta 167 162, the pier's end): no birds on the water; the glints do not flicker all over it.
  The world's reflections in the water (checked) are unchanged.

# uaRO Graphics 2.7.33 — to check

- **"Map light without blow-out" removed.** By the sewers' wall with a torch it made no difference to be seen: the
  wall is plain itself (with no effects, on Pause, no masonry even far from the fire), and the light by the fire is so
  strong that it sets the colour. Its line in the settings file changes nothing.
- The fire light that keeps a surface's pattern (2.7.31) stays.
- Nothing much to check — on to the next: Prontera's church, prt_church 100 100, the reflections on the floor.

# uaRO Graphics 2.7.32 — to check

- **"Map light without blow-out" (a new switch among "Game fixes", on).** The smooth wall by the torch was the game's
  own: it draws the ground and the walls as their texture in shade plus the map's coloured light, added and cut at
  white. By a torch every shade of the stone came out one yellow. Now a strong map light lies over the texture softly,
  as onto a screen, and never reaches white — the stones by the torches stay. A weak light (the map's tint all over)
  stays exactly as in the game; the brightest patches are a little dimmer.
- Please check: prt_sewb1 166 19, the wall by a torch — is the masonry seen; compare with Tab on "Map light without
  blow-out". Look at the bright spots of other maps too (Prontera's lamps) — are they too dim now. Send a report with
  screenshots.

# uaRO Graphics 2.7.31 — to check

- **The walls by the fire show their stone again.** The torches' light lifted the darks 3.4 times and the mid-tones to
  one level, and put the fire's colour in place of the surface's: the stones' faces by a torch ran together into one
  even orange, a lighter stone could come out darker than a duller one. Now the brightness rises on a smooth curve
  that keeps every step of the pattern (the darks the most), and a third of the colour stays the surface's own. The
  walls and the floor by a fire show their stone, the light's pool a little softer. All fire light changes so:
  torches, lamps, skills.
- If the walls still look smooth, press Tab on "Map light without steps": it smooths the light baked into the map.
  Say whether that is it.
- Please check: prt_sewb1 166 19 and the Pyramids — the walls and floor by the torches. Send a report with a
  screenshot.

# uaRO Graphics 2.7.30 — to check

- **"Leaves in the wind" easier to see.** The trees' tops swung only 1.2 game pixels — under a couple of the
  picture's. Now 4: the crown sways softly, the trunk stays.
- **"Leaves from the trees" to be seen.** The leaves did fall, but tiny (0.38 units, some 6 pixels) and dark green
  over the green forest. Now twice as big, and in summer light ones — yellow-green and yellow. In autumn as before.
- "Volume of the crowns" changes the crowns' light alone (the side to the sun lighter, the far side darker), not
  their shape — as meant.
- Please check: pay_fild01 200 200, by day — the leaves swaying and falling. Send a report.

# uaRO Graphics 2.7.29 — to check

- **Faster grass.** The grass was the costliest effect: 3–5 ms of a frame's 8–13 ms of the card's work at 4K. Now:
  - at 4K the blades' roots are looked for on every second row — a game pixel is two rows of the picture there, the
    same blades for half the work (Direct3D 11 always did so);
  - the flower patches' pattern is worked out only for the blades that may be a flower (about 12 %), not for each;
  - the sway in the wind is worked out once a pixel, not at every step of the search.
  The grass looks the same.
- Ruwach's ball stays as it is for now.
- Please check: stand on prt_fild08's grass for half a minute and send a report — the log line `GPU: effects a frame`
  will show what the grass costs now (it was 3–5 ms).

# uaRO Graphics 2.7.28 — to check

- **Ruwach's ball at the head of its trail all the way round.** 2.7.27's ball took the sprite biggest on the screen.
  But the camera looks down at an angle and the sprites nearer it look bigger than those farther off: when the trail's
  head went round to the far side of the circle, the ball jumped to an older sprite nearer the camera and spun beside
  the trail. Now it takes the most opaque sprite in the air — always the newest, wherever it is on the circle.
- Please check: prt_fild08, Clock → "Deep night 3:00", cast Ruwach and turn the camera — the ball should lead the blue
  trail all the way round. Send a report.

# uaRO Graphics 2.7.27 — to check

- **Ruwach's ball is back — on the trail's circle.** 2.7.26 showed no ball at all: it told the orb's sprites from
  their shadows by the sprites, and the game tells no effect sprite's file (the log: «0 the orb's own» of 18). Now the
  driver reads each sprite's height over its effect: Ruwach's orbs float 10 units over the ground, the shadows lie on
  it. The ball sits on the newest sprite in the air — at the head of the blue trail, on its circle.
- The log line `orbs of light:` now counts the sprites in the air and says where the head is.
- Please check: prt_fild08, Clock → "Deep night 3:00", cast Ruwach — the ball at the head of the trail, on the same
  circle round the hero. Send a report.

# uaRO Graphics 2.7.26 — to check

- **Ruwach's ball on the trail's circle.** Ruwach draws sprites round the hero up in the air and their shadows on the
  ground. 2.7.25's ball took the most opaque of them — a shadow, so it flew under the blue trail, along the ground. Now
  it takes the effect's own sprites alone (no shadows) and of them the newest, the biggest: the ball leads the trail,
  on the same circle.
- The first frames with Ruwach log which sprite the ball is on (`orbs of light:`) — if it sits wrong again, the report
  will show it.
- Please check: prt_fild08, Clock → "Deep night 3:00", cast Ruwach — the ball should fly at the head of the blue
  trail, on the same circle round the hero. Send a report.

# uaRO Graphics 2.7.25 — to check

- **A little ball at the head of Ruwach's trail.** Ruwach's blue trail is its sprites: the newest the brightest, the
  older fading behind it. Now where the newest one appears there is a small bright ball — white in the middle, blue at
  its edge, as a comet's head, the blue trail behind it. It hides behind the hero when it passes behind. Its own
  switch "Orbs of light" under "Fire of skills lights around".
- Please check: prt_fild08, Clock → "Deep night 3:00", cast Ruwach — the ball should lead the trail. If it ends up at
  the trail's tail, or is too small, big or bright, say so and I will tune it. Send a report.

# uaRO Graphics 2.7.24 — to check

- **The ball at Ruwach is gone — the particles are as the game draws them again.** You were right: the game writes
  Ruwach's particles into the frame's depth, so they hide one another — a blue streak of animation shows. 2.7.23 turned
  that off and the particles piled up into a dense ball. Now it is as in the game.
- **Ruwach's light lies on the ground alone.** So that it lights no particles in the air (the circle of 2.7.22), it
  lights only what lies near the ground: the ground, the grass and the characters' feet. The same soft blue pool round
  the hero, the light about two cells over the ground; the hero's body itself is no longer lit by it. The same for
  Sight.
- Please check: prt_fild08, Clock → "Deep night 3:00", cast Ruwach — by the hero the blue streak of animation as in the
  game, no ball and no circle, and the soft blue light on the ground. Send a report.

# uaRO Graphics 2.7.23 — to check

- **The circle next to Ruwach is gone.** The game writes all its laid-over sprites into the frame's depth whole —
  wherever they show at all. So to the light each soft orb of Ruwach was "a thing in the air", and Ruwach's light lit
  it whole — a bright circle or oval next to the hero. Now the orbs of light (Ruwach, Sight) leave the depth as it is:
  the light falls on the ground under them, and the orbs look as the game draws them.
- Please check: prt_fild08, Clock → "Deep night 3:00", cast Ruwach — no bright circle next to the hero any more, only
  the soft blue light around. Send a report.

# uaRO Graphics 2.7.22 — to check

- **Ruwach's light is calmer.** In 2.7.21 Ruwach's orbs gave two lights right by the ground that jumped after the
  orbs and lit the orbs themselves almost white. Now all the orbs round the hero are one steady light over its head:
  a soft blue pool around, about half as bright in the middle, no white blob; the colour a little deeper, blue.
  Strength 0.5 at night (was 0.6), half by day, reaching 5 cells. The same for the mages' Sight.
- Please check: prt_fild08, Clock → "Deep night 3:00", cast Ruwach — the light should be soft and steady. If it is
  still too bright, or too faint, say so and I will tune it. Send a report.

# uaRO Graphics 2.7.21 — to check

- **Ruwach lights — the cause found.** The game draws Ruwach's orbs as sprites laid over the world, not added light,
  and their picture comes from the shared sprite cache. So "Fire of skills lights around" never saw them — neither in
  2.7.19 nor in 2.7.20. Now the driver knows such effects by their number — Ruwach (pale blue-white light) and the
  mages' Sight (fire) — and gives them a light of their own: a torch's at night, half that by day, reaching about 4.5
  cells.
- The log line `skills' light at night` now also counts the orbs' draws (`of orbs of light`).
- The moon beams show on the 2.7.20 screenshots: broad slanting bands on the ground.
- Please check: prt_fild08, Clock → "Deep night 3:00", cast Ruwach — the ground around the hero should light up
  clearly, pale blue-white. Send a report.

# uaRO Graphics 2.7.20 — to check

- **Moon beams.** At night broad slanting bands of moonlight lie on the ground of the fields and the towns, as through
  gaps in the clouds, and drift slowly. They keep to the map: when you turn the camera they stay in place. On the
  whole the night is as bright as in 2.7.19. Their own switch "Moon beams" under "Moonlight".
- **Ruwach lights.** In 2.7.19 its light was too weak — about a tenth of a torch's, so the ground around it stayed
  dark. Now at night every glowing skill's light is at least about a third of a torch's.
- At night the log notes every 10 seconds how much skill light was found (`skills' light at night`): if Ruwach still
  does not light, the report will show why.
- Please check: prt_fild08, Clock → "Deep night 3:00" — are the beams visible, do they get in the way; turn the
  camera. Cast Ruwach — does the ground around it light up. Send a report.

# uaRO Graphics 2.7.19 — to check

- **The moon is the night's main light.** Under it the map is bright, silver-blue and muted: plainly night, yet the
  road, the grass and the monsters are easy to see — about 60 % of the day's light ("Deep night" about 53 %) instead
  of the earlier ~30 %.
- **"Light around the hero" removed.**
- **Skills light around them at night** — now small bright ones like Ruwach too.
- Please check: prt_fild08, Clock → "Deep night 3:00" — is it playable? Cast Ruwach and see whether it lights around.

# uaRO Graphics 2.7.18 — to check

- **"Moonlight"** (the Time of day group, on by default, strength 0–200 %). The night is no longer just dark: the
  moon lights the world in silver-blue. "Deep night" is about 40 % instead of 25 %, the whole night about 60 %. None
  by day.
- **"Light around the hero"** (same group, on, strength 0–200 %). At night a warm pool of light lies about your
  character, as from a lantern, lighting the ground and those nearby. It grows with the night.
- **"Fireflies light the ground"** — a part of "Fireflies in the grass at night": a little greenish light on the
  ground under each firefly, pulsing with its glow.
- **Skills light more at night** — up to 1.8 times.
- Please check: prt_fild08, Clock → "Deep night 3:00" — can you see the road and monsters, is there a warm pool
  about your character, do the fireflies light the ground, and cast a skill.

# uaRO Graphics 2.7.17 — to check

- **"Deep night" lighter: 25 %.** At 16 % it was simply black; now it is dark, but the road, the grass and monsters
  nearby can be made out.
- **At night only light shines.** White flowers, pale hair and pale stone no longer glow in the dark like stars —
  fire, lamps, windows and magic stay bright.
- **Puddle reflections in the rain are short and even.** No long column of copies under a character any more: a
  puddle mirrors what stands close by, and the reflection fades with distance.
- **"Sky past the map's edge" and "Land past the map's edge" removed** (with "Clouds", "Sky strength" and "Stars at
  night"). The "Sky and air" group is now "Air".
- Please check: Clock → "Deep night 3:00" on prt_fild08 — better? And the rain: the character's reflection on the
  wet ground.

# uaRO Graphics 2.7.16 — to check

- **Rain on hills without the "squares".** Wet ground and puddles were decided by each ground triangle's own slope:
  on a gentle hill one triangle was wet and the next dry — patches with straight edges, puddle reflections torn. Now
  the water lies level: everything flatter than a wall gets wet alike, and puddles mirror like a still surface.
- **"Deep night 3:00" in the Clock.** A nearly black night whatever the "Night strength" (but 0 %): the world goes
  down to 16 % of its light, while torches, lamps, windows and fireflies shine out of the dark.
- Please check: prt_fild08 at night in the rain — is the road even and are the reflections whole? And Clock → "Deep
  night 3:00".

# uaRO Graphics 2.7.15 — to check

- **The card's sparkle on the ground is bigger and brighter.** In 2.7.14 only a thin line of it showed on bright
  paving. Now it is twice the card's width, the rays are thicker, a golden glow sits in the middle, and it stands on
  the card's upper right corner — the star shows and the card under it is not lost.
- Please check: drop a card on the ground (on bright paving and on grass) — is it better?

# uaRO Graphics 2.7.14 — to check

- **"Cards on the ground glint": a proper sparkle over the card.** Instead of a white cross — a sparkle: four long
  rays and four short diagonal ones, each fading to its tip, with a soft glow in the middle, warm white-gold. It is
  drawn after all the sprites, so the card no longer covers it, and "soft particles" no longer put it out.
- Please check: drop a card on the ground — is the sparkle over it, and does it look better?

# uaRO Graphics 2.7.13 — to check

- **"Flash of a hit" is visible.** The monster was found and the white flash over it was added, but the renderer took
  it for an effect with "soft particles": such an effect fades out near what is behind it, and the monster stands on
  the ground — the flash went out almost wholly. Now it is drawn in full.
- Please check: "Flash of a hit" and the game's sound on — hit a monster a few times. Does it flash white?

# uaRO Graphics 2.7.12 — to check

- **"Flash of a hit": the monster is recognised.** The driver remembers which frame lies where in the sprite cache.
  When the game wrote a monster's frame where a cursor's or a player's frame lay before, the old record stayed first
  and the monster was taken for a cursor or a player — the flash found it only now and then. Now a new frame pushes
  out the old ones. "Cards on the ground" (the little star) needs the same recognition.
- Please check: "Flash of a hit" and the game's sound on — hit a monster 5–8 times with a normal attack. Does it
  flash white? If not — a report.

# uaRO Graphics 2.7.11 — to check

- **Diagnostics of "Flash of a hit".** The 2.7.10 report shows the driver hears the hits and finds their place on
  the screen correctly, but sees no monster there. Now, when a hit finds no monster, the driver writes into its log
  everything drawn at that place over the next two frames. The picture does not change.
- Please check: "Flash of a hit" on (ticked), the game's sound on — hit a monster 5–8 times with a normal attack and
  send a report right away.

# uaRO Graphics 2.7.10 — to check

- **"Flash of a hit" on hills.** The game gives a hit's place at the player's height, so on a slope the driver
  found no monster there and nothing flashed. Now the hit goes to the monster standing at that place higher or
  lower. The first 8 hits go into the log.
- **"Gusts of wind" removed.** The gust's band was too faint to see. The grass and the leaves sway as before; "One
  wind for everything" still bends the grass and carries rain, snow and sand one way.
- **"Sun flare" removed.** The camera looks down at the ground and the sky is hardly ever in the picture: the
  flare showed only at a map's edge, while by day the card checked the whole screen every frame for nothing.
- **"Fade in monsters and items" removed.** Monsters, characters and items appear at once again, as in the
  game.
- The log no longer has the ground seams line every 5 seconds — the blinking was found and fixed in 2.7.9.
- Please check: prt_fild08 200 200, "Flash of a hit" and the game's sound on — hit a monster with a normal attack
  a few times, one standing higher or lower on the slope too. Does it flash white? Then a report.

# uaRO Graphics 2.7.9 — to check

- **"Grass and road seams" blinking in the middle of a map — fixed.** The neighbour's strip (magenta in the seams
  view) picks which way to bend the wavy line between grass and road by comparing two cells. On the map's middle row
  that comparison hung on an error of thousandths of a unit that changes as the camera turns — the line jumped back
  and forth. prt_fild 200 200 is exactly the map's middle. The side no longer depends on the camera; elsewhere the
  seams look as before.
- Please check: prt_fild08 200 200, the seams on — turn the camera. Does it still blink? To see it, you can switch on
  "Ground seams view" → "Coloured" (the ~ console): does the magenta blink.

# uaRO Graphics 2.7.8 — to check

- **Blinking on hills with "Grass and road seams" — fixed.** The narrow band at the edge of each cell cut across the
  hill's crease and went under the ground in places (up to a third of a unit). As the camera turned, those places
  hid and came out again — that was the blinking. The band now lies exactly on the ground; flat ground is unchanged.
- **Debugging: "Ground seams view"** (the ~ console, the "AI textures" section, under "X-ray of the AI pictures"; in
  the settings, the Debugging page). "Coloured" paints the seam strips: a cell's own band cyan, the neighbour's strip
  magenta. "Coloured, over everything" is the same without the depth test. Direct3D 12 only.
- Please check: prt_fild08 200 200, the seams on — does it still blink when you turn the camera? If it does, switch
  on "Coloured": does the colour blink, and which colour; then "Coloured, over everything". And a report.

# uaRO Graphics 2.7.7 — to check

- **Diagnostics of "Grass and road seams" blinking on hills.** Every five seconds the driver writes to its log how
  many seam strips it drew and whether the game drew some of the ground after them. The picture does not change.
- Please check: the field by Prontera (prt_fild08 200 200), the seams on, Direct3D 12 - turn the camera over the
  hill for 20-30 seconds so that it blinks, then send a report.

# uaRO Graphics 2.7.6 — to check

- **Fixed again: "Grass and road seams without steps" on hills.** The seams' strips were drawn in an order that
  changed with every turn of the camera, and at the cells' corners, where strips overlap, the colour blinked. Now
  the order is always the same.
- **No flowers in the grass in winter.** While the season's snow lies on the map no flowers bloom in the grass - as
  the butterflies, they come back in spring.
- Please check: the field by Prontera (prt_fild08 200 200) - turn the camera over the hills with the seams on: no
  blinking; Season: Winter (with Snow in winter and Grass by the season on) - no flowers in the grass; Spring or
  Summer - the flowers are back.

# uaRO Graphics 2.7.5 — to check

- **Fixed: "Grass and road seams without steps" no longer blink on hills.** The seams' strips were cut along the
  other diagonal than the ground's cell under them: on a hill a strip hung up to a third of a unit above or under
  the ground and came and went as the camera turned. Now the strips lie right on the ground.
- **No butterflies, dragonflies nor fireflies in winter.** While the season's snow lies on the map (the one chosen or
  the calendar's), no insects fly; the birds stay.
- **Another Season no longer makes the creatures anew** - the butterflies and the birds came again at every switch;
  now only when it is needed (autumn's leaves, winter without insects).
- Please check: the field by Prontera (prt_fild08 200 200) with the seams on - turn the camera over the hills: no
  blinking. Turn on Snow in winter and Grass by the season under Seasons on the map, choose Season: Winter - snow,
  no butterflies, birds there; Summer - the butterflies fly again.

# uaRO Graphics 2.7.4 — to check

- **Fixed: the grass no longer blinks with the camera far back.** Far off the blades grew smaller than a pixel and
  came and went. Now as the camera goes back the grass thins out smoothly and is gone; at the usual distance it is
  as it was.
- Please check: the field by Prontera (prt_fild08 200 200) - take the camera all the way back: the grass thins out
  and goes without blinking; come closer - the grass is thick again.

# uaRO Graphics 2.7.3 — to check

- **Fixed: the grass by the water no longer blinks.** In shallow ponds the grass came under the water and went again -
  the game's waves lift the water and let it fall, and the grass's line jumped with them. Now the line is the
  highest the waves have come: no grass under the water, and it does not blink.
- Please check: the pond on cmd_fild01 (244 187) - stand a minute: no grass on the water, the grass on the bank as
  before, nothing blinks.

# uaRO Graphics 2.7.2 — to check

- **Fixed: no grass in the water.** In shallow ponds and rivers the green bed showed through the water and blades
  were drawn over it. Now no grass grows under the water.
- **"Surf" removed with its "Wet sand".** The wet sand darkened the shore barely, and only warm-coloured sand, and
  the foam "breathed" too slowly - switching it changed nothing seen. The foam along the shore stays: it is a part of
  Modern water ("Foam along the shore").
- **"Fish" removed.** The fish's shapes under the water were too faint: a fifth of a cell long and only a little
  darker than the water, they could not be made out even where it is deep. The switch is gone from the Water group;
  an old `fish` line in uaro-graphics.ini is no longer read.
- Please check: the pond on cmd_fild01 (244 187) - no blades on the water, the grass around as before; no Fish in the
  Water group; the water in Alberta as before - waves, glints, the houses and the ships reflected.

# uaRO Graphics 2.7.1 — to check

- **Fixed (Direct3D 12): "World reflections in water" work.** They were never drawn before: the game starts every
  frame by clearing the screen, and at that moment the driver swapped the houses and ships gathered from the last
  frame for an empty list. For the same reason "Hills without steps of light" did not work on Direct3D 12, and the
  grass grew on bridges and decks too, not only on the ground. All three work now.
- **Fixed: the console ~ no longer types "ё" into the game's chat.** In a Russian or Ukrainian layout every opening
  of the console added "ё" to the chat line.
- Please check: Alberta, the pier's edge (167 162), Clock: Day, the Remaster mode - by the ships and the houses the
  water mirrors them; on the fields' hills the sunny side lighter, without steps; no grass on bridges and decks;
  open and close the console - no "ё" in the chat.

# uaRO Graphics 2.7.0 — to check

- **New: the effects grouped by what the eye is on.** Fourteen groups instead of ten: Day, night and seasons;
  Weather: rain, snow, sand; Characters and monsters; Skills; Fire and smoke; Light; Ground and walls; Grass and
  trees; Water; Creatures; Caves and dungeons; Sky and air; Whole screen; Game fixes. A group's button tells how many
  of its effects are on: "Effects: 3 of 7 on".
- **Clear names.** 30 effects are named by what shows on the screen: "Living characters" is now "Volume from the
  sun", "Rim light on characters" - "Lit edge on the fire's side", "Soft shadows" - "Shadows in corners and at
  walls", "Damp cave walls" - "Water running down the walls". The app's search finds the effects by their old names
  too.
- **The console (~): search and "Only what this map has".** Type letters and the effects with them in the name or
  the hint stay (Backspace erases, Esc clears). The row "Only what this map has" hides what does not work here: in a
  cave the grass, the sky and the weather.
- Your settings stay: an old group that was off keeps its effects off. The app rewrites uaro-graphics.ini once, after
  a backup.
- The switch "Sun spots under the trees" is gone: it drew nothing since 2.1.4.
- **"Season" and "Weather", in the manner of the Clock.** At the top of the effects, under the Clock and the Night
  strength (and in the console ~). Season: Auto - the computer's calendar, or spring, summer, autumn, winter.
  Weather: Auto - the map's own, as before, Clear, or rain, snow, falling leaves, sand in the wind under every open
  sky (none in houses and dungeons). They change at once. The snow-and-rain switch is now called "Precipitation" and
  must be on.
- Please check: in the app's Effects, fourteen groups and "N of M on" on their buttons; in the game ~, type "snow" -
  the snow's effects stay, Esc clears; in a cave turn on "Only what this map has" - the grass, the sky and the weather
  leave the list; on a field by Prontera "Season: Winter" - snow, "Weather: Rain" - rain and puddles.

# uaRO Graphics 2.6.21 — to check

- **"Dust in the light" shows now.** With it the dust used to vanish: by a torch the motes took nearly the colour of
  the yellow wall, and in the dark the effect put them out altogether. Now in the fire's light the motes are lighter
  than what is behind them - warm specks by the torches - and in the dark a faint dust stays, about a third.
- **"Damp cave walls": no blinking dots, the trickles run down.** The glints on the stone looked like dots in the
  air - gone; the trickles of water crawled up the walls - now they run down.
- Please check: in the Pyramids light motes in the warm light by the torches, a faint dust in the dark corners; with
  Tab on the "Dust in the light" row the dust is even, without the fire's colour. In the Prontera sewers the
  trickles on the walls by the torches run down, no dots.

# uaRO Graphics 2.6.20 — to check

- **Fixed the black screen at the login (1 frame a second).** In 2.6.19 the renderer (uaro-render64.exe) went down
  right after the start: the log line about the effects' settings had, since the Depth of field was removed, read
  data that was not there - it got away with it before, not in 2.6.19. The line is fixed, and the build now checks
  every log line itself so that it cannot happen again.
- Please check: the game reaches the login and enters the world as usual. All else as in 2.6.19.

# uaRO Graphics 2.6.19 — to check

- **No more leaves falling in the Pyramids.** Leaves fall only from trees, and the graphics know trees by a green
  picture with gaps. There are no trees in the Pyramids, but green Poporings live there: the page with their frames
  was now and then taken for leaves, so the monsters shed leaves, the wind swayed them and "Living characters" did
  not work on them. Now the pictures of characters, monsters and effects are never taken for leaves.
- **The Clock is the master of the time of day.** It now sits at the top, right under the Quality and the Mode - in
  the settings app and in the console (~), not inside the Air and weather group. What the Clock says all the effects
  show: the sun and its light, the night, the butterflies, birds and fireflies, the dew, the morning fog.
- **A new choice "From the server":** day and night as on the server. When the server's night comes, the sun sets
  through a sunset in about a minute; when it goes, it rises through a dawn. The server tells the night only on the
  maps that have one; elsewhere it is day.
- **The night always goes by the Clock.** The "Night" and "By the clock" boxes are gone, only the "Night strength"
  under the Clock stays (0 % - no night). Note: on Auto the world now darkens from 20:00 to 6:00 by the computer's
  clock even if the Night was off before; "Quality: Off" takes the night away too.
- "Moths at the fire" removed.
- Please check: in the Pyramids near the Poporings no leaves fall; in a field by the trees the leaves still fall.
  Then the Clock and the Night strength sit under the Quality and the Mode; with "From the server" type @night -
  night in a minute, @day - the dawn.

# uaRO Graphics 2.6.18 — to check

- **The torches' fire on the walls no longer blinks every 1-2 seconds.** The real cause was in the game itself: it
  burns every map torch as two effects at once - it starts a new one every 2 seconds and each lasts 4. At each
  restart there are one or three of them for a moment. The graphics lit the wall from each one, so every two seconds
  the light on the wall dipped or flared. Now both effects are one torch: one fire and one light, as bright as
  before. And with many torches in view none of them shows the game's old drop any more.
- "Embers from the fires" over the torches are half as frequent now: each torch used to be counted twice. "Moths at
  the fire" circle the four nearest torches, not two.
- **A fire's shadow no longer goes through the wall in front of it.** The graphics now read the map's shape (its
  .gnd file) and know where its walls stand: a fire a wall hides the character from casts no shadow of them.
- **Blindness covers the whole screen.** The game's darkness is a square round the character, and from a camera far
  off it did not reach the sides of a wide screen. Now it covers the screen whole; "Softer skills" with "Skills'
  brightness" no longer thin it (the walls showed through), and the torches' light and fire and our other effects no
  longer shine through it.
- Please check: in the Pyramids stand by a torch on a wall for half a minute - the light on the wall is steady, no
  dips or flares, and the fire and its light are as bright as in 2.6.17. Then stand by a wall with a torch burning
  behind it - no shadow from it; and under Blindness with the camera far off the screen is dark all over, the
  torches far away do not shine.

# uaRO Graphics 2.6.17 — to check

- **The torches no longer blink every couple of seconds.** The new fire was thrown by the game itself: its flame
  shifts a little through its animation and now and then goes missing for a frame. That changed the torch's "seed",
  and the "Alive" fire jumped to another shape, while in the missing frame the game's blurred drop showed through.
  Now each torch is followed from frame to frame and keeps its look, one missing a moment stays in place, and the
  game's brightness dips are smoothed. A torch's first appearance on entering a map is as before.
- Please check: in the Pyramids stand by a torch and watch it for half a minute, then walk around near it - the fire
  burns steadily, no flashes or jumps of shape.

# uaRO Graphics 2.6.16 — to check

- **The torches' "Flicker" now shows across the whole slider.** From 0 to 100 % the light on the walls went from
  steady to only ±12 %, the flame and its glow to ±17 %, hardly seen under the fire's own motion. Now 50 % is the
  same calm flicker as before (nothing changes by default), 0 % burns evenly, 100 % four times livelier: the light on
  the walls sways ±25 %, the glow ±35 %, the flame dances. The shadows from the fire tremble alike.
- **A new torch fire - "Alive"** (picked from four on the comparison page). Instead of a smooth candle-like drop,
  a wide flame whose top tears into tongues and flamelets breaking off; coloured as heat glows: from dark-red
  edges through orange to a near-white heart. And every torch burns its own way - its own pace, height, lean and
  moment - instead of repeating its neighbours. With "New flame" on.
- Please check: by a torch in the Pyramids set "Flicker" to 0 - the light on the wall is steady; to 100 - the light
  clearly breathes and the flame dances; to 50 - as before. Move the slider slowly, a second or two on each value.
- Please check: in the Pyramids turn "New flame" on - the torches burn with a live, tongued flame; stand where
  you see several torches - they burn differently.

# uaRO Graphics 2.6.15 — to check

- **No more two shadows.** Under the characters and the monsters with their own shadow (from the sun or a fire) the
  game's round spot is gone - under the bats too, where it lay on the floor apart. The spot stays only where none of
  their own falls (underground with no fire near), so nobody floats, and comes back gently at a torch's far edge.
- **"The game's round shadow"** - a new line under "Characters' shadows", off. Turn it on for the spot under
  everyone, as before.
- Please check: in the Pyramids (`@warp moc_pryd01 89 80`) by a torch with the "Shadows and volume" group on, one
  shadow under your character and the bats - the fire's, no round spots; walk far from the torches - the spot comes
  back little by little; turn "The game's round shadow" on - the spots are under everyone again.

# uaRO Graphics 2.6.14 — to check

- **"Characters' shadows" from fires** (the "Shadows and volume" group). Characters, monsters and NPCs cast shadows
  from the two nearest torches and lamps: short right by a fire and longer as you step away; dense near it and fading
  with its light; a torch's shadow trembles with its flame, a lamp's is steady. Between two torches - two shadows.
  Underground the shadows come from fires alone; with no fire near only the game's round one. Under the open sky by
  day the sun's, as before; at night with "Night" on the fires', the sun's fading with the dark. Now in Direct3D 11
  too (it had the sun's alone).
- Please check: turn the "Shadows and volume" group on, in the Pyramids (`@warp moc_pryd01 89 80`) walk up to a
  torch - your character's and the bats' shadows fall away from the fire; step away - the shadow stretches and fades;
  stand between two torches - two shadows. In Prontera by day - the sun's shadow as it was.

# uaRO Graphics 2.6.13 — to check

- **"No server night"** - a new switch in "Picture fixes" (in every set, on by default). The night the server brings
  now and then (half an hour after every two hours of day, in the towns and on the fields) no longer darkens the
  picture nor tints it blue: the time of day is the Clock's and the Night's. Turn it off for the server's night as in
  the game. It works when uaRO.exe is the same build (else the log says "server's night: the game's own").
- Please check: in Prontera wait for the server's night or type `@night` (a GM command: night for every player on
  the night maps), then `@day`. With "No server night" the picture does not darken; hold Tab on that line of the
  console - in a second or two the world's light goes half as dark and bluer, let go - it brightens back.

# uaRO Graphics 2.6.12 — to check

- **Fixed a game crash (uaRO.exe+0xa89f6)** on Enter in the chat box while a map loads: through a portal, a
  teleport, `@warp`, `@follow`. The bug is the 2007 client's own: the count of the chat lines sent is set to zero
  only when the map has loaded, and before that the line was written by whatever number the memory held. ddraw.dll
  now fixes that check in the game when uaRO.exe is the same build (the log says "chat: a line sent while a map
  loads no longer crashes the game").
- **"Moths at the fire" and the embers work without "Live torches".** They took the fires' places from the
  torches' effect only, and with the "Light" group off there were none.
- **The moths show better:** twice the size, circling wider and in front of the flame (half the circle went into
  the wall behind the torch), dusty wings that flash warm as they beat - the pale specks by the fire were taken
  for sparks.
- **A report shows the settings and the log as they are when it is sent.** The app wrote into the report's text
  what it read when it opened: after switches in the console (~) the next reports carried the old settings.
- Please check: type a command that takes you to another map in the chat (`@warp prontera`, say), press Enter and,
  while the map loads, type something more and press Enter again - the game does not crash, the line goes to the chat.
- Please check: "Clock" at "Night 0:00", only the "Living world" group on with "Moths at the fire" and the
  embers, in the Pyramids (`@warp moc_pryd01 89 80`) three moths circle in front of a torch's flame and sparks
  rise over it; the same with the "Light" group on.

# uaRO Graphics 2.6.11 — to check

- **"Clock"** - a new row under the "Air and weather" group (in the console and in the app): "Auto" - the computer's
  clock, as before, or a chosen moment that stays: "Dawn 6:30", "Morning 9:00", "Day 13:00", "Sunset 18:30", "Night
  0:00". All that goes by the hour follows the chosen time: the sun and the light of the day cycle, the night, the
  butterflies and the birds, the dew, the morning fog, the stars, the windows at night. A chosen "Day" is day even
  with "Night" on without "By the clock".
- **Night creatures** (the "Living world" group, off by default): "Fireflies in the grass" - from dusk to dawn lights
  drift low over the grass where the butterflies fly by day, each lit a while and dark a while; "Moths at the fire" -
  at night three pale moths circle each of the nearest torches and fires, lit by the flames.
- **"Volumetric fog" removed** with "Fog density" (a Remaster-mode effect, off by default).
- **"Film-like colours" removed.** The bloom of bright places and the fade into a map work as before.
- **"Depth of field" removed** with "Blur" and "Sharp zone".
- **"Film grain" removed** with "Grain strength".
- Please check: the "Air and weather" group has no "Volumetric fog" and "Picture" no "Film-like colours", "Depth of
  field" nor "Film grain"; the game in Remaster mode starts and draws as before; "Clock" at "Day 13:00" at night by
  the computer's clock - the butterflies fly, at "Night 0:00" with "Night" on - dark, at "Auto" - as the computer's
  clock says; turn "Fireflies in the grass" on and set "Clock" to "Night 0:00" at `@warp prt_fild08 200 200` - lights
  blink over the grass; "Moths at the fire" - in the Pyramids (`@warp moc_pryd01 89 80`) moths circle a torch.

# uaRO Graphics 2.6.10 — to check

- **"No game fog under the open sky"** - a new switch among the "Fixes of the picture", on by default. On the fields
  and in the towns the game's own fog is no longer drawn: the light green veil over Prontera's fields (prt_fild08's
  colour in the game's data is 0xBAFF77) and the white one in Prontera, which thickened as the camera went back. In
  the dungeons the game's dark fog stays - it hides the void past the map's edge.
- Please check: turn the "Fixes of the picture" group on, at `@warp prt_fild08 200 200` move the camera back - no
  green veil; turn "No game fog under the open sky" off - it is back. Far back, the void past the map's edge may show
  at the border - "Sky past the map's edge" of the "Air and weather" group covers it.

# uaRO Graphics 2.6.9 — to check

- **"Haze and fog" removed** with "Haze's strength" and "Low fog in dungeons": no more bluish haze far off under the
  open sky, no dark far off and low fog in the dungeons. The sky's horizon past the map's edge is now a pale blue, not
  the haze's colour.
- **"Morning fog" and the swamps' mist stay** and now work on their own, by their own switches ("Morning fog",
  "Swamps"). Until now they were drawn only with the haze on.
- Please check: the "Air and weather" group has no "Haze and fog"; in the dungeons (e.g. `@warp pay_dun00 21 183`) no
  fog lies over the floor; with "Morning fog" on, in the morning by the computer's clock a fog lies over the fields (the
  log: `mists: the morning's over <map>`).

# uaRO Graphics 2.6.8 — to check

- **"Ground relief" defaults to 30 %** (was 100 %): at 100 % the stones' tops turned gritty, at 30 % the stones bulge
  softly. Where the strength was already set, it stays as it was.
- **Grass 3-5 times cheaper** (estimated). On an HD 6570 it cost 22 ms a frame and "Keep the frame rate" turned it off
  ten seconds after it came. The steps that tread the grass down are now worked out once a frame in a small grid
  instead of at every pixel; on Direct3D 11 the blades' roots are looked for on every second row - the blades as long
  as before.
- **"Light from the sky" and "Reflected light" removed.** RO's camera looks from above and nearly everything on screen
  faces up: "Light from the sky" only made the ground and the roofs colder (Prontera's paving: red -4 %, blue +6 %),
  and "Reflected light" only lightened the corners of the wall's battlements a little - for 1.6 ms of the graphics
  card.
- **"Clouds' shadows" removed** with "Clouds' darkness". The clouds in the sky past the map's edge stay.
- **"Motes in the air" default to 200 %** (was 100 %): twice as many pollen flecks - at 100 % they were all but unseen
  over Prontera's white paving. It costs next to nothing: 0.09 ms a frame on an RTX 5060 Ti. Where the amount was
  already set, it stays as it was.
- **Console (~): an effect inside a group that is off is now grey**, marked "its group is off". It used to show green
  although nothing was drawn: a group that is off turns off everything in it.
- Please check: on a weak card the grass stays on the field (no `keep_fps: … effects.grass off` in the log) and is
  still trodden down under the feet; on a fresh install the strength of "Ground relief" is 30 % and "How many motes"
  200 %; the "Light" group has neither "Light from the sky" nor "Reflected light", and "Air and weather" has no
  "Clouds' shadows"; in the console turn the "Light" group off and open it - the effects on inside it are grey with
  the mark; turn the group on - green again.

# uaRO Graphics 2.6.7 — to check

- **"Ground relief" works on its own.** It showed only with "World materials", "Smooth hills" or "Smooth seams of the
  ground" on: without them the shader did not know where the camera was and put the relief out entirely. 2.6.6 named the
  wrong cause - the step across the texture had nothing to do with it.
- Please check: in the console (`~`) keep only "Ground relief" on (materials, hills and seams off), walk up to
  Prontera's paving and hold Tab on its line. The stones should bulge, lit from one side.

# uaRO Graphics 2.6.6 — to check

- **"World materials" default to 50 %** (was 100 %): the relief of stone and brick is calmer. Where the strength was
  already set, it stays as it was.
- **"Smooth light on models" removed.** It made no visible difference on RO's models: by Prontera's fountain and on a
  rock in the Louyang field the picture is the same with it and without. The game lights the models smoothly anyway, and
  low rocks have edges too steep to round. Its line is gone from the settings and the console; an old value in the ini
  changes nothing.
- **"Ground relief" shows again.** It measured a stone's height one texel apart, and uaRO's ground textures are 4 times
  the originals: several texels merge into one screen pixel, and there was no step. The step is at least a screen pixel
  now - the paving's cracks and stones are lit from one side.
- Please check: on a fresh install the strength of "World materials" is 50 %. Yours stays what you set in the console.
  "Ground relief": on Prontera's paving there is a difference with Tab and without - the stones bulge, lit from one
  side.

# uaRO Graphics 2.6.5 — to check

- **"Living characters" reworked, softly.** Instead of a narrow rim along the silhouette (not seen at 60 %, a white
  outline at 200 %) the whole figure is lit now: the side of the body towards the sun a little lighter and warmer, the
  far half in a soft shade. Nothing white is added on top. Default strength 100 %.
- Please check: in the console (`~`) set the strength of "Living characters" to 100 % (200 % is left from the last
  check - strong now), stand in the sun and hold Tab on the "Living characters" line. The character should look more
  solid: the side towards the sun (opposite its shadow) lighter, the far side darker, no white rim.

# uaRO Graphics 2.6.4 — to check

- **"Living characters" with the network's textures:** the band of light along the silhouette's edge is as wide as
  designed now - 2.3 times wider. With the network's pictures (4 times larger) the edge was measured in their small
  texels, and the light was barely seen.
- Please check: in the console (`~`) turn "Living characters" on, stand in the sun and hold Tab on its line. On the
  character's edge towards the sun (opposite its shadow) a light warm band, the far edge a little darker. Turning the
  character does not change it: the edge facing the sun is lit.

# uaRO Graphics 2.6.3 — to check

- **A character's shadow no longer blinks while walking.** The game draws each part of a character (body, head, hat,
  weapon) a little nearer the camera than the one before. The shadow knew the parts by their depth and took the first
  two only; when a turn drew the weapon before the body, the head stopped passing and its shadow was gone for those
  frames. Now the shadow knows the parts by the character itself.
- So the shadow is fuller: hats, weapons, shields and capes are in it - the whole silhouette.
- At a game resolution of 1440p and above the sprite-shaped shadows could be missing altogether (only the round spot) -
  they show now.
- **At the edge of a walkway, a wall, steps and by posts the shadow is no longer cut.** The shadow found the ground by
  one point under the feet; at an edge that point fell on the wall below, and the shadow falling that way was gone -
  walking along the edge it came and went (as in Aldebaran by the balustrade).
- **No more flickering where a model lies in the ground's plane** - like the house doorway in Aldebaran (dark paving one
  moment, light planks the next as the camera turns). The game counts on a 16-bit depth buffer, where the ground, drawn
  after the model, always wins in such a plane; the card's buffer is finer, and either side won. Models now lie one
  16-bit step deeper, as they did in the game.
- Please check: "Characters' shadows" on, walk around Prontera in different directions with turns, and along the
  balustrade in Aldebaran - the shadow is steady, never gone; a character's hat and weapon show in it. In Aldebaran by
  the same house turn the camera - the doorway does not flicker.

# uaRO Graphics 2.6.2 — to check

- **The Ctrl+2 flag is now the Right Sector's own:** red over black with the white «ПРАВИЙ СЕКТОР» and the trident and
  sword across the middle. The flag is small - a letter is about two pixels - so the lettering shows as a white band.
  In 2.6.1 it was divided diagonally by mistake.
- A curled edge of the cloth (its back showing) takes its own half's colour on both flags now.
- The same file, `uaro-flags.grf`: listed in DATA.INI already, nothing to do.
- Please check: Ctrl+2 in the game - a red-black flag with a white band of lettering across the middle.

# uaRO Graphics 2.6.1 — to check

- **The emotions' flags: Ctrl+1 is Ukraine's flag, Ctrl+2 the Right Sector's** (red at the pole's top, black below,
  divided diagonally). They come as an archive of their own, `uaro-flags.grf`, in the game's folder next to
  uaRO.exe. The game reads it once DATA.INI lists it above the client's GRF; the update leaves DATA.INI alone.
- The flags wave as the old ones did: the same 27 frames, folds and shades; turned away they show their back in the
  flag's colours too. The other emotions are untouched. With the DATA.INI line, Make on the Texture pack tab improves
  them too.
- Please check: the DATA.INI line → the game → Ctrl+1 and Ctrl+2 wave the Ukrainian and the red-black flags; another
  player with the updated client sees them over your character; the other emotions (Alt+L, /!, /?) as before.

# uaRO Graphics 2.6.0 — to check

- **The settings app in three modes** — a switch at the top: Simple, Detailed, Developer. Simple: the Main page (the
  quality in one choice, DLSS, frame generation, the frame limit and VSync, HDR, the sound, the gamepad, the
  battery, the driver's and the card's state, the HD textures), Effects (the set and ten groups) and Gamepad.
  Detailed: every setting on eight pages instead of fifteen — Graphics (the former Renderer, Quality, Smoothing and
  Frames), Screen, Effects, AI textures (tabs: what to improve, models by group, the texture pack, ready textures),
  Sound, Gamepad, Game and chat, Service (with the diagnostics and the report). Developer: the Debug page and the
  test rows too. Nothing is gone: hidden rows keep their values in the ini.
- A new install opens in the Simple mode, an earlier one in the Detailed mode; the chosen mode is kept.
- **The window grows and maximizes** and keeps its size and place (880×600 at the least). On a wide window
  Graphics, Effects, the AI models and Main go in two columns.
- **Search** — the field at the top (Ctrl+F): the rows found change right in the results, the button under each
  opens its page; Enter opens the first one's page, Esc goes back.
- **A blue dot** before a row: its value is not the one recommended for this computer (the dot's hint says which).
  "Reset the page to the recommended values" is at the top of the pages of settings rows; Apply writes it.
- **The status bar** at the bottom: the driver's version and its update, the texture pack, the game. **Report a
  problem** at the top opens the report to the developers.
- **What's new:** after an update the app shows these notes once; the button is on the Main page.
- **The Debug page** (Developer mode): the log, the profiler, X-ray, DLSS's signs, how the 64-bit renderer's picture
  reaches the window, shader variants, the state cache, merged draws, the game's fast paths (some of them were in the
  ini alone before), uaRO Studio, the game's keys.
- **Compact the texture pack.** Pictures made again or taken out stay inside uaro-textures.zip: the game does not
  read them, but they take room. Compact (Developer mode, the Texture pack tab) copies the pack to a new file
  without them. Close the game first; it needs about the pack's size of free disk.
- **The pack's download from the site is gone** — the pack is made on your own computer only.
- **38 model groups instead of 8** (the Models by group tab): the ground, the models and buildings by the map's
  kind, trees, plants, water, the character's parts, monsters, bosses and MVPs, items, effects, the interface. After
  the update every new group takes its former group's model; the pack's pictures learn their groups at the next Make.
- Please check:
  1. The app opens in the Detailed mode; switch Simple → Developer → Detailed — the list on the left changes, the
     Developer one ends with a grey "Developer only" heading and Debugging.
  2. Stretch the window and maximize it — the page is made again, Graphics and Effects in two columns. Close and
     open the app — the same size and place.
  3. Ctrl+F, "light" — rows marked "Effects › group"; switch one right there; Esc goes back.
  4. Blue dots on Graphics; "Reset the page to the recommended values" → the question → the dots gone → Apply.
  5. Simple mode, Main: Quality → High (Graphics then holds the Maximum preset), Recommended settings, What's new.
  6. Report a problem → the Service page, the caret in the comment field.
  7. AI textures → Texture pack → Make (no new pictures, only their groups written) → Models by group — the sections
     and the pictures in each group.

# uaRO Graphics 2.5.27 — what to check

- **The Game AI 2.0 model for AI textures.** It can be chosen for any group of pictures on the AI textures page, for
  "One map's textures" on the Texture pack page and in uaRO Studio. A model for game textures (real and drawn), not
  for small icons; about 3 times slower than anime 6B. By Tal, under the WTFPL.
- The model is large, so it comes apart from the package: when it is chosen and its file is not there, the AI
  textures page shows "Download Game AI (29 MB)", which starts get-models.bat. It is kept in 16 bits: 33 MB instead
  of 67, the pictures within 2 levels of 255.
- The models' preview on the AI textures page shows Game AI too (once it is downloaded).
- Please check: choose Game AI for one group -> "Download Game AI" -> a console downloads the model -> the warning on
  the page goes. Then the preview and "One map's textures" with Game AI - how it looks in the game.

# uaRO Graphics 2.5.26 — what to check

- **NVIDIA Reflex without frame generation too.** With DLSS on an NVIDIA card the game no longer draws frames
  ahead: each frame waits until the graphics card is about to be free, and only then does the game read the mouse.
  The cursor should follow the mouse noticeably closer, with nearly as many frames a second. A new box "NVIDIA
  Reflex" on the Frames page (on); it applies from the next start of the game.
- With Reflex the frame limit (`fps_limit`) waits before the game starts a frame, not before a finished one is shown.
- **The log of how the picture is shown.** The `presentation:` line now says whether the game's window covers its
  monitor pixel for pixel: only then can Windows show the picture straight, without composing it with the desktop. In
  2.5.25 the line told nothing.
- Please check: the same settings as last time (DLSS, no frame generation) - the cursor, and the frames a second on
  the counter (74-78 before). To compare, turn "NVIDIA Reflex" off and start the game again. Then send a report.

# uaRO Graphics 2.5.25 — what to check

- **DLSS frame generation responds faster.** The game no longer draws frames ahead while the 64-bit renderer waits
  for the graphics card: it starts its next frame only once its own is shown. The frames drawn used to wait in a
  queue, and the cursor lagged the mouse by some 100 ms (4x at 4K); now by about half of that.
- **No more frames on the screen than the monitor shows.** A 144 Hz monitor got 156 a second; it dropped the extra
  ones unevenly, and the card spent on them the time the game's own frames needed. Now the game's frames are at most
  the monitor's refresh divided by the factor: at 144 Hz up to 69 a second with 2x, up to 34 with 4x.
- The cursor and the game's windows are drawn by the game itself, and frame generation does not make them faster:
  they move with the game's own frames. 2x suits a 144 Hz monitor best.
- The log now tells how Windows shows the picture: composed with the desktop or straight to the monitor.
- Please check: frame generation 2x (and 4x to compare) - the cursor and the camera; the counter shows e.g. "55 fps
  (110 with frame generation)". Then send a report.

# uaRO Graphics 2.5.24 — what to check

- **Palettes.grf with every hair colour.** The Service page's build takes every hair colour its source has (128 in
  Kamishi's pack), not only 0-20. The server clamps hair at 8 now, but should the limit go up, the new colours show in
  the game at once, without building again. It adds about 70 KB: 4.5 MB for 4.4. The texture pack makes heads in
  colours 0-20, the driver colours the rest itself.
- **The frame counter with DLSS frame generation** shows the frames on the screen too: "42 fps (166 with frame
  generation)". It counted the game's frames alone, and frame generation looked off.
- Please check: Service -> "Game palettes" -> make it again; about 23,600 palettes in the end. Put the new
  Palettes.grf in place (the game closed) - the game looks as before.

# uaRO Graphics 2.5.23 — what to check

- **Dyed clothes and hair with AI.** A character in dyed clothes was drawn without AI: the pack makes the bodies in
  their own colours only, and the bodies in all 78 colours would take about 7.5 GB more. Now the driver takes the
  pack's picture in the own colours and colours it as the dye: the network's shading and lines stay, the colour is the
  dye's. The same for hair of any colour; heads in colours 0-20 still come ready from the pack. The pack does not grow.
  On by default: the AI page -> "Dyed clothes and hair".
- Please check: a character in dyed clothes (at the stylist or among the players) - the clothes as sharp as undyed
  ones, the colour right. Untick the box - such a character looks as in the game. The log has a "sprite frames in a
  dye" line. Send a report.

# uaRO Graphics 2.5.22 — what to check

- **Super-light mode and "Save the laptop's battery" lighter on the graphics card.** The world drawn at half (75 %) the
  size was made larger with a sharpening of its own - 45 taps for every pixel; on a 4K screen that cost ten times the
  world itself. Now it is made larger by plain smoothing: the world a little softer, the card's work several times less.
- Please check: super-light on a map, a minute of play, a report - "GPU: … ms of work a frame" should be well under
  the 2.2 ms of the last one.

# uaRO Graphics 2.5.21 — what to check

- **Super-light mode on Direct3D 11: a black or frozen world.** On the maps of a known region (Prontera, Louyang and
  others) the world was black, and after Scroll Lock the last frame without the mode stayed frozen on it. The cause:
  the night and region colours pass ran with "Colours of each region" off too, and the mode's 16-bit world cannot be
  copied into its 32-bit copy. Now the pass runs only when it has something to change, and the frame's copies always
  take the frame's own format. Direct3D 11 also no longer makes a needless full-screen pass on region maps with their
  colours off.
- Please check: turn super-light on and go to Prontera - the world is there and moves; Scroll Lock there and back - the
  picture does not freeze. Send a report.

# uaRO Graphics 2.5.20 — what to check

- **Super-light mode** (Quality → the first box). For the weakest graphics cards, over the settings while it is on: no
  effect and no AI picture, the world at half the picture's size, no anisotropy, sharpening or smoothing. With
  Direct3D 11 also the ground's and the models' textures as their colour alone (a clay look in the map's colours), a
  16-bit world and mip levels without blending. The interface and the characters as they are; Scroll Lock shows the game
  as it was. Turned off, everything comes back.
- **Save the laptop's battery** (Frames). While a laptop runs on its battery: frames at most as set (30), the effects at
  most the weak set's, the world at 75 % (Direct3D 11), a minimised game nearly idle. On the mains, everything as
  before.
- **A quicker game thread.** The game's textures made 16-bit several times faster (map loads), rounding without
  switching the processor's mode, the effects' tags without memory made every frame, less work for crisp text's windows
  and for the frame sent on.
- Please check: turn super-light on in a map and compare the frames (the counter, Ctrl+Shift+F11); off - the picture as
  it was. On a laptop: turn "Save the laptop's battery" on, pull the charger - the log says "battery saver: on the
  battery". Maps load no worse, textures look as before. Send a report.

# uaRO Graphics 2.5.19 — what to check

- **The game's palettes in the settings app** (Service → "Game palettes (Palettes.grf)"). Makes Palettes.grf with only
  the palettes the server lets be worn: hair styles 0-23, hair colours 0-20, clothes 0-77. Costumes and the Doram race
  are unknown to the 2007 client and stay out. The same palette is kept once: Kamishi's pack (219 thousand palettes,
  153 MB) becomes about 18,500 palettes in 4-5 MB in a few seconds. The source: a GRF, a ZIP or a folder.
- **Texture pack: heads in every hair colour 0-20** (0-8 before). Press "Make the pack": the heads in colours 9-20 are
  added, nothing made is made again. uaRO Studio makes its catalogue again once.
- Please check: make Palettes.grf of your whole one (save it under another name), put it in place of the old one (the
  game closed) and look at characters with hair colour 20 and clothes colour 77 - their own colours, not the default.
  Send a report.

# uaRO Graphics 2.5.18 — what to check

- **Skills' effects from the texture pack.** The big effects of skills (the .str animations: lightning, wings, rocks,
  fire, the "Lv uP!!" text) load their textures with black as transparent, and the driver did not find their pictures
  in the pack. Now it does; the pack need not be made again.
- **Full-colour textures.** The game makes every texture 16-bit: glows, fire and the skills' particles had steps, the
  effects' coloured frames only 16 shades of transparency. Now the textures are drawn in their pictures' own colours
  (Quality → "Full-colour textures", on). The texture pack finds its pictures as before.
- **The pack builder** also makes the effects' TGA textures as the .str animations take them. For them, press "Make
  the pack" on the Texture pack page: only the new pictures are added.
- Please check: skills with big effects and a level up - their pictures sharp, from the pack; glows and fire without
  steps. To compare: turn "Full-colour textures" off and change the map (it applies to the textures loaded after the
  change). Send a report.

# uaRO Graphics 2.5.17 — testing

- **Floor reflections on smooth floors only.** The floor used to reflect on every map under a roof, and in caves (Ant
  Hell, for one) smeared copies trailed under the loot and the character. Now castles, churches, palaces, Glast Heim's
  halls, towers, labs, temples, ice and factories reflect; houses half as much; caves, mines, sewers and pyramids not
  at all. In the rain outdoors as before.
- **The light of fire and torches without rings.** A wide soft circle of light (a skill's fire round the character) no
  longer splits into visible ring-shaped steps.
- Check, please: in Ant Hell (anthell01/02) no streaks under the loot and the character; in Prontera's church or a
  castle the floor still reflects; the circle of light from Magnum Break is smooth. Send a report.

# uaRO Graphics 2.5.16 — testing

- **Direct3D 11: a shader for each state.** One shader for every state used to decide by branches, at each pixel, how
  to blend the textures, test the transparency and add the fog; old Radeons (an HD 6570) pay for each such branch. Now
  each state the game uses gets a shader of its own without them (a sprite's is 25 times shorter). They are made in
  the background as the game is played and kept: in the first minutes after the update the processor works more,
  from the next start they are there at once. The same picture.
- **World resolution (Direct3D 11).** A new slider on the Quality page when the game draws with Direct3D 11: the world
  (the map, the characters, the effects) is drawn at a lower resolution and made larger with sharpening, while the
  interface and the text stay sharp. 75 % is about half the pixels. It changes in a running game. "Set up
  automatically" gives old cards 75 %.
- **keep_fps** now also turns off what the world itself draws: the materials, the relief, the light on the
  characters, the seams, the soft particles.
- **The statistics** (if sending them is on) know the processor and the memory.
- Check, please, on the HD 6570: 1) Scroll Lock (everything off) - the FPS and "GPU … ms" against the 35 frames of
  before; 2) your settings with the "World resolution" at 100, 75 and 50 % - the FPS and how it looks; 3) the log's
  lines "draw shaders made for the game's own states" and "the world drawn at". To compare, the shaders can be turned
  off: `[debug] shader_variants = 0`. Send a report.

# uaRO Graphics 2.5.15 — testing

- **The report tells the whole computer now, not only its graphics card:** the processor (its name, cores, threads,
  clock), the memory (in all and free), the power (mains or battery, the power plan). The driver writes the same into
  its log at the start.
- **What holds a frame back.** Every 30 seconds the log tells how much of a core the game's thread and Direct3D 11's
  thread take (their work, not their waits), and Direct3D 11 times the game's world and its interface on the card
  apart ("GPU: the game's world … its interface …").
- Check, please, on the HD 6570: play a couple of minutes in a field and in a town, then compare by the frame counter
  (Ctrl+Shift+F11), each for about 20 seconds standing still: 1) as it is; 2) Scroll Lock - every improvement off; 3)
  Scroll Lock again and the "Low" quality of effects in the console (~). Write down the FPS and "GPU … ms" of each and
  send a report.

# uaRO Graphics 2.5.14 — testing

- **Direct3D 11: a frame no longer waits for the monitor.** With two buffers a graphics card slower than the monitor
  waited for its refresh each frame (20 ms of work, say, became 33 - 30 frames instead of 50). Now there are three,
  and the "GPU … ms" counter leaves that wait out - on weak cards (HD 6570) it shows the card's real work.
- **keep_fps turns "Light from the sky" off.** On an HD 6570 this pass cost up to 29 ms of a frame, and keep_fps
  turned off everything but it. Now it turns it off too (Direct3D 11 and 12).
- **Less work for the game's thread.** The textures' mip levels the game builds by stretching are worked out several
  times faster (the same picture, bit for bit); the 32-bit part does its fractions with SSE2.
- Check, please, on the HD 6570 with the same set of effects: higher FPS than before; the log's "GPU … ms of work"
  below the frame's time; the same picture. On the main computer: all as before, no blinking or stalls; screenshots
  and leaving the game work.


# uaRO Graphics 2.5.13 — testing

- **A white screen with "Direct3D 11 on a thread of its own".** In 2.5.12 the drawing thread made none of the game's
  textures (the game's side numbers them, and Direct3D 11 took no texture under a given number) - everything was
  drawn white. Now they are made under those numbers, as in the 64-bit renderer.
- Check, please, with Direct3D 11 and the "Direct3D 11 on a thread of its own" box ticked (Render page): the same
  picture as without it; in a field and in a town the FPS no lower than before; no blinking or stalls; screenshots
  and leaving the game work. Send a report after a couple of minutes in a field - the log's "waiting for Direct3D
  11's thread" line shows how long the game's thread waits for the drawing.

# uaRO Graphics 2.5.12 — testing

- **Direct3D 11 on a thread of its own (a test, off by default).** A new box on the Render page: "Direct3D 11 on a
  thread of its own". With it the game's thread only writes each frame down and goes on at once, and a thread of its
  own draws it on another core of the processor - as the 64-bit renderer does for Direct3D 12. Where the processor
  holds the game back (an older computer, many effects) there should be more frames. If the thread does not start,
  the game draws as before.
- Check, please, with Direct3D 11: tick the box, restart the game. In a field and in a town the FPS no lower than
  before, the same picture, no blinking or stalls; screenshots and leaving the game work. Send a report - the log's
  "waiting for Direct3D 11's thread" line shows how long the game's thread waits for the drawing.

# uaRO Graphics 2.5.11 — testing

- **Less work on the game's own thread (Direct3D 11 above all).** From the profile of your Direct3D 11 report:
  - the textures and sprite frames the game writes are converted 3-15 times faster (they were 17-20% of its thread);
  - the ground's seams are built 2.3 times faster, and not built again at all while you stand still (were ~20%);
  - the set of effects is no longer made anew at every draw (was ~2%).
  The picture is the same: the results match bit for bit, the tests check it.
- Check, please: with Direct3D 11 in the same field and the same settings - the FPS above 2.5.10's; send a report (the
  profile's share of DDRAW.dll should drop clearly).

# uaRO Graphics 2.5.10 — urgent fix

- **Direct3D 12: a black screen at the first light.** Since 2.5.0 the torches' light buffer took the final picture's
  place. As soon as a light came into the frame - a town's lamps, a dungeon's torches, a skill's light (a priest's cast,
  say), a portal - the screen went black (the screenshots too). After Scroll Lock the picture came back, but a large
  bright rectangle showed about the skills. Fixed; that place is now guarded against such mistakes too.
- Check, please: skills cast next to you (Heal, Increase AGI, Blessing), a town with lamps (prt_in) and a portal - the
  screen does not go dark, no bright rectangle; Scroll Lock there and back breaks nothing.

# uaRO Graphics 2.5.9 — testing

- **Direct3D 11 draws every effect of Direct3D 12.** The last four of the High set are added: the smooth hills, the
  ground's seams (the edge between grass and road wanders instead of following the cells' steps), the world reflected
  in the water and the materials' relief. With Direct3D 11 the materials are lit by the map's sun, as with Direct3D
  12, and the windows light up at night.
- **Direct3D 11: the water.** The foam along the shore, the sun's glints and the light on the bed never came on until
  now - they do. Water in a frame with the glow did not see what lay under it and came out dark - fixed.
- **Direct3D 11: Keep the frame rate (keep_fps)** now sees the effects' costs and turns off the dearest ones, not
  only the characters' shadows, the lamps and the colour.
- The app's texts no longer say Direct3D 11 has no effects.
- Check, please, with Direct3D 11 (the Render page) and the High effects: the hills in a field (prt_fild08), the seams
  between grass and road, houses reflected in the water (Alberta, Izlude), the foam and glints on the sea.

# uaRO Graphics 2.5.8 — urgent fix

- **The game crashed at start with Direct3D 12 (versions 2.5.0-2.5.7).** Turning on the lost-device diagnostics
  (DRED) was built wrong: instead of calling the system it jumped to address 0. It crashed on Windows 10 and 11 at
  every start with Direct3D 12, DLSS too. Fixed; a check of the build now catches such a mistake.
- Check, please: the game with Direct3D 12 starts and reaches the server's login.

# uaRO Graphics 2.5.7 — testing

- **The light in the air costs a quarter.** The rays from the sky (by day under the open sky) and the glow of the air
  about the fires are now worked out at half the size and laid over the picture smoothly - as the floors' reflections
  are. It was the dearest pass by day: each pixel of the rays read the depth 24 times. On Direct3D 12 and Direct3D 11.
- **Direct3D 11: the floors' reflections show again.** Since 2.5.3 they were worked out but never shown - the pass did
  not write their picture's transparency.
- Check, please: by day in a field (prt_fild08, say) with the effects on - the FPS before and now; the rays from the
  sky look as soft as before. On Direct3D 11 the floors reflect in a dungeon or in the rain.

# uaRO Graphics 2.5.6 — testing

- **The game thread's profile (`[debug] profile = 1`) names the driver's own busy places too.** In the Radeon HD 6570
  report the game's thread spent up to 70 % of its time in `ddraw.dll`, of which the renderer itself was only a part.
  The log now writes a "DDRAW.dll's busiest places - +0x… N%, …" line every 10 seconds, from which the slow functions
  of the driver can be named.
- Check, please: on the Direct3D 11 computer (the profile is on there already) play a couple of minutes - a field, a
  town, a dungeon - and send a report with the app's button.

# uaRO Graphics 2.5.5 — testing

- **Direct3D 11: the strong set's effects and those by hand** (turned on on the Effects page or in the `~` console):
  - the shared wind: the weather, the clouds, the sky, the grass, the sand and the leaves move with one wind;
  - the light from the sky and from around (what faces up takes the sky's colour, what faces down the ground's, the
    colours around light the surfaces);
  - the light in the air about the lights and the rays from the sky; the sun's flare;
  - the characters lit by the sun from their silhouette's sides;
  - the depth of field focused on the character;
  - smooth light on the models (stones and bark without facets);
  - the volumetric fog over the fields (in the remaster's mode).
- Check, please, on Direct3D 11 with the Strong set or by hand: uaro-graphics.log has "light from the sky: …
  (Direct3D 11)", "depth of field: … (Direct3D 11)", "smooth models: … (Direct3D 11)"; on a weak card the FPS will
  likely drop hard with them - expected, that is what the strong set is for.

# uaRO Graphics 2.5.4 — testing

- **Direct3D 11: the grass and the living ground.** The grass with its flowers, dew in the morning, gusts of wind,
  bent under the steps - on the map's ground only (not on the bridges and decks). The living ground: sand drifting
  over the desert, the snow glittering, a damp cave's walls glistening, the wet sand along the shore, the season's
  snow and grass, the ground wet in the rain.
- **Direct3D 11: the living world** - butterflies, dragonflies over the water, birds, leaves falling from the crowns,
  sparks from the fires, breath in the frost, splashes about the feet in the water, drips in the caves, the light
  from above under the sea.
- **Fixed (Direct3D 12):** the living world's sparks never rose from the torches - the torches' pass took the list
  of flames before the creatures read it.
- Check, please: on Direct3D 11 the grass on Prontera's fields (the Nature group), butterflies and birds by day (the
  Life group), leaves falling from the trees, sparks rising from the torches (on Direct3D 12 too), the snow
  glittering in Lutie. uaro-graphics.log: "grass: … (Direct3D 11)", "living ground: … (Direct3D 11)", "living
  creatures: … (Direct3D 11)".

# uaRO Graphics 2.5.3 — testing

- **Direct3D 11: the portals** - the last effect of the weak set: a soft blue light and the air turning about them.
- **Direct3D 11: more of the medium set** (to be turned on by hand on the Effects page or in the `~` console):
  - the hot air shimmering over the flames, the burning skills and the lava, far off in the desert;
  - the forest's beams of light by their own shader, with dust and light where they fall;
  - the morning fog over the fields and the towns and the swamps' mist (in the haze);
  - sparks about the fire skills;
  - the floors' reflections indoors and in the rain, the wet ground;
  - the rain's splashes on the ground.
- Check, please, on Direct3D 11: a portal in Prontera (the blue light, the air turning); the heat over the torches
  and in the desert (Sograt); the beams in the forest (Payon); the fog on a field in the morning; the floors'
  reflections in the houses. The log: "heat haze: … (Direct3D 11)", "rays: … (Direct3D 11)", "floor reflections: …
  (Direct3D 11)".

# uaRO Graphics 2.5.2 — testing

- **Direct3D 11 got nearly all of the weak set** (what cards without Direct3D 12 get by default) - by the same
  shaders as Direct3D 12:
  - the ground's smooth lightmap (the switch was there, but it never worked on Direct3D 11);
  - the torches' new flame with its glow in place of the game's glow (with "new flame", of its flame too), the map's
    hidden torches in their own colour;
  - the map's hidden lights - floating orbs, pools of light, clouds of dust - and the fireflies the map's makers
    placed;
  - footprints: prints in the snow and the sand, dust on dry ground; rings on the water where the steps go;
  - the chimneys' smoke and living lava by their own shaders, the lava lighting around it;
  - the new map's fade-in, and the bloom, filmic and film grain (when turned on by hand).
- Of the weak set only the portals are missing on Direct3D 11 now.
- The lean world shader smooths the lightmap itself now - the ground on weak cards stays on it.
- Check, please, on Direct3D 11: the torches in Prontera and the dungeons - the new flame and glow, the lamps' light;
  footprints in the snow (Lutie) and the sand (Morroc); the chimneys' smoke in Payon; lava in Magma; the map coming
  in from black after a teleport. uaro-graphics.log: "torches: N lights, M flames, … (Direct3D 11 …)", "footprints:
  … (Direct3D 11)", "smoke: … (Direct3D 11)", "lava: … (Direct3D 11)". If it got slower, the log's "GPU: effects a
  frame - …" line shows which effect costs.

# uaRO Graphics 2.5.1 — testing

- **Direct3D 11: five more effects** by the same shaders as Direct3D 12: the soft shadows where surfaces meet (the
  Shadows group: a mask at a half or a quarter of the size, blurred, multiplied into the picture), the clouds'
  shadows on the open maps (the Air group) and the light of the lamps the map's makers placed (the Light group: the
  map's file is read as on Direct3D 12; the map's axes are found from the torches seen, so the lamps come a few
  seconds after the first torch in view).
- And two more of the weak set, which Direct3D 11 gets by default: the light of the skills' fire (the brightest
  places light around them, as a torch does; with "skill colours" the other skills in their own colour) and the
  light of the map's hidden torches (the game does not show them; no flame - the light only). Both go in the same
  pass as the torches' light.
- Check, please, on a map without Direct3D 12: uaro-graphics.log has "shadows: … (Direct3D 11)", "clouds: their
  shadows over … (Direct3D 11)", "torches: N lit (Direct3D 11: their light and the map's lamps …)"; shadows under
  the houses and along the walls, the clouds' patches on a field, the lamps' light in Prontera at night.

# uaRO Graphics 2.5.0 — testing

- **Speed, Direct3D 12.** The torches' and lamps' light is not one pass over the lights' whole area looping over
  every light in every pixel any more (a tower's sixty torches cost 3-7 ms at 1080p), but a quad per light into a
  buffer of light and one apply pass: the same arithmetic, but a pixel's work is its own lights' and a pixel no light
  reaches costs nothing. The game's draws are gathered into fewer draw calls, as Direct3D 11 has done since 0.90
  (the log, every 30 s: "draws: N a frame in M calls to the GPU"); `[debug] merge_draws = 0` turns it off.
- **Speed, both renderers.** A lean world shader for the sprites, the effects and the interface - without the
  materials, the relief, the hills, the seams, the leaves and the textures' explicit gradients, which the old cards
  pay for dearly; the ground and the models with materials keep the whole one. The picture does not change.
- **Direct3D 11:** the haze with the low fog, the sky past the map's edge and the weather (snow, rain, leaves, sand;
  pollen and dust) by the same shaders as Direct3D 12. And the game's camera reaches Direct3D 11 now - its modern
  water (0.99) never came on until now.
- **The graphics card lost after a texture's A/B (report R-TT5RVW, 2.3.1).** The AI picture is not freed at an A/B
  (Ctrl+Shift+F7) any more, only not drawn; every deferred release of a resource goes through one place with the
  right slot. Should the card be lost again, the log says exactly what happened: "DRED: page fault at … freed lately:
  … there: …" with the resources' names, or "DRED: no page fault" - a shader's hang or a reset by another program.
- Check, please: uaro-render64.log has "GPU: effects a frame - … lights and torches …" (in a tower or a dungeon with
  torches - less than before) and "draws: N a frame in M calls"; the picture is the same. On Direct3D 11: a field
  shows the haze far off and the sky past the map's edge, a map with rain or snow its weather, the water its waves and
  reflections; uaro-graphics.log has "haze: over …", "sky: over …", "weather: … (Direct3D 11)", "water: … drawn anew
  (Direct3D 11)".

# uaRO Graphics 2.4.2 — testing

- **setup.exe is not needed any more.** As it starts, the settings app makes the game's settings in the registry if
  there are none yet, and with the game closed it takes over everything setup.exe wrote. After that setup.exe changes
  nothing the game sees, and it can go from the client's download.
- No more "As in setup.exe" buttons and "setup.exe has: …" notes in the app, nor "the game uses …, setup.exe has …"
  lines in the diagnostics and the report.
- **DLSS: own size.** Smoothing has a DLSS mode "Own size": the world is drawn at a share of the screen (33-100 %)
  or at a size written by hand (width and/or height; with one of them alone the other follows the screen's
  proportions), and DLSS enlarges it to the screen. The size it makes on this screen is shown under the rows. A size
  DLSS does not take is brought to the nearest one it does; uaro-render64.log says what was chosen ("DLSS: own size
  …").
- **DLSS model:** Auto (as before: K for DLAA, Quality and Balanced, M and L for the fast modes) or K, J, L, M - for
  any mode.
- **Fixed:** the checks and lists on the Frames and Sound (own part) pages changed nothing.
- Check, please: open the app with the game closed and look at Diagnostics - "Game settings in the registry: there;
  this app writes them", no lines about setup.exe; the game starts with the same screen and sound as before. DLSS Own
  size at 75 % and, say, 50 % - the picture and the FPS change, uaro-render64.log has "DLSS: own size …"; DLSS model K
  and L - the difference shows; a check on Frames stays.

# uaRO Graphics 2.4.1 — testing

- **Monsters no longer blink in their animations.** "Fade in monsters and items" took an attack's frame for a
  new monster and faded it in from transparent: a quarter of the monsters move their frame's middle 45 sprite pixels
  or more from one frame to the next (Lunatic, Baphomet, wolves), over 96 pixels of the screen with the camera near.
  A monster is now known within one and a half of its size.
- **The new trees and the anime-style trees are gone** altogether. The game's own trees keep their crowns' volume,
  the light through the leaves, their own shades, autumn crowns, leaves in the wind, gusts, sun spots and falling
  leaves.
- Check, please: with "Fade in monsters and items" on, monsters do not blink in a fight (Lunatics, wolves,
  Baphomet); new monsters coming onto the screen still fade in.

# uaRO Graphics 2.4.0 — testing

- **The specks on the paving are gone.** They were the seasons' autumn "fallen leaves": they lay on everything that
  faces up, Prontera's paving too, a couple of pixels each. "Pebbles on the ground" made the same specks and are gone
  as well; the old `ground_clutter` line in the settings is simply not read.
- **Under DLSS one effect switched off in the console (~) now reaches the picture at once.** The effects newer than
  1.9 (the seasons, the new trees, the sky's light, smooth hills, the shared wind and others) were not passed to the
  64-bit renderer one by one - which is why the specks went only with the whole "Nature" group.
- **The parts of the effects have boxes of their own** under the effect - on the Effects page and in the console
  (~). All on, as before; switch off what you do not like:
  - Modern water: foam along the shore, sun glints on the water, light on the bed, waterfalls;
  - Surf: wet sand; Footprints: dust from steps;
  - Seasons on the map: snow in winter, grass by the season;
  - Motes in the air: pollen outdoors, dust indoors;
  - Weather: rain splashes; Sky past the map's edge: stars at night;
  - Haze and fog: low fog in dungeons; Heat haze: heat over the desert;
  - Light in the air: rays from the sky; Every tree its own: autumn and winter crowns; Butterflies: dragonflies.
- Check, please: no specks on Prontera's paving by day or by night; under DLSS any single box in the console (the
  seasons, say) changes the picture at once; the new boxes switch off just their own part.

# uaRO Graphics 2.3.2 — testing

- **Ctrl+Shift+F8 picks the ground, not the cursor's cell:** the game draws the cell under the cursor
  (`data\texture\grid.tga`) over the ground right there, and the pick took it. The first press now takes what is drawn
  opaque (the ground, a model); pressed again at the same place, the next texture down, round and round.
- Check, please: Ctrl+Shift+F8 on the ground opens the ground's texture in uaRO Studio; again - the cell, again - the
  ground.

# uaRO Graphics 2.3.1 — testing

- **The game starts while uaRO Studio is open:** the studio (and the texture pack's builder) opened the game's
  archives for reading and writing, and the game, which shares its archives for reading alone, could not open them:
  "Unable to load table class_tab.txt". They are now opened for reading alone.
- **Ctrl+Shift+F8 picks the texture under the cursor:** 2.3.0 read the key after the frame's draws had gone by and
  always found nothing there; the next frame is looked through now. The studio also finds a texture the game draws
  smaller (texture detail below the highest).
- Check, please: open uaRO Studio, let it read the catalog, start the game - it starts; Ctrl+Shift+F8 on the ground
  opens its texture in the studio.

# uaRO Graphics 2.3.0 — testing

- **uaRO Studio** (`uaro-graphics\uaro-studio.exe`): a program of its own for the texture pack. It copies itself to
  `%LOCALAPPDATA%\uaroGraphics\studio\` and runs from there, so the driver can be updated while it is open.
  - **Textures:** every texture of the game in a list with a thumbnail pair (the game's / the pack's), its state, the
    model that made its picture, quality flags (smoothed, tone shifted, bands, halos, pink fringe, alpha, seams) and
    the detail kept; the navigator by maps, folders, states, models and what the game drew on a map.
  - **Inspector:** side by side, a wipe (right mouse), tiled 3x3, as the map lays it, the difference; the wheel zooms
    to 16x. The information: the archive and the archives it shadows, the keys, when and by what the picture was
    made, the maps, an atlas's tiles.
  - **Make again** any texture (or a selection) by any model and a chain: deblock, colour key, tone as the
    original's, the original's grain back, sharpen, clean alpha, a second model on the edges, an atlas's tiles one by
    one. Look, then Keep or Discard. Take out of the pack; the game's own always; the material (gloss, relief,
    height) lighter or stronger for one texture.
  - **Pack:** pictures by group, model and kind, pictures no texture names, textures without a picture, the space
    replaced pictures hold, Compact. Pause lets go of the engine, the pack and the game's files.
- **In the game:** Ctrl+Shift+F9 the X-ray (green from the pack, blue made in the game, red no picture, grey the
  game's by a rule); Ctrl+Shift+F8 opens the texture under the cursor in the studio; Ctrl+Shift+F7 turns its picture
  off and on (A/B). What the studio changes shows in the running game at once.
- The pack's pictures carry the time they were written and what they were made from; the builder names its stages
  and every thread's file, and reads the studio's catalog instead of the game's files when it is up to date.
- Check, please: Comodo's beach (cmd_fild02) in the studio — its sand's flags, Make again by general with tone and
  grain, Keep, the game shows it without a restart; Ctrl+Shift+F8 on the sand opens it in the studio; the X-ray.
- **One map's textures on the Texture pack page:** at the page's foot - the map (the last one you played is filled
  in), Its ground and Its models, a model and two buttons. Make again makes the pictures of that map's textures anew
  by the model chosen; Take out of the pack takes them out, and the game shows its own textures there. Close the game
  first: the change shows at its next start. Other maps with the same textures change too.
- **The settings app no longer freezes at Stop and Make:** the pack page no longer reads the whole pack at every click,
  nor every picture of a pack its builder is still writing.
- **A faster build:** only the colours the server lets be worn (hair 0-8, clothes 0-77), not all 128 hair colours and
  up to 700 clothes colours; Stop works at once; while the files are read, the one being read is shown.
- Check, please: Comodo's beach (cmd_fild02) - Its ground, model general, Make again, then the sand in the game; and
  Take out of the pack - the sand becomes the game's texture.

# uaRO Graphics 2.2.4 — testing

- **Sharp pack textures with DLSS:** DLSS draws the world smaller than the screen, and the pack's textures were taken
  from a smaller mip level made for that smaller world: sand, tiles and grass came out softer than the game's own
  textures. The level is now picked for the screen's pixels, as NVIDIA asks. Without DLSS all is as it was.
- **Trees and Objects:** Objects turned off in the console take the pack's pictures off the new trees too.
- Check, please: Comodo's beach with DLSS and Map ground on — the sand no softer than with it off.

# uaRO Graphics 2.2.3 — testing

- **Sand and gentle slopes:** Reflected light no longer draws bright lines along the cells' edges on bright sand
  (Comodo's beach, cmd_fild02). The light of the surfaces around now counts by how much they turn to each other:
  small folds of the ground get next to nothing, a wall's foot as before.
- **Auto by the graphics card:** the driver saw 3072 MB on any newer card (all Windows tells a 32-bit game), took it
  for a weak one and drew the weak set of effects while the settings app said Strong. The driver now takes the card's
  real memory, as the settings app does.
- Check, please: Comodo's beach (cmd_fild02) with all effects; in the console (~) "Auto: …" names your card's class,
  and Custom chosen from Auto no longer turns nearly everything off.

# uaRO Graphics 2.2.2 — testing

- **Glints on tiles and steps:** the relief, the materials and their gloss now fade by one measure — "the pixel sees
  the texture's relief poorly" (the texture stretched along the face, the face seen edge-on, the pixel's texels in one
  line). Each effect had a measure of its own, and a face that passed one glinted by another: only turning the whole
  Surfaces group off helped. Materials brighten at most 1.3 times. Ordinary ground looks as before.
- **Floor reflections at edges:** at the rims of pits, steps and thin details the reflection fades smoothly instead
  of blinking as the camera turns; in the rain such rims darken like the floor around them (no light specks).
- **The console (~):** choosing Custom starts from the set's effects (as the Effects page does), not from what the
  file held; Auto quality names the card's class — "Auto: Medium".
- **AI textures in the console:** a switch lights up at once (2.2.1 missed a kind turned back on); monsters, players,
  NPCs and items turn off one by one and at once — their frames are drawn from the game's own pictures (a single kind
  of sprites used to wait for the next start). A kind the game started without still needs a restart (the ↻ mark).
- **Fog in dungeons:** the low fog lies at the ground's height at the character; walking past walls it used to jump
  up and down, the walls going light and dark by turns (alde_dun03).
- **keep_fps** turns effects off only when the graphics card is what is slow; slow for the processor's part (a crowd,
  a town), the effects stay — turning them off would not help.
- **Statistics:** sent more reliably (nothing is lost while the game writes), know whether the 64-bit renderer draws
  and the frame limit; the Diagnostics page shows when they last went and has "Send the statistics now". A profile
  loaded or Reset no longer loses your answer about the statistics.
- Small things: when the 64-bit renderer does not join, the log says why; dgVoodoo's leftovers are gone.
- Check, please: c_tower2's and gl_church's floors and gaps while turning the camera (with the Surfaces group on);
  floor reflections at steps and in the rain; alde_dun03's walls while walking; the console's Custom, "Auto: …",
  Monsters off and on; Diagnostics → "Send the statistics now".

# uaRO Graphics 2.2.1 — testing

- **The Effects page works:** the quality choice, the groups' boxes and the effects inside them did nothing in 2.2.0
  (the main window passed their clicks on to no page). Now each group has a button "The group's effects ▸" that opens
  it to its effects, each with its own box and settings; the open groups stay open.
- **Effects quality as in any game:** Auto, Off, Low, Medium, High, Custom (Low - built-in graphics and cards under
  4 GB, Medium 4-8 GB, High 8 GB and more). A new group, **Fixes of the picture** (the map's light smooth, effects soft
  at the walls, the fade between maps), is in every quality.
- **The console (~) by groups:** the frame counter, the AI textures, the quality, then the ten groups - each with its
  strength and a row that opens it (Space, Enter, right arrow). About thirty rows instead of a hundred and fifty.
- **AI textures in the game:** the console turns the pack's pictures on and off by kind - ground, buildings,
  characters and items, skills, icons. Off at once; on at once when the game started with them, otherwise at the next
  start (↻).
- **What an effect costs:** the frame counter (Ctrl+Shift+F11, now remembered) shows the graphics card's work a frame
  - "60 fps · 16.7 ms · GPU 7.1 ms (DLSS Quality 2.6)": at the frame limit the frames a second stay the same, the GPU's
  milliseconds do not. The console shows each effect's and group's own milliseconds; hold Tab on one to see the GPU
  time drop. The frame counter's box in the settings applies at once.
- **Bridges are bridges:** 나무 is "wood" as well as "tree" - umbala's bridge, field bridges, boxes, fences, stumps
  and more (64 models) were grown as bushes. Fixed.
- **No grass on decks:** the grass grows only on the map's own ground, not on umbala's green decks and bridges.
- **No cracks on tiled floors:** smooth seams of the ground only on fields and in towns - in dungeons (Glast Heim's
  church, the Clock Tower) they spread the black broken tiles over the stones and blinked.
- **Anonymous statistics (asked once):** with your leave, the settings app sends once a day which effects and
  textures are on, what is turned off after a try, the card and its frames a second - no name, no login. The best
  settings for each kind of card are made from it. Diagnostics page: change your mind any time.
- Check, please: the Effects page (quality, groups, the group's effects); the console (~) and its textures section;
  the frame counter's GPU milliseconds; umbala's bridge and decks; gl_church and c_tower2's floors.

# uaRO Graphics 2.2.0 — testing

- **A simpler Effects page.** At the top, the set of effects for your graphics card: Auto (picked by the card - it
  says which), Off, Weak, Medium, Strong graphics card, or Custom. Under it nine groups - Light, Shadows and depth,
  Surfaces, Nature, Water, Air and weather, Living world, Skills, Picture - each with its own box and a Strength
  slider (100 % as made, less gentler, more stronger; the living world has none). The milliseconds a group cost in
  the last game are shown by its name. Every effect on its own is under **Details**; a change there makes the set
  Custom, starting from what the set had on. The Strong set is the Remaster mode, the others Classic+.
- **Your settings stay:** an existing uaro-graphics.ini has no set yet, so it is Custom - everything as you had it.
  A new installation, Reset and "Set up for this computer" choose the set by the card.
- **The console (~)** has the set and the groups at the top; an effect switched there under a set makes the set
  Custom too. Tab held on a group turns the whole group off while held.
- **dgVoodoo is gone:** the game always draws with uaRO Graphics (if it cannot start - no Direct3D 12 or 11 - with
  Windows' own DirectDraw). The renderer choice, use-dgvoodoo.bat and use-uaro.bat are gone; uninstall.bat still
  puts dgVoodoo back.
- **Lighter driver:** the code of the moving shadows (off since 2.1.4) and the old ways of the trees (the bunches of
  1.10 and the full crowns of 1.9.7 - the new trees replace them) are removed: 6 shaders fewer to compile at the
  game's start.
- Check, please: the Effects page (the set, the groups, Details) - what Auto says for your card; that after the
  update your old boxes are as they were (the set shows Custom); a group off and its strength in a running game; the
  console with a set chosen.

# uaRO Graphics 2.1.7 — testing

- **No white flashes in dark gaps:** the materials' glint is no brighter than the place it is on - it was added as
  white over dark gaps and flashed as the camera turned (c_tower2).
- **Striped step sides:** where the game stretches a texture along a face (a step's side, in stripes) the material
  (relief and glint) is off - it was as garbled there as the picture.

# uaRO Graphics 2.1.6 — testing

- **No blinking at steps:** the seams between textures are no longer drawn over a step of the floor (a little wall
  between two tiles) - the strip at the wall's foot blinked against it (c_tower2).

# uaRO Graphics 2.1.5 — testing

- **No rainbow floors:** the map's lamps light with a third of their colour - the game has their colours in its
  light already; full ones laid orange, green, blue and violet patches over c_tower2's floor.
- **No smears at the tiles' edges:** the materials' relief (Remaster) moves the texture by at most a pixel and a half
  on the screen - farther, it took the next tile's part of the texture (coloured stripes, torn edges).

# uaRO Graphics 2.1.4 — testing

- **No moving shadows:** "Sun shadows" and "Real shadows" are gone - they turned and jumped with the camera and
  argued with the shadows the game has painted under trees and houses, which stay. The two boxes left the Effects
  page and the Recommended button; `sun_shadows` / `real_shadows` in the ini are no longer read (switched off by
  themselves). The crowns' and the characters' shadows from them went with them; the light still comes from the
  map's own sun.

# uaRO Graphics 2.1.3 — testing

- **One sun:** the real shadows (and the materials', the trees', the characters' light) now fall the way of the map's
  own sun - the light its makers set in the map file, by which the game's painted shadows on the ground were drawn
  - so that the two kinds of shadows agree. The day cycle only lifts and lowers the sun (longer shadows morning and
  evening) and colours its light; without the cycle the sun stands as high as the map's.
- Check, please, in Prontera and on prt_fild08 by day: do our shadows (houses, trees, sakura) fall the same way as
  the dark shadows painted on the ground under them? If ours fall exactly the other way (towards the sun), say so -
  one sign to turn.

# uaRO Graphics 2.1.2 — testing

- **Shadows that came out of nowhere:** the real sun's map covered about 150 x 150 units about the player - the far
  part of the screen had no shadows, and those of the houses and trees at its edge came and went as one walked or
  turned. It now covers the ground the screen shows (75-250 units, in steps of 25 so that zooming does not shimmer),
  and the shadows fade out towards its edge instead of being cut off.
- **Blinking where two ground textures meet** (Smooth seams of the ground): the strip across a seam was laid between
  the cell's four corners, the game's ground on its two triangles - on a slope they part by tenths of a unit and the
  ground hid the strip now and then. The strip lies on the game's own triangles now, a little higher.
- Check: walk and turn on prt_fild08 and in Prontera - the shadows lie still, none pop in at the screen's far edge;
  the seams on slopes do not blink.

# uaRO Graphics 2.1.1 — testing

- **Where a frame's time goes, in the log** (to find the drops to ~46 fps): every 30 s
  - "bridge: a frame of the game's thread - X ms its draws written and sent, Y ms waiting for the 64-bit renderer";
  - "CPU: X ms a frame in the driver's drawing (Y of it waiting for the card); Z ms the frame limit's wait, W ms the
    swap chain's present".
- **The new trees cheaper on the processor:** after a forest's first 10 s only the draws whose texture was a tree's
  (or leaves) are read back into the world - before, every model's vertices were, every frame.
- Please: play 2 minutes on prt_fild08 (walk about, some fights), then 2 minutes in Prontera, and send a report.

# uaRO Graphics 2.1.0 — testing

- **The new trees much lighter:** a frame draws only the trees in view (with a margin for their size and the wind),
  not all of the map's - on prt_fild08 some tens of its 491; the far ones (small on the screen) with their trunk and
  two leaf cards in five; the real sun's map gets only the trees in its box. Nothing changes to the eye.
- **The trees' cost in the log and on the Effects page:** the log's line "GPU: effects a frame" now has "new trees",
  and the Effects page shows its milliseconds next to New trees, as for the grass.
- The log also says how many of the map's trees a frame showed: "forest drawn - N of its M trees in view (K far)".
- Check: walk and turn on prt_fild08 and pay_fild01 - no tree pops in or out at the screen's edges; the frame rate
  against 2.0.4; send a report (the log has the new numbers).

# uaRO Graphics 2.0.4 — testing

- **Realistic trees of the game's own leaves:** the cards of the new trees' crowns carry pieces of the model's own
  leaves picture (the AI's when shown) with a ragged round edge - the trees' shapes the driver's, steady as you walk,
  the leaves RO's. A model whose picture is not seen yet shows the drawn leaves for a moment.
- **Anime-style trees off by default** (the setting stays) - they did not fit the game's pictures.
- **Sakura:** no autumn on it, its crown pink and white (its one picture is bark and blossom - it went brown); the
  blossom clouds in their own pictures' colours.
- **Autumn softer**, on the broadleaf trees only - no whole crowns of orange; firs, palms and sakura keep their colours.
- Check: prt_fild08, pay_fild01/02, Prontera with New trees on - do the crowns look like RO's leaves, do they fit the
  ground and the houses; the sakura.

# uaRO Graphics 2.0.3 — testing

- **Anime-style trees** (Effects → Remaster, on by default, needs New trees): the crowns are large soft puffs with a
  scalloped rim, cel-shaded in steps over each clump - warm light, cool blue shade, a bright rim against the light,
  a faint ink at the edges. Off: the realistic trees. Switching in the console (~) grows the forest anew at once.
- **Sakura pink and white again:** a blossom's picture now goes to the blossom (the smallest tree that holds it),
  not to the cherry it hangs in; blossom is pink until its colour is seen.
- **Realistic trees:** small leaves (16 a card, each turned its own way, the cards round), each clump of the crown a
  volume of its own, deeper shade inside.
- Check: prt_fild08 and Prontera with New trees on, the anime style on and off; the frame rate.

# uaRO Graphics 2.0.2 — testing

- **Sakura is back, grown anew.** Prontera's cherries are a cherry's trunk and branches (벗나무01) with clouds of
  pink and white blossom hung about them (분홍벗꽃, 하얀벗꽃); 2.0.0 took the cherry for a plain tree and hid the
  blossom in its crown - all green. Now a cherry is its own kind - a dark trunk, wide low branches - and each blossom
  cloud is grown where the map hangs it, of small five-petal flowers in its own colour (pink, white).
- **The trees' light:** the crowns' upper clumps shade the lower ones and the branches - the real sun's map, with
  Real sun shadows; other trees and houses shade them too. The leaves let a little sun through: their far side
  glows warm green.
- **The ground's seams:** the light straight lines along the cells on even ground (Comodo's beach, 1.25-2.0.1) are
  gone - the band that covers a cell's rim is narrower and fades out.
- Check: Prontera (pink and white sakura where they were), prt_fild08 (crowns lighter on top, shaded below),
  cmd_fild02 (no grid of light lines on the sand).

# uaRO Graphics 2.0.1 — testing

- **Shadows no longer turn with the camera.** The sun's way was taken from the screen (the shadows always fell, say,
  down-right on the screen) - right for the old screen shadows, wrong for real shadows in the world: turning the
  camera turned the sun. The sun now stands still in the world; every shadow (the trees', the houses', the
  characters') falls the same way on the ground however the camera turns.
- **Shadows no longer creep as you walk.** The sun's map followed the player smoothly and the shadows' edges jumped
  from texel to texel; it now moves in its own texels' steps.
- Check: Real sun shadows on, walk and turn the camera on prt_fild08 - the shadows lie still on the ground.

# uaRO Graphics 2.0.0 — testing

- **Trees 2.0** (Effects → Remaster → New trees, Direct3D 12, Remaster mode): the map's trees are no longer guessed
  from the picture each frame - the driver reads the map file (where each tree stands, its model, turn and size) and
  the tree models, grows its own 3D tree for each - a trunk with branches and a crown of clumps of leaves (firs in
  tiers, palms with fronds, bushes), in the colours of the map's own trees - and draws the whole forest in a few
  calls; the game's tree models are left out. They sway in the wind (with One wind for everything, its wind) and,
  with Real sun shadows, cast their shadows. They must stand still as you walk and turn.
- What to look at, first on prt_fild08: do the trees stand where the old ones stood (not shifted, not floating, not
  sunk); are any old trees still drawn among the new (doubled); is anything else gone that is not a tree (a fence,
  a sign by a tree); the colours and the size; the frame rate. Screenshots please.
- The log says what happened: "new trees: <map> has N tree models, M of them read", then "forest grown - N trees"
  and "forest drawn, N of the game's tree draws left out". If it says "do not match" or "axes are not known", the
  old new trees are drawn instead - send that report.

# uaRO Graphics 1.25.2 — testing

- **New trees: no grass and flowers on the crowns.** The new trees were drawn before the grass, and the grass took
  their green for green ground - blades and flowers grew on the crowns, their edges went to steps. They are drawn
  after the grass now. Check prt_fild08 with New trees and Grass on.

# uaRO Graphics 1.25.1 — testing

- **Important: since 1.15.0 the game's shader compiler stopped at one mistake of mine** (a name used twice in the
  smooth hills' code) and built every shader without the newest features. So nothing new since 1.15 was really on:
  smooth hills, world reflections, living characters, land past the edge, smooth models, seasons, the wind, dust in
  the light, the windows, the trees' 1.24 look. Fixed; the check here now uses the game's compiler's old rules.
  After the update all of them that are switched on work for the first time - look at each, and if something looks
  wrong, turn it off in the console (~) and send a report.
- **The new trees** no longer blink as the camera turns or the player walks: each crown is kept from frame to frame
  with its own layout; the bunches are round leafy patches, not squares; their swaying a third of before.

# uaRO Graphics 1.25.0 — testing

By the screenshots of the reports:
- **The desert's "cracks"** - thin dark lines along the ground's cells since 1.9.0 (Ground seams): a cell's own band
  now lies whole at its edge and is drawn first, the texture's dark rim no longer shows through. Check moc_fild maps
  with Ground seams on - no grid of lines.
- **Sand drift** fewer and fainter streaks, in drifting runs, instead of the whole ground hatched.
- **The level 99 aura** no longer goes into the glow: no white pools under the players (Prontera's square).
- **Fish** fewer, larger and soft, only in deep water - before they read as scratches on the water.
- **Shore foam** in thin lines rolling in along the shore and a rim at the edge, not a row of white dabs.
- **Dragonflies** smaller, their wings a faint shimmer - from above they looked like sticks with a rim.
- **Effects page: "Recommended for a strong card"** - one click turns on Materials of the world, soft shadows,
  both sun shadows, rounded crowns, the sky and the land past the edge, smooth hills, living characters, filmic
  tones; calmer far blur and colours. The rest stays.
- Not in the button, worth a try: AI textures page, the world's model "general" instead of "anime" - the stones and
  roofs less drawn, fewer dark smudges (the pictures are made again as you play).

# uaRO Graphics 1.24.0 — testing

- **Trees from above, by the screenshots** (Living world: Rounded tree crowns, Every tree its own, seasons):
  - autumn no longer paints whole crowns one orange: fewer trees turn, and each only in patches with green left
    between; the turned leaves keep their own light and shade (the old formula lifted the shade - flat orange);
  - each crown in clumps of its own - lighter and yellower, deeper and bluer - so the same leaf picture on every
    layer shows less;
  - the crown's lower layers in the shade of the upper ones, darker and cooler; the top lit by the sky.
  Check on prt_fild maps from the usual camera: the crowns look deeper, layered, the autumn ones mixed green and
  gold; compare with the console's Rounded tree crowns and Every tree its own switches.

# uaRO Graphics 1.23.0 — testing

- **Dust in the light** (Effects → Living world, off by default, needs Motes in the air, Direct3D 12): indoors and
  in dungeons the dust shows where light falls - in the beams, by the torches - in that light's colour, and hardly
  in the dark corners. Check in Prontera's church, a pub, Glast Heim: dust visible in the light shafts and around
  the torches, nearly none in the dark; nothing changes in the fields (pollen stays as it was).

# uaRO Graphics 1.22.0 — testing

- **One wind for everything** (Effects → Living world, off by default, Direct3D 12): the grass, the leaves'
  gusts, drifting sand, rain, snow, falling leaves and both kinds of clouds go one way with one wind; it turns
  slowly over minutes and blows stronger and weaker. Check on a grassy field with gusts and the sky on: the grass
  leans and the gusts run the way the clouds go; turn the camera - everything keeps the world's way, nothing jumps;
  in the rain the drops slant with it. Without the setting everything is as before.

# uaRO Graphics 1.21.0 — testing

- **Windows at night** (Effects → Living world, off by default, needs Materials of the world and the night,
  Direct3D 12): as the night comes the houses' windows light up warm, about two in three. The panes are found in the
  walls' pictures by themselves - dark even patches in a lighter wall - so check what lights up: Prontera, Payon,
  Alberta at night (the console's night or Night clock). Wanted: windows and maybe doors glowing; not wanted: roofs,
  shadows painted on walls, whole dark walls. A screenshot of anything wrong helps.

# uaRO Graphics 1.20.0 — testing

- **Seasons on the map** (Effects → Living world, off by default, Direct3D 12): by the computer's calendar - winter
  snow on the roofs, the rocks' tops and the ground; autumn's fallen leaves and drier grass; spring's greener
  fields. Snowy maps keep snow on their roofs all year; the desert and the tropics have none. Now (October): leaves
  on the ground of prt_fild maps and Prontera, the grass a little towards straw. Check: leaves only on what faces
  up, not on walls, nothing on the characters; Lutie's roofs white. To see the winter, set the computer's date to
  January for a minute.
- **Wet in the rain** (Effects → Living world, off by default, needs the weather): where it rains the walls, roofs
  and models get wet too - darker, a sheen along them, water running down the walls.

# uaRO Graphics 1.19.0 — testing

- **Smooth light on the models** (Effects → Remaster, off by default, Direct3D 12): rocks, trunks and other models
  the game lights face by face get their light spread smoothly over the curved parts; the sharp edges of walls and
  roofs stay. Check on rocky maps (mjolnir, cliffs of prt_fild, payon's houses): rocks without the flat facets, the
  houses' corners still sharp, no flicker as the camera turns, the frame rate about as before. The log says
  "smooth models: N draws of <map> lit face by face" - if that line never shows, the client lights its models
  smoothly itself and the setting does nothing.

# uaRO Graphics 1.18.0 — testing

- **Land past the map's edge** (Effects → Remaster, off by default, needs the Sky, Direct3D 12): at the edge of a
  field map the ground goes on - fields and darker woods in the map's own colours, fading into the haze - instead
  of the void or a sea of clouds. Check at the edges of prt_fild maps: no hard line where the map ends, the colour
  close to the map's ground, nothing over the map itself.
- **Fixed, worth a look:** three settings passed one of their values into the wrong place of the shaders.
  - **Haze far off** (since 0.34) was never shown on Direct3D 12 - now it is: if it is on, the far part of the
    picture goes into the map's haze colour. Too much - lower Haze or turn it off and say so.
  - **The sky at night** (since 0.69) never went dark and had no stars - now it does.
  - **Living characters** (1.17.0) took the sun as on the horizon - now at its real height, the rim softer.

# uaRO Graphics 1.17.0 — testing

- **Living characters** (Effects → Living world, off by default, Direct3D 12): characters and monsters are lit by the
  sun from the side - the edge towards it brighter with a warm rim, the far side a little darker. With Real sun
  shadows on they also darken in the shadow of a house or a tree. Check in a town by day: the rim sits on the side
  the shadows fall away from, the heads and the bodies alike, no bright seam between a head and its body, no flicker
  while walking; walk into a house's shadow - the character darkens smoothly, not with a jump.

# uaRO Graphics 1.16.1 — testing

- **Lighter grass in Classic+** (Direct3D 12, the Remaster mode off or the card under 6 GB): its blades are at most
  16 pixels long instead of 28 - the grass pass costs up to 1.7 times less at high resolutions. Check: grass on,
  Classic+, the frame rate on a grassy field against 1.16.0; the log says "grass: on <map>, blades up to N pixels".

# uaRO Graphics 1.16.0 — testing

- **World reflections in water** (Effects → Remaster, off by default, Direct3D 12, needs Modern water): the water
  reflects the houses, rocks and trees along its shores, also what is above the screen's top edge. Check on maps
  with water by houses and cliffs (alberta, comodo, payon's pond, izlude): the reflections stand upside down at the
  shore, move with the waves, nothing from under the water shows in them, no flicker when the camera turns. The log
  says "world reflections: N draws ... mirrored at WxH".
- Under the hood: the scene table of the Direct3D 12 renderer holds four textures now, the sun map's vertices carry
  their colour. If anything about water, soft shadows, real sun shadows or the effects broke since 1.15 - say so.

# uaRO Graphics 1.15.0 — testing

- **Smooth hills** (Effects → Remaster, off by default, Direct3D 12): the ground is lit by the sun over smooth slopes
  instead of cell by cell - the sunny side of a hill brighter, the far side in a soft shade. Check on hilly fields
  (prt_fild08, pay_fild maps, mjolnir): no steps or squares of light along the cells, no dark lines along cliffs,
  the seams' strips the same brightness as the ground around; the console (~) switch shows the difference at once.
  The log says "smooth hills: NxM corners of <map>'s ground".

# uaRO Graphics 1.14.0 — testing

- **Materials of the world now work on Direct3D 11 too** (Classic+ on weaker cards): relief, gloss and the large
  patches of shade on the ground, the houses and the rocks; the material is made at 256 pixels a side there. Check:
  Direct3D 11, Effects → Materials of the world on - stones and bricks lit from their side, no flicker, no black
  textures; the log says "materials: made on the card ... (Direct3D 11)".
- **Cliffs**: on steep slopes where the game stretches a texture into stripes, the materials lay a stone grain with
  cracks of its own over it (both renderers). Check on a map with rocks (mjolnir, prt_fild maps with cliffs).

# uaRO Graphics 1.13.1 — testing

- The settings app's Living world and Remaster blocks have **Turn all of these on** and **Turn all of these off**
  buttons - to switch the whole set on for testing and back as quickly.

# uaRO Graphics 1.13.0 — testing

- **Volumetric fog** (`effects.volumetric_fog`, density `effects.volumetric_fog_density`, the Remaster block, Remaster
  mode only, Direct3D 12, off by default). A light fog lies low over the fields and the towns, drifting slowly, lit by
  the sun. With Real sun shadows the light goes through it in rays: through the crowns, past the houses. Characters
  far off are in it too. No sunlight in it at night.
- The log: `volumetric fog: over <map>, lit by the sun's map`.
- How to check: prt_fild08 or pay_fild01 by day, with Real sun shadows - switch it in the console, move Density,
  compare with Tab. To know: the rays showing, ripples or noise, too thick, the frame's cost.

# uaRO Graphics 1.12.0 — testing

- **Real sun shadows** (`effects.real_shadows`, the Remaster block, Remaster mode only, Direct3D 12, off by default).
  The driver gathers the frame's opaque geometry (ground, houses, walls, rocks, crowns) in the world, draws it as the
  sun sees it into a 2048x2048 shadow map about the player, and darkens what the sun cannot reach. Unlike the old
  Shadows of houses and trees (a search on the screen): things off the screen cast too, shadows do not break at the
  edges, crowns cast dappled shadows through their holes. The way is the sun's, turning with Time of day. The
  darkness is the Shadows of houses and trees slider; the screen's own sun shadows are not drawn meanwhile.
- The log: `real sun shadows: N draws, M triangles of <map> into a 2048x2048 map`.
- How to check: prontera, prt_fild08, payon by day - switch it in the console, compare with Tab. To know: the way
  right (as the characters' shadows), stripes or shimmer on the ground (self-shadowing), shadows sliding off their
  objects, the frame's cost.

# uaRO Graphics 1.11.0 — testing

- **The remaster's phase D: light.** Two new boxes in the Remaster block (off by default, Direct3D 12):
  - **Light from the sky** (`effects.sky_light`): surfaces facing up take a little of the sky's colour (cool by day,
    golden at sunset, deep blue at night), those facing down the ground's warm one. Houses', rocks' and the ground's
    shapes read better. Weaker indoors.
  - **Reflected light** (`effects.bounce_light`): light bounces between nearby surfaces - grass tints walls' feet
    green, a red roof warms the eaves, sand lights the stones.
- The log: `light from the sky: on <map>`.
- How to check: prontera (walls, roofs), prt_fild08, moc_fild01 - switch them in the console, compare with Tab; in
  the evening with Time of day. To know: too strong, noise or ripples at edges, the frame's cost.

# uaRO Graphics 1.10.1 — testing

- A safeguard: the vertex shader's new part (Full crowns) is under the "without the newest features" guard - should
  Windows' compiler not take it, the game draws as before rather than nothing. No visible change.

# uaRO Graphics 1.10.0 — testing

- **The remaster's phase C: new trees** (`effects.new_trees`, the Remaster block, Remaster mode only, Direct3D 12, off
  by default). A tree's flat crown (a few cards with a picture of leaves) is not drawn - the driver builds a volume
  crown of 24-140 bunches of its own leaves instead: pieces of the picture where it is leaves, in the crown's volume
  in the world, each a card facing the camera. The bunches are lit as parts of a ball (lighter to the sun, darker
  inside and below) and sway in the wind, the top more. Turning the camera shows the crown round. The trunks are the
  game's. All automatic - no models, no artist. Tiny sprigs and long hedges stay as the game draws them.
- The log: `new trees: N crowns of <map> drawn as M bunches of leaves in K draws`; the console: New trees.
- How to check: prt_fild08, pay_fild01, Geffen's fields - switch it in the console, turn the camera, compare with
  Tab. To know: how the crowns look (too "fluffy"? see-through?), bunches hanging apart, trees missing, the frame's
  cost.

# uaRO Graphics 1.9.9 — testing

- **Cheaper grass.** The costliest effect (1.3-3.2 ms a frame at 4K): how far the steps trod the grass down was
  worked out for every blade a pixel looked at (up to 28 blades times 64 steps), now once a pixel. It should help
  most where there are many characters and steps. It looks nearly the same.
- How to check: `GPU: effects a frame ... grass N ms` in the log (debug.profile = 1) against earlier reports on the
  same maps (prt_fild08 ~1.3 ms, pay_fild01 ~3.2 ms).

# uaRO Graphics 1.9.8 — testing

- **Pebbles on the ground** (`effects.ground_clutter`, the Remaster block, off by default, Direct3D 12): small stones
  here and there on bare earth, roads and sand, each lit from its side with a little shadow, staying put as the
  camera moves. Not on snow.
- **Rounded tree crowns** now with the leaves' relief too: the picture's light parts stand out, the dark ones sink,
  from the sun's side.
- How to check: prt_fild08 (paths), moc_fild01 (sand) - Pebbles on the ground in the console; any field with trees -
  Rounded tree crowns with Tab.

# uaRO Graphics 1.9.7 — testing

- **Full tree crowns** (`effects.full_crowns`, the Remaster block, off by default, Direct3D 12): each crown is drawn
  twice more - a little larger, darker and behind, a little smaller and lighter in front. The old trees' thin flat
  crowns look dense and deep. The trees cost three times as much to draw.
- How to check: prt_fild08, pay_fild01 - switch it in the console (`~`) and compare holding Tab. To know: crowns
  doubling as the camera moves, dark rims showing, crowns creeping through trunks and roofs.

# uaRO Graphics 1.9.6 — testing

- **Fixed: effect changes that "would not turn on".** The driver applied effects changed in `uaro-graphics.ini` in a
  running game only by an old list (up to 0.33). A change of a newer one alone - grass, foliage, the sky, the relief,
  soft particles, all of the Living world and the Remaster - changed the file and the game did not see it until one
  of the old ones changed too. Hence "turned the grass off in the console and cannot turn it on", and settings-app
  boxes that did nothing until a restart. Now every effect setting is compared and any change applies at once; the
  log: `settings: uaro-graphics.ini changed: grass 0 -> 1`.
- How to check: in the console (`~`) switch Grass, Leaves in the wind, Sky, Materials of the world off and on.

# uaRO Graphics 1.9.5 — testing

- **Ground seams: rounded corners, wider.** The line between textures now follows each texture's share, smooth
  between the cells' middles: at the corners a texture goes round the line cuts an arc instead of a step. The strips
  reach the cell's middle, the noise moves the line further.
- **Seams far cheaper.** The strips go in batches: one draw for each pairing of textures instead of 600+ apart
  (moc_fild01 dropped to 35 fps). The log: `ground seams: N kinds of strips in M draws ...`.
- How to check: prt_fild08 and pay_fild01 - the paths' edges (the steps smooth); moc_fild01 - fps with the seams. The
  console is handy for it: `~`, Smooth seams of the ground, hold Tab.

# uaRO Graphics 1.9.4 — testing

- **A hang when switching effects in the console.** In reports R-HAAN9H and R-AQ6PVP the card was lost (device
  removed) a couple of seconds after an effect was turned off from the console; before that, Soft shadows and Glow
  turned on again could not make their buffers ("no video memory"). Now an effect turned off keeps its buffers (until
  the size changes or the game ends) - turned on again it takes the same ones.
- **Breadcrumbs on the card.** If the card is lost all the same, the log has `the GPU stopped in "..."` - the pass it
  stopped in (shadows, grass, seams, the game's world, the interface...). That will show the exact cause.
- How to check: open the console (`~`) and switch effects off and on many times, Soft shadows, Glow, Characters'
  shadows, Clouds the most. If the game hangs all the same, send a report right after restarting it.

# uaRO Graphics 1.9.3 — testing

- **A console in the game.** The `~` key (left of 1) opens a panel over the game with every switch and slider of the
  Effects page, by blocks, in the settings app's language:
  - Up/Down (PgUp/PgDn by 10) choose; Space or Enter switch; Left/Right move a slider or the mode;
  - **hold Tab** - the chosen effect off while held: an instant with/without comparison;
  - `~` or Esc close.
  Changes go into `uaro-graphics.ini` at once and apply in the running game; the settings app sees the same. While
  the console is open the game gets no keys (the chat does not open). `~` itself never reaches the game.
- A reminder: **Ctrl+Shift+F12** - a screenshot at the screen's full size with every effect into the game's
  `ScreenShot` folder.
- The log: `console: open` and each change (`console: effects.grass = 0`).
- How to check: press `~` in the game, walk the list, switch a couple of effects, hold Tab on Smooth seams of the
  ground.

# uaRO Graphics 1.9.2 — testing

- **The ground's seams cover the thin lines along the cells' edges.** After R-PKXZT5's screenshots: the desert shows
  a grid of thin dark lines along the cells - the textures' outermost texels (their dark rim and what bleeds in
  from the next quarter of a shared picture). 1.9.1's seams only broke them into dashes: where the noise kept a
  cell's own texture, the line stayed. Now at the edge each cell also covers a narrow band with its own texture
  taken from a little inside - no line stays on either side. The neighbour's texture is taken without its rim too.
- How to check: moc_fild01 - the grid of lines on the sand gone; prt_fild08 - the paths' edges. A pair on/off in one
  run.

# uaRO Graphics 1.9.1 — testing

- **Smooth seams of the ground without dark "bricks".** 1.9.0's strip carried the neighbour's texture on past its
  edge, and many maps' ground takes a quarter of a shared picture of four - the strip showed the next quarter
  (cracks, stones), and dark rectangular pieces stood along the cells (moc_fild01, report R-PMH75Q). Now the
  neighbour's texture is mirrored across the edge and stays its own.
- How to check: moc_fild01 and prt_fild08 with Smooth seams of the ground - a pair on/off.

# uaRO Graphics 1.9.0 — testing

- **The remaster's phase B, its first step: smooth seams of the ground** (`effects.ground_seams`, the Remaster block,
  off by default, Direct3D 12). The driver gathers the ground's cells the game draws, turns them into the world, and
  where neighbouring cells have different textures (grass and road, grass and sand) draws strips along their edge:
  one texture reaches into the other by a noise both share. The line wanders instead of stepping along the cells.
  Works with the AI textures, the materials and the grass.
- The log: `ground seams: N kinds of strips over M triangles of <map>'s ground`.
- How to check: prt_fild08 - the paths' edges; pay_fild01, moc_fild01 - sand and grass. Compare with the box off. To
  know: holes, shimmer or "bands" along the edges, anything lost under the characters, the frame's cost.

# uaRO Graphics 1.8.4 — testing

- **Fixed:** the driver read the game's camera only for the older effects (water, torches, characters' shadows and
  the like). With only the newer ones on - grass, the Living world, the materials - they had no camera: no trees'
  shades, no patches on the ground, no creatures. Now the camera is read for any effect the driver draws itself.
- How to check: everything off but, say, Materials of the world or Butterflies - they work.

# uaRO Graphics 1.8.3 — testing

- **The grass no longer "flows in waves" as you walk.** Its sway and the gusts went by the pixel on the screen: as
  the camera moved, the waves' pattern moved with it. Now by the place in the world: the grass sways where it stands
  and walking does not drive it. Parting under the feet a little softer.
- **Far fewer flowers, in patches** instead of a speckle over the whole field; heads a little larger.
- How to check: prt_fild08 with Grass and Flowers - walk across the field.

# uaRO Graphics 1.8.2 — testing

- **Autumn trees** (Every tree its own, seasons) golden and orange now, not muddy brown: the autumn's colour taken
  lighter (after report R-4NMYFH's screenshots).
- **Butterflies and dragonflies** nearly twice as large: at 4K they were specks. Birds on the ground a little larger.
- How to check: prt_fild08 - the autumn crowns and the butterflies by day.

# uaRO Graphics 1.8.1 — testing

- **Screenshots in a report.** On the Diagnostics page, by the report's sending, the box Attach screenshots (on): the
  6 newest of the last day from the game's `ScreenShot` folder (Print Screen in the game) and Windows' Screenshots
  (Win + Print Screen), made 1600 pixels across at most. The message after sending says how many went.
- **Fixed:** the first start after an update could draw in the game's process instead of the 64-bit renderer: it
  compiled its shaders (7 s) and read a big texture pack's index (14 s) and did not answer within 20 s. Now up to
  90 s. 1.8.0's log had `bridge: the 64-bit renderer did not answer`.
- **The materials of the game's textures without levels** (its own, not the pack's) now have levels and the large
  scales of relief too - less shimmer far off.
- How to check: update, start the game - no `did not answer` in the log. Take a couple of screenshots (Print Screen)
  and send a report with the box on.

# uaRO Graphics 1.8.0 — testing

- **The remaster begins.** A new block "Remaster" at the bottom of the Effects page:
  - **Mode** (`effects.tier`): Automatic (Remaster on a card with 6 GB or more, else Classic+), Classic+ (any
    card), Remaster (a strong card).
  - **Materials of the world** (`effects.materials`, strength `effects.materials_strength`, off by default,
    Direct3D 12): the card makes relief, height and gloss for each texture of the ground, the houses and the rocks.
    The sun lights stone, brick and bark from the side, hollows are darker, smooth stone and tiles glint, the
    ground's repeats hide under large patches. In Remaster also depth (parallax). The old Ground relief gives way to
    the materials on those textures.
- **Fix:** with the 64-bit renderer (bridge) a change of many effects in a running game reached it late - sometimes
  only a minute later. Now at once.
- How to check: prontera (paving, walls), prt_fild08 (ground, rocks), payon (wooden houses). Switch Materials of the
  world in the game and compare. The log: `materials: made on the card ...`. To know: seams along the ground's
  cells, shimmer far off, the frame's cost (debug.profile).

# uaRO Graphics 1.7.3 — testing

- **Living world, creatures and particles** (Direct3D 12, all off by default):
  - **Butterflies and dragonflies** (`effects.butterflies`): by day butterflies over the fields and green towns, their
    shadows on the ground; dragonflies over the water only.
  - **Birds** (`effects.birds`): by day a flock pecks on the ground and flies off when you come near; another comes
    later.
  - **Leaves from the trees** (`effects.leaf_fall`): leaves come off the trees in sight, turn and lie on the ground -
    golden in autumn, sakura petals in Amatsu.
  - **Embers from the fires** (`effects.embers`, needs Live torches): sparks rise from the torches.
  - **Breath in the cold** (`effects.breath`): in Lutie the characters' breath shows.
  - **Splashes** (`effects.splashes`): a step through shallow water throws up drops.
  - **Drips in the caves** (`effects.cave_drips`): in damp dungeons drops fall and rings spread on the floor.
  - **Under the sea** (`effects.underwater`): in Byalan's dungeon bubbles rise and light falls in bands.
- The creatures are drawn as small quads each (not over the whole screen): they cost little.
- How to check: prt_fild08 by day (butterflies, birds - walk up to the flock), pay_fild01 (leaves), payon or
  prontera at night by the torches (embers), xmas (breath), comodo through the water (splashes, dragonflies),
  pay_dun00 (drips), iz_dun01 (under the sea). The log: `living creatures: on <map>`.

# uaRO Graphics 1.7.2 — testing

- **Living world, the water** (needs Modern water, all off by default):
  - **Surf and wet sand** (`effects.shore_foam`): the foam at the shore comes in and goes back, the sand just above
    the water dark and wet in a band (wet sand: Direct3D 12).
  - **Fish** (`effects.fish`): dark shapes of fish circle where the water is deep enough; now and then a ring where
    one jumped.
  - **Swamps** (`effects.swamp`): in Umbala, Brasilis and Dewata the water is murkier, bubbles swell and burst, a
    greenish mist over the ground.
- The sun's light on the shallow bed was already in Modern water - no box of its own.
- How to check: comodo or cmd_fild01 (surf, sand), pay_fild04 or prt_fild01 by the water (fish), um_fild01 (swamp).

# uaRO Graphics 1.7.1 — testing

- **Living world, the ground and the grass** (all off by default, Direct3D 12):
  - **Flowers in the grass** (`effects.flowers`, needs Grass): a flower here and there among the blades, the region's
    colours - Prontera's meadows, Payon's reds, Amatsu's pinks, bright tropical ones in Umbala and Comodo.
  - **Morning dew** (`effects.dew`, needs Grass): from dawn to noon drops on the blades' tips flash.
  - **Morning fog** (`effects.morning_fog`, needs Haze and fog): 4:30-10:30 a low fog over the fields and the towns,
    thickest 6-8.
  - **Drifting sand** (`effects.sand_drift`): in the desert of Morroc and Veins the wind drives sand over the ground.
  - **Glittering snow** (`effects.snow_sparkle`): Lutie's and the ice caves' snow glitters.
  - **Damp cave walls** (`effects.wet_walls`): in damp dungeons the stone glistens in the light, trickles run down.
- Better without new boxes: the grass under the feet now also **parts to the sides**; **Gusts of wind** bend and
  lighten the grass in a band; **raindrops' rings** in the puddles of Wet ground.
- How to check: prt_fild08 with Grass (flowers, dew in the morning, gusts), moc_fild01 (sand), xmas (snow),
  pay_dun00 or prt_sewb1 (walls), any field in the morning (fog).

# uaRO Graphics 1.7.0 — testing

- **A new block "Living world"** at the bottom of the Effects page. Its first part, the trees, all off by default:
  - **Rounded tree crowns** (`effects.tree_volume`): a crown lit as a ball - the side to the sun lighter, the far side
    and the middle darker. With "Time of day" the light turns.
  - **Sunlight through the leaves** (`effects.leaf_light`): the leaves on the crown's sunward edge glow warm green,
    more in the morning and the evening, none at night.
  - **Every tree its own, seasons** (`effects.tree_tints`): each tree its own shade and brightness. From September to
    November (the computer's calendar) some trees golden and orange - not in deserts, jungles and dead woods; rime
    on the crowns in Lutie.
  - **Gusts of wind** (`effects.gusts`, needs Leaves in the wind): now and then a gust crosses from the left and the
    trees bend one after another.
  - **Sun spots under the trees** (`effects.dapple`, needs Shadows of houses and trees, Direct3D 12): a crown's shadow
    with stirring spots of light.
- How to check: a field with trees (prt_fild08, pay_fild01), one box at a time. The autumn colours show now (October).
  Lutie (xmas): rime on the crowns.

# uaRO Graphics 1.6.7 — testing

- **The emotions (/emotion) from the pack show in the game.** The builder put them into the pack as effects, and the
  skills' effects are off in the game by default, so the emotions stayed as they were. `emotion.spr` now goes with
  the icons (on by default). No need to rebuild the pack: the emotions already in it show at once.
- How to check: show any emotion in the game (Alt+1...Alt+0 or /lv, /heh) - it is sharp like the icons.

# uaRO Graphics 1.6.6 — testing

- **The builder reads the game's files on every core but one** (1.6.4 stopped at 8 threads: 20-50 % of a CPU with
  16-32 threads). The threads are bounded by the memory: each needs the archives' directories of its own (about
  100 MB with Palettes.grf's palettes), at most half of the free memory is taken. The log at the start of reading:
  `reading the game's files in N threads (M cores, K MB free)`.
- How to check: Make the pack - during the reading the CPU nearly fully busy.

# uaRO Graphics 1.6.5 — testing

- **The pack builder names DATA.INI's archives that did not open.** Such a line of DATA.INI was passed over silently;
  the log now says `not read: <file> (<why>)` after the archives, also for a file that is no GRF, GPF or ZIP.
- How to check: Make the pack - the line `texture pack: ... pictures' files` shows every archive of DATA.INI, the ones
  that did not open with the reason.

# uaRO Graphics 1.6.4 — testing

- **The pack builder reads the game's files in several threads.** The first pass (open every file, take the sprites'
  frames and hair colours apart, work out the keys - what the pack lacks) ran in one thread and stood long on
  `data\sprite\*.spr`. Up to 8 threads now (the CPU's cores but one), each with the archives open on its own; the
  same result. The log: `the game's files read by N threads`.
- How to check: Make the pack - the reading much faster, several cores busy.

# uaRO Graphics 1.6.3 — testing

- **Small pictures by a model of their own.** "Models by group" has "Small pictures by a model of their own": a
  picture smaller than the given points on both sides (the game's: an icon is 24, an item on the ground about 23, a
  character's frame 30-80) is made by the chosen model ("Their model", anime by default) whatever its group has. So
  anime 6B stays for the large ones while icons and small frames are not redrawn. Off by default; the side 16-256, 48
  by default. In the ini: `[ai] small_pictures`, `small_side`, `small_model`.
- It holds in the game ("Make new ones while playing"), in the pack builder and in the ⚠ counts: once on, small
  pictures 6B made count as "another model's", and "Make again ⚠" makes them again.
- How to check: tick it, Apply - "Icons and windows" shows ⚠ (if 6B made the icons); "Make again ⚠" → Make the pack
  - the icons as anime makes them again.

# uaRO Graphics 1.6.2 — testing

- **A viewer for the models.** The AI page's preview has "Open the viewer": a window of its own (it can fill the
  screen), the four side by side in their own proportions. The wheel zooms all four about the point under the mouse,
  a drag moves them together, a double click shows them whole again; keys +, -, 0, Esc. Zoomed in, sharp pixels, no
  smoothing. The zoom is in the window's title.
- The page's small pictures are no longer squashed: a narrow picture (a Kafra) stays narrow.
- How to check: Show an example for NPCs → Open the viewer → zoom into the face with the wheel, drag - all four move
  together, the proportions right.

# uaRO Graphics 1.6.1 — testing

- **Preview: zoom.** Under the example's pictures - "Zoom": whole, x2, x4, x8. Zoomed, all four show the same middle,
  pixel by pixel (no smoothing), two to a row and larger - to compare the models' lines and details.
- How to check: the AI page → preview → Show an example → Zoom x4: four large pictures two to a row, sharp pixels.

# uaRO Graphics 1.6.0 — testing

- **A model of its own for each group of pictures.** The AI page has "Models by group" in place of two models:
  World (ground, buildings, water), Monsters and pets, Players, NPCs, Items on the ground, Skills' effects, Icons and
  windows, Loading screens and backgrounds - each with general / anime / anime 6B. Above them "Models": Fast (anime
  everywhere), Balanced (general for the world, anime for the rest, anime 6B for the loading screens), Maximum
  (general for the world, anime 6B for the rest), Own. In the ini: `[ai] model_world`, `model_monsters` ... (the old
  `model`, `sprite_model` stay the defaults).
- **The pack remembers each picture's model** (its index: the model and the group). Under each group - how many of
  its pictures the pack holds and how many another model made (⚠). Pictures of before 1.6 count as made by the models
  of then (model / sprite_model); the sprites' frames of before 1.6 get their group at the next build.
- **Making again.** "Make again ⚠ (N)" opens the Texture pack page with "Make again what another model made" ticked:
  the build makes such pictures again by their group's model, each into its place in the pack.
- **Preview.** At the bottom of the page: a group → "Show an example" - a picture of the group from the game's files
  (its middle, up to 128) as the game has it and through general, anime, anime 6B, made by the graphics card in a few
  seconds (Direct3D 12).
- In the game with "Make new ones while playing" each group is made by its own model.
- How to check: choose "Balanced" and, say, anime 6B for "Players" → Apply; "Players" shows ⚠ with a number; "Make
  again ⚠" → the Texture pack page, the box ticked → Make the pack - the log says `made again by another model`; the
  preview of "Monsters" shows four pictures.

# uaRO Graphics 1.5.11 — testing

- **The update did not download: `CRYPT_E_NO_REVOCATION_CHECK`.** Windows' own curl checks that GitHub's
  certificate is not revoked; where an antivirus, a firewall or the provider blocks that check, the download failed.
  update.bat and get-dlss.bat now try again with the check as far as it can go, and failing that without the
  revocation check (the connection itself is still encrypted and checked).
- Update now always puts its own update.bat into the game's folder (before: only when there was none), so a fix of
  update.bat arrives with the new settings app at once.
- Whoever's update already fails with this error - once, by hand: download
  https://github.com/kwebmn/ragnarok-graphics/raw/main/release/uaro-graphics.zip in a browser, unpack it over the
  game's folder and run install.bat.
- How to check: Update now on a computer that had the error - the download goes through.

# uaRO Graphics 1.5.10 — testing

- **The ground's relief faded under the pack's HD textures** (Prontera's paving went flat). The relief comes from
  the brightness changing between neighbouring texels; an HD picture has 2-4 times as many, the neighbours nearly
  alike - x4 kept a quarter of the relief, x2 half. The change is now taken across one texel of the game's own
  texture, whatever picture lies over it: the relief as with the game's textures. Direct3D 11 and 12.
- How to check: Prontera, the pack at x4, the ground's relief on - the paving raised again (Pause to compare).

# uaRO Graphics 1.5.9 — testing

- **Auto setup for small cards (Direct3D 11, and up to 4 GB):** the texture pack's pictures at twice the game's
  size, not four times (`[ai] scale = 2`), and on Direct3D 11 no day and night. From the reports of the tester with a
  Radeon HD 6570 in Prontera: x4 - 25.7 ms of the card's work a frame and 33 frames a second, x2 - 21.2 ms and 42;
  day and night - about 1 ms.
- How to check: Auto setup on a small card - the AI page's scale "2 times", day and night off on the Effects page.

# uaRO Graphics 1.5.8 — testing

- **Two switches of their own on the AI page:**
  - "HD textures from the texture pack (uaro-textures.zip)" - the pack's ready pictures shown;
  - "Make new ones while playing (debugging)" - making them in the game (Direct3D 12), now on its own.
- Both on - as before: from the pack, and what it lacks is made and written into it.
- Making alone (the pack off) - the pack is neither read nor changed, the network makes every picture anew and keeps
  none. A model can be tried that way without renaming `uaro-textures.zip`.
- The pack alone - as the players have it. Both off - no AI textures.
- Choosing a set ("Recommended", "All") no longer touches either switch.
- How to check: the pack off, making on, the anime 6B model - the log says `the texture pack is not read` and
  `not kept: the pack's pictures are off`, the textures sharpen; the pack unchanged after it (size and date).

# uaRO Graphics 1.5.7 — testing

- **The AI page's main box was called "Improve textures with a neural network (experiment)"** - it did not say it
  turns all the improved textures on and off, the ready ones of the pack `uaro-textures.zip` too: a tester took it
  off thinking it stops making them, and the pack's pictures went. It is now "HD textures from the texture pack
  (uaro-textures.zip)". Making them in the game is its own box, "Make new ones while playing (debugging)".
- How to check: the AI page - the first box under its new name.

# uaRO Graphics 1.5.6 — testing

- **The AI textures are worked out faster, anime 6B most.** The convolution shader read the network's weights in
  each of a group's 64 threads on its own and waited for the group at every input channel; anime 6B has up to 192,
  and the card worked at 3-4 % of its strength. The group's weights are now read once into the group's memory, four
  channels a pass. The shader is every model's: at the start the card is checked against the CPU, and should it
  differ the AI textures go off, not the game.
- In the game the network still takes only "The card's time a frame" (2 ms) - so the card is hardly busy; for a
  test raise it to 15-20 ms. The pack builder has no such limit.
- How to check: making with anime 6B and with anime - the log says `self-test passed`; in a minute more "made by the
  network" than in 1.5.4; `GPU: ... ms of it AI textures` close to the time set.

# uaRO Graphics 1.5.5 — testing

- **The pack builder (Texture pack page → Make the pack) did not work with anime 6B:** it has its own way through
  the network, which knew only the old kind - anime 6B went through as a compact model, the pictures came out as
  garbage and the card hardly worked. The builder now takes the same tiled way as the game, the work handed to the
  card in parts (a large texture is over a trillion multiply-adds).
- anime 6B pictures the 1.5.4 builder or older wrote into a pack are wrong: delete that pack.
- How to check: Make the pack with anime 6B - the card busy, proper pictures in the pack.

# uaRO Graphics 1.5.4 — testing

- **Making textures nearly stopped in the background.** Behind other windows the game draws 15 frames a second
  (power saving), and the network got its share of each frame - four times less work in the background while the
  card stood idle. With "Make new ones while playing" the network now takes up to 40 ms of each frame there. In
  front - as before, by "The card's time a frame".
- How to check: turn on making, put the game behind another window for a minute - the log's "made by the network"
  grows faster than it did in the background.

# uaRO Graphics 1.5.3 — testing

- **anime 6B works** (1.5.2: the check passed, the textures are made). The start with this model took 10 seconds
  longer: the step-by-step check always ran. It now runs only when the usual check fails (for the log) - the start
  takes about a second.
- How to check: start with "anime 6B" - the log says `self-test passed in ... ms` around 1000 ms, not 10000.

# uaRO Graphics 1.5.2 — testing

- **anime 6B: the graphics card hung at the check's very first step** (0x887a0006 - DXGI_ERROR_DEVICE_HUNG), and the
  game did not start after it. The likely cause: the shader read "no such" stand-in addresses (0xFFFFFFFF) under a
  condition, and the game's compiler may read them before testing it - far past the buffer's end. Every read of the
  shader now stays inside its buffer; what is not wanted is multiplied by zero.
- How to check: as for 1.5.0 - the "anime 6B" model, "Make new ones while playing", Direct3D 12. Should it fail
  again - send the log (the line with `anime-6b`).

# uaRO Graphics 1.5.1 — testing

- **anime 6B: the graphics card's check against the CPU failed** (a difference of 255 of 255), and the game made
  no picture after it. The likely cause: a shader variable named `half`, a type in HLSL - the game's compiler could
  read it otherwise. Renamed. The steps' logic is checked by tests against an exact copy of the shaders in C++, whole
  and in tiles.
- The check now goes step by step first: should the card go its own way somewhere, the log names the step and the
  values (`step N of 96 ... differs: GPU ..., CPU ...`) or the card's stop code (`the graphics card stopped at step`).
- A render target that cannot be made logs the card's state.
- How to check: as for 1.5.0 - the "anime 6B" model, "Make new ones while playing", Direct3D 12. Should the check
  fail again - send the log (the line with `anime-6b`).

# uaRO Graphics 1.5.0 — testing

- **A third model for the AI textures: anime 6B** (RealESRGAN_x4plus_anime_6B, Real-ESRGAN, BSD-3). Made for drawn
  pictures: finer lines, details kept, less of the "plasticine" look. 5-7 times slower than anime. Chosen on the AI
  page in both model lists (the ground and buildings; characters and effects). Direct3D 12 only.
- A pack made before stays as it is: the model shapes new pictures (in the game with "Make new ones while playing",
  or when the pack is built).
- Large textures go in pieces of 96x96 with margins of 10 pixels; small ones (up to 116x116) whole.
- At the start with this model the graphics card is checked against the CPU; should they differ, the log says
  `the GPU's network does not match the CPU's` and the AI textures stay off.
- How to check: move `uaro-textures.zip` away (rename it), choose "anime 6B" on the AI page, turn on "Make new ones
  while playing", the card's time 8-10 ms, start on Direct3D 12 — the textures sharpen step by step. The log says
  `AI textures: on - ... realesrgan-x4plus-anime-6b`. Put the pack back after the test.

# uaRO Graphics 1.4.2 — testing

- **Depth of field: the ground went soft when the character stood under a tree or a roof.** The focus came from
  the depths seen in the middle of the screen, and there a tree's crown, much nearer the camera than the ground,
  took it — all the ground around blurred; a step away and it was sharp again. The focus now comes from the game's
  camera — exactly the point it looks at, the character, whatever covers it. Without the camera (another exe) — as
  before.
- How to check: turn on the depth of field, step under a tree's crown or an awning — the ground around the
  character stays sharp, only the distance is soft.

# uaRO Graphics 1.4.1 — testing

- **uaRO Pad 0.7.0 comes along: talking to NPCs from the gamepad.** D-pad ↓ talks to the nearest NPC (with the right
  stick pushed — to the one it points at). While the NPC's window is open the buttons answer it: A — Next / Close /
  OK, D-pad ↑ ↓ — choose in a menu, B — cancel; a number (how many to buy, store): ↑ ↓ by 1, RB / LB by 10, → ← by
  100. A hint line at the bottom of the screen. The pad presses the client windows' own buttons.
- The Gamepad page has a "Hint in NPC windows" box.
- To update the pad: Gamepad → Update (or update.bat updates a switched-on pad itself).
- How to check: go to a Kafra, D-pad ↓ — her window opens; A pages on, ↑ ↓ choose a line, A takes it; at a shop or
  the storage — the number field. Should something not press — send uaro-pad.log (the `NPC window` lines).

# uaRO Graphics 1.4.0 — testing

- **The gamepad comes along: uaRO Pad 0.6.0 and a new Gamepad page.** The pad travels in the package
  (uaro-graphics\pad\). Switch on puts its dinput.dll into the game's folder; the client's dinput.dll (ROExt) becomes
  dinput8.dll and goes on working, Switch off puts everything back. The game has to be closed.
- **PlayStation controllers (DualShock 4, DualSense) over USB and Bluetooth** — read directly, no DS4Windows or Steam;
  rumble and the light bar. On the page: which controller (any / Xbox / PlayStation), the light bar's colour, what the
  touchpad's click does, vibration, whether the pad is on at the start, its menu's language. Saved to uaro-pad.ini at
  once.
- update.bat updates a switched-on pad together with the graphics.
- How to check: Gamepad → Switch on → connect the controller → start the game: the stick walks, Start opens the pad's
  menu (with a PlayStation controller it names Cross/Circle/Square/Triangle). Change the light bar's colour — after a
  restart of the game it shines in the new one. Switch off — the old dinput.dll is back, the game as before.

# uaRO Graphics 1.3.5 — testing

- **Depth of field: the distance "swam" while walking.** The blurred distance came from the picture made four times
  smaller by averaging 4×4 squares; as the camera followed a walk the picture moved across that grid and the blur
  shook. The reduction is now soft (8×8 with smooth weights) — a pixel's move hardly changes it. Direct3D 12.
- How to check: turn on the depth of field, walk through a town — the far houses evenly blurred, not shaking. Should
  it still swim — tell us (and check whether it goes with the depth of field off).

# uaRO Graphics 1.3.4 — testing

- **Text jumped while typed** (the login, the chat). Crisp text spread its large letters evenly over the whole line
  since 0.41: every new letter changed the line's width and moved the ones already typed a little. Each letter now
  sits in the middle of its own place in the game's line — the typed ones stay, the room after "m" and "w" split
  either side.
- How to check: type the login and a chat message — the letters stay put, the windows' text reads evenly. Should it
  still jump — turn off crisp text on the Quality page and tell us.

# uaRO Graphics 1.3.3 — testing

The auto setup ("Set up for this computer") now sets these too:
- **The interface a whole number of times.** Where it is a fraction now (167 %: the game at 1150×647 on 1920×1080 —
  soft letters), 100 % (up to 1440p) or 200 % (4K) — the crispest letters.
- **NVIDIA RTX:** DLAA on screens up to 2560×1440 (the picture at its own size, smoothed by the network; DLSS Quality
  at 1080p draws the world at 720p and it shows), DLSS Quality on larger ones. GTX and other cards: DLSS untouched.
- **HDR**, when Windows' HDR is on for the main monitor and the card draws with D3D12.
- **Sound:** the new surround engine (Windows 10 and later) and spatial sound for headphones.
- The auto setup's window lists what of these it chose.
- How to check: on a computer with a fractional interface press the auto setup — the window says "The interface at
  100 %", the game then at the screen's own size; on an RTX 3060 at 1080p — "Smoothing: NVIDIA DLAA".

# uaRO Graphics 1.3.2 — testing

- **The settings app started from the uaro-graphics folder.** Opened not from the client folder but from its
  uaro-graphics subfolder, it did not see the game: "Cannot start uaRO.exe (error 2)", "driver unknown", the update
  without update.bat. The app now finds the client in the folder above. With no game there either, the start's error
  says where the app belongs.
- How to check: run uaro-graphics\uaro-graphics.exe — the driver's version shows, "Apply and play" starts the game,
  "Update now" works.

# uaRO Graphics 1.3.1 — testing

- **"Update now" without update.bat.** With a client copied without the install's files, the Service page's button
  said "no update.bat in the game's folder". The app now puts its own update.bat into the game's folder and runs it.
- How to check: delete update.bat from the game's folder, press "Update now" — the file appears, the update runs.

# uaRO Graphics 1.3.0 — testing

- **Music makes way in a fight** (the Sound page, on by default; works without the new engine too). While many
  sounds play at once — a fight, a crowd's skills — the music goes quieter, down to 65 % at 14 sounds and more, and
  comes back over two seconds when the fight is over.
- **The maps' ambient sounds** (birds, water, machines — placed by the maps' makers: 22 thousand sources in 480 maps)
  are played by the game itself; with "Surround sound (new engine)" they now come from their side too, duller far
  off and with the place's echo.
- How to check: in a fight with several monsters or in a crowd with skills the music is quieter and comes back after;
  on a map with ambient sounds (1@gef, towns with fountains) the birds and the water come from their side with the
  new engine.

# uaRO Graphics 1.2.0 — testing

- **Headphones: Windows' spatial sound** (the Sound page, on by default, works with "Surround sound (new engine)").
  When spatial sound is on in Windows — "Windows Sonic for Headphones" (free, built in) or Dolby Atmos / DTS
  Headphone:X — the sounds of monsters, skills and hits go to Windows as objects in space, and Windows itself makes
  them surround in headphones: behind, in front, beside. The place's echo stays. With Windows' spatial sound off —
  as in 1.1 (placed on the speakers).
- To turn it on in Windows: right-click the sound icon → Sound settings → the output device → Spatial sound →
  Windows Sonic for Headphones.
- The log: `spatial sound: Windows spatial sound, N objects - ...` (or `not on in Windows`), and
  `sound engine: the game's sounds placed around the player (Windows spatial sound, ...)`; every minute how many sounds
  went to the spatial sound.
- How to check: in headphones turn your back to a monster — the hits sound behind; turn the camera — the sound moves.

# uaRO Graphics 1.1.0 — testing

- **Surround sound (new engine)** — the Sound page, "Sound through uaRO Graphics", off by default, after the game is
  restarted; Windows 10 and later (Windows 7 keeps the game's sound). The sounds of monsters, skills and hits are
  played by uaRO Graphics itself through XAudio2 instead of Miles:
  - around you on any speakers — stereo, 5.1, 7.1: what is behind sounds behind (the camera's turn counted);
  - far sounds quieter and duller (the highs fade with the distance), at the edge of hearing a sound fades out instead
    of breaking off;
  - the place's echo: caves and dungeons long, houses short, towns a little, fields hardly; the "Echo of the place"
    slider;
  - quieter in the background and the crowd's even loudness — as before.
  The music stays Miles'. Sounds the engine cannot play (not PCM) play as before.
- The log: `sound engine: XAudio2 at ... Hz on N channels ..., reverberation`, then
  `sound engine: the game's sounds placed around the player (stereo|5.1|7.1, the place N)` and a count every minute.
- How to check: turn it on, restart the game; stand with a monster on the left/right/behind — the sound comes from
  there (in headphones left/right; surround for headphones is the next step); go into a cave — the echo; walk away
  from a monster — duller. Should sounds be missing or distorted — turn it off and send a report.

# uaRO Graphics 1.0.2 — testing

- **Monsters and the character blinking with the fade-in.** A sprite already shown was now and then taken for a new
  one and faded in again: when it gained a layer (a weapon in an attack, a head turned), when it was missing from one
  frame or a frame was shown twice. A sprite is now remembered for 0.4 s after it was last drawn, and a new layer
  next to a shown sprite of the same kind is shown at once.
- How to check: turn on the fade-in, hit monsters, walk in a crowd, turn the camera — nobody blinks; monsters coming
  into view at the screen's edge still fade in.

# uaRO Graphics 1.0.1 — testing

- **Fireflies in Direct3D 11.** The fireflies the game draws itself (forests, night maps) are now drawn anew in
  Direct3D 11 too — soft twinkling lights, hidden behind trees and houses, as bright as the slider says. The maps'
  hidden fireflies from their files are not there in D3D11 yet (they need the map read, which D3D12 alone does). The
  log has `fireflies: the game's fireflies drawn anew (Direct3D 11)`.
- How to check: Direct3D 11, turn the fireflies on, go to a map that has them (the woods by Payon at night, say) —
  soft blinking lights instead of the game's squares.

# uaRO Graphics 1.0.0 — testing

- **The effects' glow in Direct3D 11.** The "Glow of effects" switch (with its sliders) now
  works in Direct3D 11 as in D3D12: skills, fire, auras shine with a soft halo. Water under the glow is drawn by its
  own shader. The log has `glow: ...x..., N levels (Direct3D 11)`.
- How to check: Direct3D 11 on the Render page, turn on "Glow of effects", use fire skills or stand by a portal — a
  soft halo
  around the bright effects; the frame counter Ctrl+Shift+F11 before and after (on a weak card the glow costs
  noticeably). Should the picture darken, flicker or effects vanish — turn the switch off and send a report.

# uaRO Graphics 0.99.0 — testing

- **Modern water in Direct3D 11.** The water with waves, reflections and clarity was Direct3D 12's alone; old cards
  (Radeon HD 5000/6000 and the like) showed the game's water. The "Modern water" switch now works in Direct3D 11 too —
  the same shaders, the waves', reflections' and clarity's sliders. No ripples from steps in D3D11 yet. The log has
  `water: the map's water found, drawn anew (Direct3D 11)`.
- How to check: choose Direct3D 11 on the Render page, turn on "Modern water", go to a map with water (Comodo's beach,
  Izlude) — waves and reflections as in D3D12; the frame counter Ctrl+Shift+F11 — what it costs on a weak card. Should
  the water turn black or vanish — turn the switch off and send a report.

# uaRO Graphics 0.98.0 — testing

- **Sun flare** (Effects page, off by default; Direct3D 12). Under the open sky in the day, when the sky shows towards
  the sun (up and to the left, against the way the shadows fall), a soft glow at the screen's edge and a few faint
  coloured rings on the line through the middle. None in dungeons, in towns without sky, or at night.
- How to check: turn it on, in a field turn the camera so that the sky past the map's edge shows at the top left —
  the glow and the rings come; cover the sky (turn towards houses or trees) — the flare goes. Should it be too bright
  — tell us.

# uaRO Graphics 0.97.0 — testing

- **Wet ground in the rain** (Effects page, off by default; Direct3D 12, needs "Weather" on). On maps where it rains
  the ground is a little darker, and here and there puddles mirror the houses, the trees and the characters. The
  puddles keep their places on the map; they do not travel with the camera.
- How to check: turn on "Weather" and "Wet ground in the rain", go to a map with rain — the ground is darker, the
  puddles reflect; maps without rain do not change.

# uaRO Graphics 0.96.0 — testing

- **Flash on a hit** (Effects page, off by default; Direct3D 12 and 11). A monster that is hit lights up white for a
  moment (0.14 s). The hit is known by the sound the game plays where it lands (a weapon's `_hit_...` or the
  monster's `..._damage`), so the game's effect sounds must be on. The log has `effects: a monster hit flashes`.
- How to check: turn it on, hit a monster — it flashes at every hit; monsters next to it do not. Should the wrong one
  flash or none — send a report and a screenshot.

# uaRO Graphics 0.95.0 — testing

- **Cards on the ground sparkle** (Effects page, off by default; Direct3D 12 and 11). A dropped card flashes a small
  golden star now and then (about every 2 seconds, each at its own moment). A card is known by its sprite on the
  ground (every card has the same one, "이름없는카드"); other items do not sparkle.
- How to check: turn it on, get a card to drop (or drop one of yours) — a star flashes over it; nothing over other
  items.

# uaRO Graphics 0.94.0 — testing

- **Ripples from steps** (Effects page, off by default; Direct3D 12, needs "Modern water" on). Where characters and
  monsters walk through water, rings of waves spread (the three newest steps). The steps are now followed without the
  characters' shadows too — footprints in snow and sand work with the shadows off as well.
- How to check: turn on "Modern water" and "Ripples from steps", walk through shallow water (Comodo's beach, say) —
  rings spread behind the character; on land nothing changes.

# uaRO Graphics 0.93.0 — testing

- **Fade in monsters and items** (Effects page, off by default). Monsters, characters, NPCs and items on the ground
  fade in over a quarter of a second where they come into view instead of popping up. Entering a map nothing fades —
  the map is shown at once. Direct3D 12 and 11.
- How to check: turn it on, walk across a field towards the screen's edge — new monsters fade in; turning the camera
  and running, the ones in view do not flicker. The log has `effects: sprites fade in as they come into view`; should
  it say `not this one`, the effect cannot work with this client — send a report.

# uaRO Graphics 0.92.0 — testing

What is new in 0.92.0 — sound (the Sound page, "Sound through uaRO Graphics", after the game is restarted):
- **Left and right.** The client changed only a sound's loudness by distance — a monster on the left was heard from
  both speakers. A sound beside the character now comes from that side of the screen (the camera's turn counted).
  The "Left and right" slider, 60 % by default, 0 = the game's. Should the sides be swapped — set
  `stereo_flip = 1` in `[sound]` of uaro-graphics.ini and tell us.
- **Never lose a new sound.** With every sound channel busy the client simply did not play a new sound. Now the
  quietest (the farthest) stops and gives its channel to the new one.
- **Smooth change of music.** Moving to another map the old piece fades out and the new one in over 1.5 seconds.

How to check:
1. Stand with a monster on your character's left and hit it: the sound is on the left; turn the camera — it moves.
2. Many skills in a crowd: your own hits are heard.
3. Take a portal to a map with other music: the change is smooth, not cut off.

# uaRO Graphics 0.91.1 — testing

- **3D sound without effects.** In 3D mode (setup.exe) the client plays the skills' and monsters' sounds only through
  the "Miles Fast 2D Positional Audio" provider and makes no ordinary sounds; without that provider only the music is
  heard. The driver now makes the ordinary sounds then and the effects play through them. The log's `sound: ...`
  line says which 3D providers there are and what the effects go through.
- Check: set 3D sound, enter the game — both the music and the hits/skills are heard; send a report (the `sound:` line).

# uaRO Graphics 0.91.0 — testing

What is new in 0.91.0 — sound (the Sound page, "Sound through uaRO Graphics", after the game is restarted):
- **Sound quality.** The client mixes its sound at 22050 Hz at most while the music is recorded at 44100 Hz — the highs
  were lost. The sound is now opened at 44100 Hz (or 48000, or the game's).
- **48 sounds at once** whatever setup.exe says (16 or 32 there): skills in a crowd are not cut short.
- **Volume in the background** — with another window in front or the game minimised, 30 % by default (100 = as it
  is, 0 = silence).
- **Even out the loudness in a crowd** — past six sounds at once each is a little quieter, so it does not crackle.
- It works through uaRO.exe's calls of Miles Sound System (`mss32.dll`); the videos (Bink) are untouched. The log
  has `sound: Miles opened at 44100 Hz (the game asked 22050), 48 sounds at once ...`.

How to check:
1. The music in a town sounds brighter than before (compare with "the game's").
2. Many skills in a crowd: no sounds missing, no crackling.
3. Minimise the game or switch to another window — quieter; back — as before.
4. Should the sound be gone or the game crash at start — set "Sound quality: the game's" and send a report.

# uaRO Graphics 0.90.3 — testing

- The Speed preset (the auto setup's for old and small cards) now turns the characters' shadows off too: in a crowd
  they took 5–10 ms of the card's time a frame even on an RTX with Direct3D 11. Check: on a weak computer press the
  auto setup — the characters' shadows are off on the Effects page.

# uaRO Graphics 0.90.2 — testing

- The fast cell under the mouse: in a new map's first 60 picks the client is still setting the map up and answers
  "no cell" with the mouse over the ground. Such disagreements no longer count towards turning it off (the map is
  just checked anew). Check: the log has no `pick: the game's own cell under the mouse from now on` line.

# uaRO Graphics 0.90.1 — testing

What is new in 0.90.1 — the fast cell under the mouse fixed:
- In 0.90.0 the fast pick turned itself off at its first check: the client does not count the ray's hits behind the
  camera (it drops the pieces of the map wholly behind it) and ours did. Now only hits in front of the camera count,
  as the client's.
- One disagreement no longer turns it off: the map is checked anew (240 picks); it is turned off after three. The
  log's line about a disagreement now has both answers' distances.

How to check: after entering a map the log has `pick: the cell under the mouse found the fast way on this map ... ms
a time, the game's own ... ms`; there should be no `pick: ... checked anew` or `from now on` lines — if there are,
send a report.

# uaRO Graphics 0.90.0 — testing

What is new in 0.90.0 — draws merged (Direct3D 11):
- The client draws almost everything as quads of their own: a sprite's layer, a piece of a window, a ground cell —
  each a call of its own. Draws one after another with the same textures and settings, their vertices one after
  another, now go to the driver as one call. The order and the triangles are the same; the picture should not change.
- The gain is the processor's time in the driver on weak computers with old drivers. The card's load does not change.
- The log line `Direct3D 11: N draws a frame in M draw calls ...`: N is the game's draws a frame, M the calls they
  became.
- Switch: `[debug] merge_draws = 0` in uaro-graphics.ini — each draw by itself, as in 0.89.
- Direct3D 12 is unchanged.

How to check:
1. With D3D11 walk around a town and a field, open windows, the inventory, use skills: nothing disappears or flickers,
   sprites and windows are where they belong.
2. Should something be drawn wrong — set `merge_draws = 0`, check whether it is gone, and send a report.
3. The frame counter Ctrl+Shift+F11 before and after; a report with a measurement (Diagnostics → "Measure where the
   game spends its time").

# uaRO Graphics 0.89.0 — testing

What is new in 0.89.0 — weak computers and the cell under the mouse:
- **The cell under the mouse (both renderers).** Every frame the client tested every cell of every piece of the map
  the mouse's ray passes over — thousands of cells when the camera looks far (14–15 % of the game's thread while
  loading and with the mouse far away, as measured). Now the ray goes from cell to cell in order and stops at the
  first one it hits. On each map the first 240 picks are checked against the client's own answer, then every
  120th; at the first disagreement only the client's own way is used until the game is restarted. Switch:
  `[debug] fast_pick = 0` in uaro-graphics.ini.
- **Direct3D 11: textures reused.** The game keeps deleting and creating small textures of the same sizes; an old
  driver takes long to create each. Textures up to 512×512 that the game lets go (32 MB at most) now go to its next
  texture of the same size.
- **Direct3D 11: statistics in the log** every half minute: draws and driver calls a frame (how many were spared), the
  processor's time in drawing, textures made/reused/freed, MB uploaded a second; the card's time for each effect
  (characters' shadows, torches' light, night, colour).
- **Blurry status icons on the right.** When the interface is not enlarged (for example a 1600×900 window without
  scaling), the AI pictures of the interface are not used any more: they were pictures four times larger, shrunk back
  with smoothing — the icons came out soft. The interface's AI pictures are used only with the interface enlarged
  (1.2× and up).

How to check:
1. Move the mouse over the ground far away (camera raised), walk by clicking: the cell cursor and the clicks go where
   they used to. The log has `pick: the cell under the mouse found the fast way on this map ... ms a time, the game's
   own ... ms`. Should `pick: the game's own cell under the mouse from now on` appear — send a report.
2. On a weak computer with D3D11: the `Direct3D 11: ... draws a frame` and `GPU: effects a frame` lines in the log;
   the frame counter Ctrl+Shift+F11 before and after the update; a report with a measurement (Diagnostics → "Measure
   where the game spends its time").
3. In a 1600×900 window without scaling the status icons on the right are as sharp as in the original client.

# uaRO Graphics 0.88.0 — testing

What is new in 0.88.0 - less processor work a frame:
- **Direct3D 11 (small cards such as the Radeon HD 6570).** Before each of the game's draws we set everything anew:
  blending, depth, rasterizer, samplers, textures, vertex layout, buffers and constants - about 12 calls even when
  nothing changed. Now only what differs from the previous draw is set, the constants are written only when they
  change, the vertex buffer is bound once. Old AMD drivers spend noticeable processor time on each call, and on D3D11
  that is the game's own thread.
- **Both renderers.** Setting render and texture stage states (the client does it tens of thousands of times a frame)
  no longer takes the global lock - about 1 ms a frame on a fast processor.
- Should the D3D11 picture come out wrong (wrong textures or blending), set `state_cache = 0` in the `[debug]` section
  of uaro-graphics.ini - everything is set every time as before; and send a report.

How to test: on a small computer with D3D11 - the same place (the prt_fild08 field, Prontera), the frame counter
Ctrl+Shift+F11 before and after updating; a report with the measurement (Diagnostics → "Measure where the game spends
its time").

# uaRO Graphics 0.87.1 — testing

Fixed by a tester's report (Radeon HD 6570, Direct3D 11):
- The Speed preset (which the auto setup also picks on small cards) no longer lowers the game's own texture quality
  (`TEXTUREMODE` = 1, textures at half size). The first game after it crashed within 8 seconds in the game's own sprite
  unpacking code (uaRO.exe 0x417B94). Every preset now keeps the game's textures and sprites at full size
  (`SPRITEMODE` = 2, `TEXTUREMODE` = 2) - our textures fit a 2 GB card, they are kept compressed.

How to test: on a small computer "Service" → "Update now", then "Renderer" → "Set up automatically" → "Apply", start
the game and enter a map.

# uaRO Graphics 0.87.0 — testing

What is new in 0.87.0:
- **Measuring where uaRO.exe spends its time** (Settings → Diagnostics → "Measure where the game spends its time";
  applies at once, in a running game too). About a thousand times a second the driver notes where the game's thread
  is and every 10 seconds writes into the log:
  - `profile: N samples in 10 s - uaRO.exe X%, ntdll.dll Y%, ...` - the time by module (the waits show too);
  - `profile: uaRO.exe's busiest places - 0x412cc0 7.1%, ...` - the client's own busiest places;
  - `profile: called from uaRO.exe - 0x40e0d0 into ddraw.dll 5.0%, ...` - where the client goes into other modules.
  It slows the game a little - turn it off after measuring.

How to test: turn it on, play a minute in a town with people, change a couple of maps (loading), fight monsters with
skills; turn it off and send the log. The addresses are matched to the client's decompilation.

# uaRO Graphics 0.86.0 — testing

What is new in 0.86.0 (Direct3D 12) - less load:
- **Floor reflections at half the size**: a quarter of the rays, then laid over the picture smoothly (on a floor a
  reflection is soft anyway). Log: `floor reflections: on <map>, at WxH`; the cost - `floor reflections` in the
  `GPU: effects a frame` line.
- **The driver's processor time** every half minute: `CPU: N ms a frame in the driver's drawing` - what the processor
  spends on the drawing and the effects (not the waits).
- The AI textures on D3D12 already go out of video memory when it runs short (since 0.44); the compression stays on
  D3D11, where memory is small.

How to test: Prontera's church or Glast Heim - the reflections in place, not smeared on the walls; send a minute of the
game's log (the GPU and CPU lines).

# uaRO Graphics 0.85.0 — testing

What is new in 0.85.0 - less load:
- **60 frames by default** (Frames → "fps_limit"). Before, the game drew as many as it could (140-150 on the
  developer's), though it looks the same. The limiter is exact now: a steady 60 without stutter (Sleep woke up to
  16 ms late). **An installed game** has `fps_limit = 0` in uaro-graphics.ini - set 60 on the Frames page or press the
  auto setup.
- **Saving power in the background** (Frames → "Save power in the background"; on by default): while another window
  is in front or the game is minimised - 15 frames; on the login and the character select - 30.
- **The auto setup and the presets** no longer set render_scale over 100 % (150 % at 4K is 2.25 times the pixels for
  slightly smoother edges); every preset sets 60 frames and the power saving; the Speed preset turns the costly effects
  off (soft shadows, depth of field, glow, clouds, heat haze, floor reflections and more).

How to test: the frame counter Ctrl+Shift+F11 - a steady 60; switch to another window and back; the log's
`GPU: ... ms of work a frame` over a minute - compare with before.

# uaRO Graphics 0.84.0 — testing

The picture by default is as before 0.77 ("the lighting effects are too much"): off by default now - the shadows of
houses and trees, the bloom of bright places, film-like colours, the regions' colours, footprints and dust,
thunderstorms, the skills' sparks, the smooth map change and the light in the air (the halo about fire and skills). The
round characters (0.79) are apart from the rim light - their own box "Round characters", off as well. The code stays:
any of them comes back with its box on the Effects page.

One turned on by hand in the settings app is written in uaro-graphics.ini and stays on - clear its box.

How to test: the same square - it should look as before 0.77 (no big shadows on the tiles, no halo by Blessing).

# uaRO Graphics 0.83.1 — testing

Fixed by the square's screenshot (0.83.0): the sunlit tiles went nearly white.
- **Bloom of bright places** glowed from any light surface (threshold 0.72) and strongly - a light square glowed whole.
  Now only what is near white (0.88), 2.3 times weaker.
- **Ground relief** brightened the stones' sides facing the light up to x1.3 - light tiles went white. Now at most
  x1.1, the relief mostly from the shading.

How to test: the same square by day - the tiles light but with the stones' pattern visible; Pause - compare.

# uaRO Graphics 0.83.0 — testing

What is new in 0.83.0 (Direct3D 12):
- **Smooth map change** (Effects; on by default). After a warp or entering a new map the world comes in from black
  over 0.7 seconds; the interface shows at once.
- **Film grain** (Effects → "Film grain" and strength; off by default). A fine noise over the picture as on film,
  weaker in the bright parts.

How to test: several warps - any black flashes or the world staying dark; the grain - turn it on, strength 30-60.

# uaRO Graphics 0.82.0 — testing

What is new in 0.82.0 (Direct3D 12):
- **Sparks of skills** (Effects → "Sparks of skills" and their brightness; on by default; needs the skills' light).
  Embers rise from burning skills (Fire Wall, Fire Bolt, Meteor Storm), swaying and dying out; glints of their colour
  spread from the other bright skills (Soul Strike, Heal, lightning).

How to test: Fire Wall and Meteor Storm, Soul Strike, Heal - are the sparks visible, too many or too bright; do sparks
hang where the skill is gone.

# uaRO Graphics 0.81.0 — testing

What is new in 0.81.0 (Direct3D 12):
- **Grass** (Effects → "Grass" and how thick; **off** by default - the costliest effect). Blades of grass grow on the
  green ground of the fields and towns: they stay put as the camera moves, sway in the wind and lie down where
  characters just walked (with "Footprints and dust"). Characters stand over the grass. Log: `grass: on <map>, blades
  up to N pixels`; the cost - `grass` in the `GPU: effects a frame` line.

How to test: turn it on, a Prontera or Payon field; does it shimmer, does grass grow on green walls and roofs, what it
costs (the frame counter).

# uaRO Graphics 0.80.0 — testing

What is new in 0.80.0 (Direct3D 12):
- **Footprints and dust** (Effects → "Footprints and dust"; on by default; needs the characters' shadows). Characters
  and monsters leave footprints in the snow (Lutie and other snowy maps, ~25 s) and in the sand (Morroc, ~14 s), left
  and right by turns; on dry fields little puffs of dust rise under the feet. Log: `footprints: in the snow on xmas`.
- **Thunderstorms** (Effects → "Thunderstorms"; on by default). Where it rains (Niflheim, Einbroch, with the weather
  on), lightning flashes now and then - two flashes in a row, the picture lit for a moment.

How to test: run through Lutie and Morroc's fields - are the prints visible, do any hang on walls; a Prontera field -
the dust; Niflheim - lightning about every 10-20 seconds.

# uaRO Graphics 0.79.0 — testing

What is new in 0.79.0:
- **Round characters (HD-2D)** - with "Rim light" (Direct3D 12 and 11). Torches, lamps and fire light a character or a
  monster as if it were round: the side towards the fire brighter, the far one darker, smoothly across the sprite's
  width instead of evenly.
- **Colours of each region** (Effects → "Colours of each region" and strength; on by default; D3D12 and D3D11). Payon
  warm and green, Niflheim cold and pale, Morroc sun-bleached, Lutie blue, Glast Heim grim, Einbroch smoky, and a dozen
  more. Log: `region: Payon (pay_fild01)`.

How to test: a torch next to a character (Payon's caves) - walk past the fire; travel the towns: is the colour too much
(strength 0-100), which regions want changing.

# uaRO Graphics 0.78.0 — testing

What is new in 0.78.0 (Direct3D 12):
- **Bloom of bright places** (Effects → "Bloom of bright places" and its strength; on by default, 60 %). The brightest
  parts of the picture - sunlit tiles, snow, windows, fire - glow softly around (before, only the effects glowed).
- **Film-like colours** (Effects; on by default). What is bright (with the bloom) rolls off softly into white instead
  of clipping into a flat patch; a light contrast, the middle tones stay as they were.
- In the `GPU: effects a frame` line - `bloom`.

How to test: Lutie (snow), Morroc by day, fires at night; is it too soft or bright; Pause - compare; if the picture got
darker or more contrasty than you like, turn "Film-like colours" off and tell.

# uaRO Graphics 0.77.0 — testing

What is new in 0.77.0 (Direct3D 12):
- **Shadows of houses and trees** (Effects → "Shadows of houses and trees" and their darkness; on by default, 60 %).
  On the fields and in the towns houses, walls and trees cast shadows on the ground away from the sun (as the
  characters' do). With "Time of day" they turn and lengthen towards the evening; at night there are none. Dungeons and
  houses are unchanged. Log: `sun's shadows: over <map>`; the cost - `sun's shadows` in the `GPU: effects a frame` line.

How to test: Prontera, Payon (trees), Alberta; do they double with the map's baked shadows, are there stripes or noise
on the ground; turn the camera - the shadows must stay put by the houses.

# uaRO Graphics 0.76.0 — testing

What is new in 0.76.0:
- **Keep the frame rate** (Settings → Frames → "Keep the frame rate", 0 = off). When the game runs slower than this
  for 10 seconds (under 90 % of it), the costliest effect is turned off for the rest of the game, ten seconds later
  the next if needed. On D3D12 by the graphics card's measures, on D3D11 by a list. The settings file is not changed;
  restarting the game brings everything back. Log: `keep_fps: 38 fps, under 45 - effects.ao off for this game (it cost
  1.20 ms)`. The auto setup sets 45 on small and middle cards.
- **What the effects cost, in the settings app.** The game (D3D12) writes every half minute how many milliseconds each
  effect cost (`uaro-graphics\effect-costs.txt`), and the Effects page shows it next to the box: "Reflections on the
  floor · ~0.8 ms". The sky, the light in the air and the floor reflections now have their own measures in the log
  (`GPU: effects a frame`).
- **Shader safety.** If the shader compiler on someone's computer will not take the newest parts of the main shader
  (soft effects, ground relief), the driver builds it without them instead of showing no picture. Log:
  `shader draw_ps: compiled without the newest features ...` - send that line if you see it.

How to test: set "Keep the frame rate" well above your frame rate (say 500), play a minute - the log turns effects
off one by one; set 0 - they come back. After the game open the Effects page - the numbers by the boxes.

# uaRO Graphics 0.75.0 — testing

What is new in 0.75.0 (Direct3D 12):
- **TAA for any graphics card** (Settings → Smoothing → "TAA (any graphics card)", off by default). The renderer's own
  temporal anti-aliasing for cards without DLSS (AMD, Intel): the world is drawn with a sub-pixel jitter and each frame
  is blended with the last along the camera's motion - edges, fences, grass and thin lines stop shimmering. The history
  is held within the colours of the pixels around, so moving characters leave no trail. DLSS goes first; MSAA and
  render_scale are not used with TAA. Applies at once. The auto setup turns it on for strong and middle cards that are
  not NVIDIA's (middle ones up to 2560x1440). Log: `TAA: on, WxH`, `TAA: the game's camera found`.

How to test: turn TAA on (an NVIDIA card can too, with DLSS off), stand and walk on a field with a fence and grass,
turn the camera; are there trails behind the characters, a shaking picture, blurred text (the interface is not
smoothed). The frame counter - what it costs.

# uaRO Graphics 0.74.0 — testing

What is new in 0.74.0 (Direct3D 12 and Direct3D 11):
- **Time of day** (Effects → "Time of day", off by default). By the computer's clock the sun moves: it rises on the
  right at 6:00 and sets on the left by 20:00. The characters' shadows turn with it - in the morning they fall left, in
  the evening right, long in the morning and the evening, short at noon. The light is pinkish in the morning, white by
  day, golden in the evening. With "Night by the clock" the evening turns into night. The rays from the sky (0.73) come
  from the sun.

How to test: turn it on, look at the characters' shadows and the colour (a field by day); set the Windows clock to
7:00, 13:00, 19:00 and re-enter the map - do the shadows and the colour change.

# uaRO Graphics 0.73.0 — testing

What is new in 0.73.0 (Direct3D 12):
- **Light in the air** (Effects → "Light in the air" and its strength; on by default). Around torches, lamps and
  burning skills the air glows as in a light mist; a wall or a pillar in front of the fire hides the glow. At night it
  shows more. On the fields and in the towns, where the edge of the map shows, rays of light stream from the sky - from
  the sun, up and to the left.

How to test: caves with torches (anthell, pay_dun), Geffen at night, Fire Wall in the dark; the edge of a field for the
rays. Is it too bright, does it flicker.

# uaRO Graphics 0.72.0 — testing

What is new in 0.72.0 (Direct3D 12):
- **Reflections on the floor** (Effects → "Reflections on the floor" and the floor's shine; on by default, shine 35 %).
  Indoors - dungeons, castles, houses - and in the rain (Niflheim, Einbroch) the floor reflects the characters, the
  monsters and what stands on it; more in the rain. Dry fields and towns do not shine. Log:
  `floor reflections: on <map>`.

How to test: Glast Heim (gl_church, gl_cas01), Prontera's church (prt_church), Niflheim; does it slow down (the frame
counter Ctrl+Shift+F11), are there doubled stripes on the walls.

# uaRO Graphics 0.71.0 — testing

What is new in 0.71.0 (Direct3D 12 and Direct3D 11):
- **Ground relief** (Effects → "Ground relief" and its strength; on by default). The brightness of the ground's
  picture is taken as height: cracks, stones and grass are lit from the side (light from the screen's upper left) and
  look raised. Best with the AI textures - they have more detail. Walls and models are unchanged.

How to test: Prontera's road and tiles, Morroc's sand, a dungeon's stone floor; strength 0-200 - where it starts to
look dirty. Pause - compare.

# uaRO Graphics 0.70.0 — testing

What is new in 0.70.0 (Direct3D 12):
- **Motes in the air** (Effects → "Motes in the air" and how many; on by default). Where the map has no weather of its
  own, tiny motes drift slowly in the air, now catching the light, now not: pollen over the fields and the towns (less
  of it at night), dust in the dungeons and the houses. Log: `weather: pollen over <map>` or `dust`.

How to test: Prontera and its fields by day, any dungeon; are the motes in the way, too many (the slider).

# uaRO Graphics 0.69.0 — testing

What is new in 0.69.0 (Direct3D 12):
- **Sky past the map's edge** (Effects → "Sky past the map's edge", clouds and strength; on by default). On the fields
  and in the towns the void past the edge of the map (black or the haze's colour) now shows a sky: the map's haze
  colour at the horizon, blue higher up, clouds drifting over it; looking down past the edge, a sea of clouds. At night
  (the Night effect) the sky is dark with stars. Dungeons and houses are unchanged. Log: `sky: over <map>`.

How to test: the edge of a field (prt_fild08, pay_fild01 by the edge), zoom the camera out and turn it so the void
past the map shows; at night - the stars. Pause - compare.

# uaRO Graphics 0.68.0 — testing

What is new in 0.68.0 (Direct3D 12 and Direct3D 11, with the torches or the lamps on):
- **Rim light on characters** (Effects → "Rim light on characters" and its strength, on by default). The light of
  torches, lamps and burning skills fell on the characters before, but a flat sprite was lit evenly. Now the edge of a
  character (and of the models) that faces the fire is lit more - near a fire the sprites look rounder.

How to test: stand by a torch or a lamp (Payon, the caves, Geffen at night), walk around the fire; Fire Wall next to
monsters. Are there bright outlines where there is no fire.

# uaRO Graphics 0.67.0 — testing

What is new in 0.67.0 (Direct3D 12 and Direct3D 11):
- **Soft effects at the ground** (Effects → "Soft effects at the ground", on by default). Skills, sparks and portals
  facing the screen were cut along a straight line where they went into the ground or a wall. Now they fade softly
  over the last few steps before the surface. The characters are not touched - their feet stand on the ground as
  before.

How to test: skills by a wall and on the ground (Fire Wall, Soul Strike next to a wall, Warp Portal) - is there a
hard line; Pause - compare with the old look.

# uaRO Graphics 0.66.0 — testing

What is new in 0.66.0 (Direct3D 12):
- **The characters' pictures first.** The texture pack's pictures were read in the order asked for: on a new map the
  hundreds of its textures known ahead kept the characters' frames and the icons waiting behind them (blocky for a
  moment). Now one at a time, the most urgent first - the sprite frames and the icons, then the textures in view, then
  those known ahead; a texture asked for ahead and drawn meanwhile moves up. As Direct3D 11 does since 0.49.1.
- The log says how long the frames wait: `sprite frames - ... read in N ms on average, M at most`.

How to test: Direct3D 12, a town with many players, then another map: are the characters improved at once; send the
log.

# uaRO Graphics 0.65.0 — testing

What is new in 0.65.0:
- **The waterfalls of Brasilis** (Direct3D 12, with the modern water on): bra_fild01's map file has some forty
  waterfalls along its cliffs that uaRO.exe never shows (effect 821) - now each is a sheet of falling water with
  streaks running down and foam at its foot, hidden behind what stands nearer. The log:
  `waterfalls: the map file's drawn (N in view)`. Their width and which way they face are guessed from the map file:
  send a screenshot.
- **HDR** (Settings → Quality → HDR, off by default; Direct3D 12): on a monitor with Windows' HDR on, the picture in
  HDR10 - white at 200 nits, fires and sparks a little brighter than white. The log: `HDR: on ...`, or why not.
  Not with DLSS frame generation; Ctrl+Shift+F12 takes no screenshot while it is on.

How to test: bra_fild01 (Brasilis' field) with water on - the cliffs; HDR on with Windows' HDR on - the log and the
look (torches, skills).

# uaRO Graphics 0.64.0 — testing

What is new in 0.64.0:
- **The rain's splashes** (Direct3D 12, with the weather on, where it rains - Niflheim, Einbroch): rings spreading
  where the drops land, fixed on the ground (not on the screen), on what faces up only - not on walls.
- **Leaves in the wind** (Effects → "Leaves in the wind", off by default; Direct3D 12 and 11): the leaves of trees and
  bushes - textures that are green with holes - sway a little, their tops more than their trunks; fences and walls
  stay still.

How to test: Niflheim with the weather on - the splashes on the paths; Payon or Prontera's fields with "Leaves in the
wind" - the trees; tell if anything that is no leaf moves.

# uaRO Graphics 0.63.0 — testing

What is new in 0.63.0:
- **/aura at once (off).** The game reads /aura only when it makes an aura (a map entered): now the level 99 aura's
  ring and bubbles go the moment /aura is off; its circles stay, as the game shows them. On again: the game brings
  the ring back as it makes the aura anew (another map, the player out of view and back) - that it does itself.
- **Night** (Effects → Night, strength, "By the clock"): the world darker and colder, the torches, lamps, skills and
  fireflies shining out of it; the interface as it is. By the clock: only 20:00-6:00 by the computer's time, with an
  hour of dusk and of dawn. Direct3D 12 and 11.

How to test: a level 99 character, /aura - the ring at once off; Night on in Prontera, then in a dungeon with
torches.

# uaRO Graphics 0.62.0 — testing

What is new in 0.62.0 (a test, off by default):
- **Settings → Quality → "Interface at the screen's pixels"** (`[display] ui_native`): with a render scale over 100 %
  on Direct3D 12 without DLSS, only the world is drawn larger and made smaller; the windows and the text are drawn at
  the screen's own pixels - so they are not softened by the shrinking (150 % shrinks by a step that is no whole
  number). The way DLSS already draws them, with the world shrunk instead of upscaled. MSAA is not needed then
  (the world is supersampled). The log: `supersampling (the interface at the screen's pixels): the world at ...`.

How to test: Direct3D 12, DLSS off, render scale 150 %; the box on and off (restart the game): the windows' text,
item icons, the chat; the frame counter (Ctrl+Shift+F11).

# uaRO Graphics 0.61.0 — testing

What is new in 0.61.0 (Direct3D 11, part 2):
- **The characters' shadows on Direct3D 11** (Effects → characters' shadows): each character and monster casts its
  sprite's soft shadow on the ground, as with Direct3D 12 - lit by the sun alone (Direct3D 12 also turns them away
  from the map's lamps). The log: `characters' shadows: N characters ... (Direct3D 11, the sun's light)`.
- **The torches' light on Direct3D 11** (Effects → torches, their light): each torch lights the ground about it, warm
  and flickering a little; the flame and its glow stay the game's (Direct3D 12 draws them anew). The log:
  `torches: N lit (Direct3D 11 ...)`.
- With either on, Direct3D 11 now knows the sprites' kinds without AI textures too.

How to test: Direct3D 11; a field with monsters - their shadows; a dungeon with torches (Prontera's culverts, Glast
Heim) - the light round them; Pause to compare.

# uaRO Graphics 0.60.0 — testing

What is new in 0.60.0 (one file for a player):
- **uaro-graphics-setup.exe** (release/, next to the zip): the whole release in one program. Put it in the uaRO folder
  and run it - or run it anywhere and choose the uaRO folder; it unpacks uaRO Graphics there, runs install.bat
  without its questions, says what is next and opens the settings app: "Set up automatically", then "Download the
  pack", then the game. In English, Ukrainian or Russian by Windows' language.
- uaro-graphics.zip and update.bat stay as they were.

How to test: on another computer (or a copy of the client) run uaro-graphics-setup.exe; then the two presses.

# uaRO Graphics 0.59.0 — testing

What is new in 0.59.0:
- **The Effects page's colours on Direct3D 11** (brightness, contrast, saturation, warmth, vignette): drawn over the
  world before the interface, as with Direct3D 12 - the first of the Effects page on Direct3D 11. The others (lights,
  torches, water, shadows, weather, glow) still need Direct3D 12; the log says so.

How to test: Direct3D 11, Effects → colours on, saturation 150 %, vignette 60 %: the world changes, the windows not;
Pause shows the game's own.

# uaRO Graphics 0.58.0 — testing

What is new in 0.58.0 (an upscale for AMD and Intel):
- **Render scale under 100 %** (Settings → Quality, 50-200 %): on a large screen a weaker card draws the world smaller
  than the picture, and it is made larger sharply when shown - a Catmull-Rom filter and CAS sharpening, the way FSR 1
  does it (our own filter, not AMD's code; any card, Direct3D 12 and 11). DLSS stays NVIDIA's.
- "Set up automatically": a middle card on a screen past 2560x1440 gets 75 %, a small or old one past 1920x1200 67 %,
  with sharpening.

How to test: on a 4K (or 1440p) screen set 67 % and 100 %: the frame counter (Ctrl+Shift+F11) and the look.

# uaRO Graphics 0.57.0 — testing

What is new in 0.57.0 (weak graphics cards):
- **AI textures compressed in video memory** (Direct3D 11): BC1 for the opaque pictures (an eighth of the memory),
  BC3 for the ones with alpha (a quarter), made on the reading thread as they load. Settings → AI textures →
  "Compress in video memory": Automatic (a card under 3 GB - the old Radeons, integrated graphics), On, Off
  (`[ai] compress`). The log says "AI textures: kept compressed in video memory".
- "Set up automatically" turns the world's AI textures on from 1 GB of video memory (was 2 GB).

How to test: Direct3D 11, "Compress in video memory" On: the textures look the same at a glance, the log's "copies"
line shows a fraction of the MB of before; Off for comparison.

# uaRO Graphics 0.56.0 — testing

What is new in 0.56.0:
- **Texture pack → "Clothes colours"** (off by default): the builder makes the characters' bodies in every clothes
  dye too (`data\palette\몸\`), so players who dyed their clothes are improved as well - 4 to 9 times the bodies'
  pictures, a bigger pack and a longer make. "Make the pack" adds only what is missing.

How to test: tick "Clothes colours", "Make the pack"; in a town, players with dyed clothes smooth.

# uaRO Graphics 0.55.0 — testing

What is new in 0.55.0 (profiles):
- **Settings app → Service → "Profiles".** "Save as a profile" keeps every setting of uaro-graphics.ini (the changes
  not applied yet too) under a name - "Maximum for 4K", "Weak laptop", "Recording"; "Load" makes it
  uaro-graphics.ini again (a backup of the present one first, the app's language kept); "Delete". Profiles live in
  `uaro-graphics\profiles\`, so a profile can be handed to another player as a file.

How to test: save two profiles with different settings, load each in turn, start the game.

# uaRO Graphics 0.54.0 — testing

What is new in 0.54.0 (hotkeys):
- **Ctrl+Shift+F11 - the frame counter**: "143 fps · 7.0 ms (16 max) · Direct3D 11" in the game window's top left
  corner, twice a second; again to hide it. Settings → Frames → "Frame counter" shows it from the start
  (`[frames] fps_counter`). It is a small window of its own over the game: the same with Direct3D 12, 11 and DLSS.
- **Ctrl+Shift+F12 - a screenshot at the picture's full size** (3840x2160 on a 4K screen, not the game's 1920x1080)
  as PNG into the game's ScreenShot folder (`uaro-YYYYMMDD-HHMMSS.png`); the log says where.
- Pause or Scroll Lock stays the before/after key.

How to test: in the game Ctrl+Shift+F11, Ctrl+Shift+F12 (Direct3D 11 and 12); look at ScreenShot.

# uaRO Graphics 0.53.0 — testing

What is new in 0.53.0 ("download the pack from the site"):
- **Settings app → Texture pack → "Download from uaro.kiev.ua".** The pack the developers made, nothing to make on
  the player's computer: the settings app reads the pack on the site (its list of pictures, by HTTP ranges) and fetches
  only the pictures this computer lacks, each checked (CRC-32) as it comes and written into uaro-textures.zip at once.
  "Stop", a closed app or a lost connection keep what came - "Download the pack" again goes on from there; a newer pack
  on the site brings only its new pictures. Pictures the player's own game made stay, and both indexes are merged.
- The progress: pictures, MB, speed and the minutes left.
- For the developers: the pack goes to `/var/www/uaro.kiev.ua/web/files/graphics/uaro-textures.zip` on the server
  (https://uaro.kiev.ua/files/graphics/uaro-textures.zip).

How to test: put the pack on the site; on another computer (or after renaming uaro-textures.zip) press "Download the
pack", stop in the middle, press again - it goes on; then start the game.

# uaRO Graphics 0.52.0 — testing

What is new in 0.52.0 (reports, part 2):
- **A crash says where.** When the game crashes, the log has `crash: exception 0x... at <module>+0x...` and
  `uaro-graphics\crash.dmp` keeps a small minidump; the report (by hand or automatic) takes it along, and the site's
  admin offers it for download (WinDbg / Visual Studio).
- **The pictures the pack has none of** (`uaro-graphics\missing\`, up to 64) go with the report; the admin shows
  them in the report's card.
- The report's "Problem" lines are in English whatever the settings app's language.
- The site's admin has a "Зведення" tab: reports by reason, API, version and graphics card, those with errors.

How to test: send a report from Diagnostics; in the admin, the missing pictures show in its card.

# uaRO Graphics 0.51.1 — testing

What is new in 0.51.1 (from the first report, R-4ZK5KR):
- **The real video memory.** Windows tells a 32-bit program (the settings app) no more than 3072 MB: the report said
  "at least 3072 MB" of a 16 GB card, and "Set up automatically" would have taken it for a small card (under 4 GB).
  Now the number comes from the display driver's own key in the registry (16 GB is 16 GB); where there is none, the
  automatic setup counts such a card as a middle one, not a small one.

How to test: Diagnostics shows the card's full memory; "Set up automatically" says "a strong graphics card".

# uaRO Graphics 0.51.0 — testing

What is new in 0.51.0 ("logs and settings to the site, to debug for each device"):
- **Settings app → Diagnostics → "Report to the developers".** "What happened" (a line of your own) and "Send the
  report": the graphics card(s), Windows, monitors, uaro-graphics.ini and the logs' ends (uaro-graphics.log,
  uaro-render64.log, uaro-pack.log) go to uaro.kiev.ua - no login, no character, the Windows user name hidden. The
  window answers with the report's code (R-XXXXXX, copied): name it to the developers. The codes sent are listed.
- **"Send a report by itself when something goes wrong"** (off by default; `[report] auto` in uaro-graphics.ini,
  from the next start of the game): the driver starts the settings app hidden as a watcher of the game; after the game
  it reads the log and sends a report only when the renderer would not start, the graphics card was lost, or the game
  closed without closing the renderer - at most one in 6 hours.
- The site keeps the reports 90 days; the developers see them in its admin ("Звіти графіки").

How to test: Diagnostics → write a line → "Send the report" → a code comes; tick the automatic one, Apply, play.

# uaRO Graphics 0.50.2 — testing

What is new in 0.50.2 ("can the card be chosen somewhere?"):
- **Settings app → Renderer → "Graphics card"**, shown when the computer has several: "Automatic: <the fast one>"
  or any card by name (with its video memory). The game, the 64-bit renderer and the texture pack's builder draw on
  it; "Set up automatically" looks at the card chosen there. A card that is gone shows "(not found)" and the fast
  one is taken. In uaro-graphics.ini: `[renderer] gpu = auto` or the card's name; the log says
  `graphics card chosen: ...`.

How to test: on a computer with two cards - choose each in turn, start the game, check the log's
`Direct3D 1x renderer on ...` line names it.

# uaRO Graphics 0.50.1 — testing

What is new in 0.50.1 ("there can be strong cards of other makers, and several cards"):
- **Several graphics cards: the game is drawn on the fast one** - a laptop's NVIDIA or AMD, not its integrated
  Intel; Windows' own choice for high performance (Settings → System → Display → Graphics, where a player can pick
  the card for uaRO.exe) comes first, else the card with the most video memory. Direct3D 12 and 11 alike, and the
  automatic setup looks at that same card. The log names them: `graphics cards: ... - Direct3D 11 on ...`.
- Strong cards of AMD and Intel get the same settings as NVIDIA's of their memory (DLSS is NVIDIA's alone).

How to test: on a laptop with two graphics cards - press "Set up automatically" (it should name the fast card), play,
send uaro-graphics.log.

# uaRO Graphics 0.50.0 — testing

What is new in 0.50.0 ("a button to hand the client to other people"):
- **Settings app → Renderer → "Set up automatically".** It looks at the graphics card, its video memory and the
  screen and picks the settings, says what it found and offers to write them and start the game:
  - no graphics driver (Windows' basic display adapter): the game's own look, and a word to install the driver;
  - a card without Direct3D 12 (Radeon HD 6000 and the like): Direct3D 11, the game's size, no smoothing;
  - Direct3D 12 under 4 GB: the light settings, the heavy effects off; 4 to 8 GB: balanced; 8 GB and more: drawn at
    200 % (150 % past 2560x1440);
  - DLSS and frame generation off on cards that are not NVIDIA's;
  - AI textures on when uaro-textures.zip lies next to uaRO.exe (under 2 GB: characters, monsters, items and icons
    only); the game never makes pictures itself.

How to test: press the button on your computer (expected: "a strong graphics card", 150 %, AI textures on) and on
the other computers; send what the window said and each one's uaro-graphics.log after a game.

# uaRO Graphics 0.49.4 — testing

What is new in 0.49.4 ("rebuilt the pack - other players' hair still blocky"):
- **Fixed: the hair colours were never made.** The builder opened the game's archives for pictures only, so it saw
  none of the palettes (`data\palette\...`) - 0.49's hair colours came out empty, and the pack stayed at the same
  number of pictures.

How to test: update; Texture pack → "Make the pack" (it only adds what is missing: the hairstyles in every colour) -
the number of pictures should grow. Then Prontera: other players' hair smooth, `uaro-graphics\missing` without new
`player-...` files; send the log.

# uaRO Graphics 0.49.3 — testing

What is new in 0.49.3 ("the character facing us stays blocky; walking, the improved frames show"):
- The game caches a sprite frame once and draws it from its cache until the cell is needed for another frame. A frame
  that got there past the place the driver watches (say, before the cache was known to be one) stayed blocky all that
  time. Now the driver keeps a copy of every sprite cache and finds **every cell that changed**: such a frame is looked
  up in the pack like the rest. A frame the game writes again unchanged is no longer drawn over.
- Both Direct3D 11 and Direct3D 12. The log says how many such frames came:
  `sprite frames - N came past the game's call (found changed in their caches), M of them in the pack`.

How to test: update, start the game - the character standing facing us, at the start and after a while; send the log.

# uaRO Graphics 0.49.2 — testing

What is new in 0.49.2 (Direct3D 11: "at the start the character is blocky, after a while it is fine"):
- A sprite frame waiting for its picture from the pack is shown **smoothly enlarged** meanwhile, not texel by texel
  (that was the blocky look) - also the first frames of a new sprite cache, which went in as one blocky piece.
- The log says how long the frames wait: `sprite frames - ... from the pack (waited N ms on average, M at most)`.

How to test: update, start the game on Direct3D 11 - the character at the start; send the log.

# uaRO Graphics 0.49.1 — testing

What is new in 0.49.1 (Direct3D 11: "characters are blocky at first and get rendered after a while"):
- **The characters first.** The pictures of the sprite frames and the windows' icons are read before the textures,
  and a texture on screen before the ones known ahead (a map's hundreds as it loads) - the frames waited behind them.
- **Frames read lately stay in memory** (48 MB): an animation shows the same frames again and again - at once now.
- **What the pack has no picture of is saved** in `uaro-graphics\missing\` (the first 64 of a run, as BMP): send that
  folder - from it the developers see why (the hair, say).

How to test:
1. update.bat (or Settings → Service → Update now). If not yet done with 0.49: Texture pack → "Make the pack" (the hair
   colours, the .jpg pictures).
2. Direct3D 11, a town with many players: do the characters turn improved at once, or after how long?
3. If something stays blocky: send uaro-graphics.log and the uaro-graphics\missing folder (zipped).

# uaRO Graphics 0.49.0 — testing

What is new in 0.49.0 (after "the hair and the loading screens are not upscaled"):
- **Hair in every colour.** The game draws a head with its hair colour's palette (`palette\머리\...`), so its frames
  were not the pack's. The builder now makes every head with every hair palette of the game's files.
- **The .jpg pictures: the loading screens and the water.** The builder reads them now (it read only BMP, TGA and SPR).
  The game decodes .jpg with its own ijl15.dll, a level or two apart from ours, so these pictures are found by their
  likeness (a small copy kept in the pack's index), not by their exact key.
- **Updates from the settings app:** Service → Updates → "Check for updates" asks GitHub for the newest version;
  "Update now" starts update.bat and closes the app.

How to test:
1. update.bat (or Update now in the settings, once 0.49 is installed).
2. Texture pack page → "Make the pack": it goes on with the pictures already made and adds the hair colours and the
   .jpg ones. Send the speed and the time it shows.
3. In the game: hair of different colours, the loading screens, water - improved (Pause to compare). In the log
   `not in the pack - ...` should show fewer.
4. Service → Updates → Check for updates: it shows 0.49.0 as the newest.

# uaRO Graphics 0.48.0 — testing

What is new in 0.48.0:
- **upscaled.grf is gone: the AI textures come from the texture pack uaro-textures.zip alone** (Direct3D 12 and
  Direct3D 11). The old upscaled.grf is not used any more; the Ready textures page offers to delete it.
- **Making new ones while playing** (Direct3D 12, a debugging aid) writes the pictures **into uaro-textures.zip**,
  with their kind and the maps they were seen on in the pack's index. Only one writer at a time: while the pack's
  builder runs, the game does not write (and the other way round).
- **Check the keys** is gone from the Texture pack page. Instead the game's log says what the pack lacks:
  `AI textures: not in the pack - ground N, objects N, ...; sprite frames N, icons N`.
- The **Ready textures** page shows the pack: its pictures by map and kind, deleting the ticked ones (the pack is
  written again without them), deleting the whole pack, and deleting the old upscaled.grf.
- A pack cut short (its builder or the game stopped before its directory) is still read, by its pictures' headers.
- The settings app starts in **English** until a language is chosen (it was the language of Windows). A language
  chosen before stays.
- These notes are in three files now: TEST.en.md, TEST.uk.md, TEST.ru.md. Earlier versions' notes: TEST.ru.md.
- Fixed in passing: in a sprite cache a picture of a frame could go over a newer frame of another kind.

How to test:
1. update.bat. In the game folder uaro-textures.zip is there (made on the Texture pack page).
2. Direct3D 12, AI textures on: in uaro-graphics.log `render64: AI textures: on - ...`,
   `the texture pack uaro-textures.zip has N pictures`, every 30 seconds `... from the pack ...` and, if something is
   missing, `not in the pack - ...`. Textures improved as before. No `the 64-bit renderer exited`.
3. The same with Direct3D 11 (the Render page): `AI textures: on with Direct3D 11 - ...`.
4. Settings → Ready textures: the pack's pictures in the table; "Delete the old upscaled.grf" if it is still there.
5. Send the log and, if something looks wrong, a screenshot.

Notes of earlier versions (in Russian): TEST.ru.md.
