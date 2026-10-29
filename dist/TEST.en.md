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
