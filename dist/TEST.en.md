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
- How to check: Direct3D 11 on the Render page, turn on "Glow of effects", use fire skills or stand by a portal — a soft halo
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
