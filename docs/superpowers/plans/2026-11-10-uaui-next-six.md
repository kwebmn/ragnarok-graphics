# uaUI - the next pieces (2, 3, 4, 5, 6, 7) - plan and ledger

Owner 2026-11-10: «делай без остановки 2,3,4,5,6,7». Betas only, the gate before each (gship.sh).
- 2: the pad's hint while dead with the death window closed («B: повернутися»).
- 3: «Could not get value from data variable 'words[N]…'» - a sheet's lists shrinking: the cause and a cure.
- 4: more live windows (cards first: ask, tip, macros, chat rooms, whisper setup, options, pet, homun, merc).
- 6: what is still a picture - «Довідник»'s body first, then signs, buff overlay, drag icons, pad texts.
- 7: the chat's line our own field (sending through the game's own code).
- 5: the core painters out once their tests check layouts and words without painting.

## Ledger
- 2+3 beta.80: deadHint («B: повернутися на точку збереження») while dead with the death window closed; sheet and item grid lists never shrink (rows past the end empty and unseen), images with no source skipped
- 4 beta.81: $S/live_common.py applies the live-window pattern; live: the tip of the day (paragraphs, startup tick, Back/Next), the ask card (requests, number/text/count with the game's field shown, quick choices), the pet (gauges, four commands)
- 4 beta.82: the game's options live (volumes = RmlUi range sliders, switches ticks); Doc::pointer keeps a press's release (a slider let go off the window)
- 6 beta.83: «Довідник»'s world map picture painted again only when what it shows changes (a key of its look; was every hover), its texture by the paint count. Ruling: the map squares (minimap, big map) stay pictures - a map is pixels, drawn from its texture each frame (cheap); buff_overlay (labels over the game's own buff icons with the buffs column off) repaints on change only - kept; world_signs' plates painted once a sign - kept; drag icons - kept
