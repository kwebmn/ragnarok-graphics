# Models by our renderer («Модели нашим рендером») - ledger

Spec: the owner's concept Claude Doc https://claude.ai/artifact/XMkZ5UMHQXV9CJCvsy82j6 (answered 15.11: 1 - the 3D view
first, then the top view if it works out; 2 - the animated models drawn by us; 3 - colours strictly as the game's;
4 - on by default: later; 5 - see what it gives, weak cards most wanted). Stages: 1 measure + colours from the game's
memory (no change to the picture), 2 our models in place of the game's in the 3D view (a switch, off), 3 casters,
reflections and far colours from ours, 4 animated models, 5 default on + the top view. Betas 3.0.20-beta.N (3.0.19
RELEASED 15.11, main 8394155).

1. 3.0.20-beta.1 (the owner «приступай»). The game's facts (uaRO.exe): the model node C3dNode - its draw FUN_004DE9F0
   (virtual, one vtable slot; thiscall + 3, ret 0xC; entry 55 8B EC 81 EC 88 00 00 00; ebx = the node); its setup
   FUN_004DF080 from the resource node (name +8, alpha +0xA0 = res[0x148]/255, textures +0x88 count / +0x90 array,
   mesh +0x9C = res +0x114, the faces' colours vector +0x118..+0x11C sized to the faces, 0x24 bytes a face); its light
   FUN_004DF930 - the faces' colours from the map's light into DAT_007284B0, then into +0x118: three colour dwords and
   u v three times (scaled by the texture's real / padded size), then FUN_004E0070 adds each vertex's local light
   (FUN_004E7510 at its world place) into +0x144 (9 ints a face, clamped 255); the mesh: faces +4, vertices +0xC (12
   bytes), the faces +0x1C of 0x18 bytes (vertex indices +0/+2/+4, uv indices +6/+8/+0xA, texture +0xC, two-sided
   +0x10). The draw: per texture group the kept faces' vertices one after another from the pool block (FUN_00412980),
   a face filed per group (FUN_00412730/FUN_00412020: [0] its vertices, [1] their count, [2] the texture); colours by
   the way: 0x748878 not 0 (other), +0x124 (half see-through: alpha 0x80, the colour as in memory; cleared after the
   draw - set by someone each frame), 0x7710B9 not 0 (night?), else plain (the colour bytes through __ftol); alpha
   the actor's (+4) byte +0x1ED. Done: core/model_census (compare: drawn vertices in threes matched to the next memory
   face with the same uv - the culled passed by, the block's stale rest ends it; per channel same / drawn-to-memory
   ratio / most apart; Tally: nodes, faces, ms a frame and its share, the checks by way); win/model_census (the node
   draw hooked with a trampoline: depth stack, the outermost timed, the way read before the draw; exe_hot growHook
   tells noteGrow the node's own first block - its offset from the pool's start, found again after a child's growth;
   4 nodes a frame checked; frame() at the present, a line every 30 s). Tests: model_census_* (3; the matcher's skip
   mutated -> RED). Ruling: the tests written first but the module's header written with its code before a run - the
   RED shown by a mutation instead; cost if wrong: none. Untested in the game.
3.0.20-beta.1 SHIPPED (ff37304, update.ini 45ff42b).
2. 3.0.20-beta.2. The owner's reports on beta.1 (R-V89FD6 payon, R-KGER4B prontera, R-BURESU prontera at night - one
   session): the game's models a frame 449-958 nodes, 17.8-26.9 k faces, 1.55-2.12 ms (9-13 % of its frame); every
   node checked in the «night» way (0x7710B9 set by day too), 27-61 % the same as +0x118, the rest x1.8-2.3. The code
   (0x4DEDB0-0x4DEF7F) read: 0x7710B9 = lit - the drawn colour from +0x144 (each vertex's ints: the face's colour plus
   its place's light, FUN_004E0070) x 0x7710E4/E8/EC (bytes 2 1 0); plain - +0x118 colour bytes x the same; other
   (0x748878) - x 0x7710D8/DC/E0; half see-through - the colour, alpha 0x80; each product __ftol-truncated, its low
   byte stored (no clamp); alpha the actor's +0x1ED. Done: modelcensus::Colouring / expected() / FaceLit; compare()
   against the exact 32 bits; the driver reads the way, the factors and the alpha before the draw, +0x144 for lit.
   Tests: model_census_expected_as_the_game_colours_each_way, model_census_lit_faces_checked_against_their_vertex_light
   (+ the two kept). Also R-JJE8HA (the beta tester, auto «hang» on the RELEASE 3.0.19, i7-5820K, D:): after a death
   prt_in loaded 8.23 s (cps.dll inflated 311 files, 7.3 MB in 8 s - a slow disk), then the Kafra «Склад» opened and
   the game stood 15 s: cps.dll 53.6 % (the game's GetData 0x50D1A2 -> 0x5110C1) + zip::Index::read 46 % - our
   clienttables::icon read every new icon at once (the game's .bmp, then the AI pack's copy). core/read_budget: the
   icons' first reads kReadMs 4 a 16-ms window, the rest in the next ones (logged once when 20 waited). Test:
   read_budget_spends_its_share_of_a_window_then_waits_for_the_next. Untested in the game.
3.0.20-beta.2 SHIPPED (2821b68, update.ini 7f54add).
3. 3.0.20-beta.3 (stage 2a). R-CFQAN3 (the owner on beta.2, prontera 3D): the colours as the game makes them from its
   memory 99.1-100.0 % of 527-746 k vertices a 30 s (the lit way; the rest 128 apart at most - an alpha, likely fading
   actors); the models 1.22-1.88 ms a frame (7-11 %), 588-799 nodes, 17.8-24.8 k faces. The game's map loader
   (FUN_004EA9A0) makes an actor (0x200 bytes, FUN_004E0540) a .rsw object - its place copied to actor[0..2], turn
   [3..5], scale [6..8], the animation's speed [0x2D] and kind [0x7C] (+0x1F0), the object's name at +0x2C, its root
   node (0x150 bytes) at +0x28 (FUN_004E0CA0, node +4 the actor); the world's actors vector at +0x48..+0x4C - far ones
   queued for the loader thread (FUN_004EE3C0), so not in the file's order. A node's children list +0xAC (MSVC list:
   next, prev, the child at +8); its keys of turning +0xF8 (0x14 each) and moving +0x108 (0x20 each), one default each
   when none; its mesh +0x9C: vertices count +0, vertices +0xC (12 bytes), faces +4 / +0x1C (0x18: vertex indices
   +0/+2/+4, uv indices +6/+8/+0xA, texture +0xC, two-sided int +0x10); textures +0x88 count / +0x90 CTexture* (its
   surface +0xC: our Surface7). Ruling: capture the game's own nodes as it draws them (the node from the census's call
   stack in modelProjection, the node-to-view matrix x the inverse of the view at CView +0x94 = the world) instead of
   colouring our .rsm-placed models - the game's exact geometry and colours, no face matching; cost if wrong: the
   capture's world off (checked: the renderer's corners against ours). Done: core/game_models (Node/Face 84 bytes over
   the wire, compose/worldOf/transform, Corners grid); Msg::GameModel (bridge kVersion 36); Remote::sendGameModel,
   serve.cpp, Renderer::addGameModel (D3D12 keeps them: win/gpu/game_models.cpp - a new map's first clears, our own
   corners by mapWorld.axes, a line «game models: ... corners on our own models X of Y» every 30 s when more came);
   driver: modelcensus::noteProjection (8 a frame, once a node a map; animated - keys > 1 or the actor's kind - left to
   the game), mapChanged at the map's change, dx7::gpuTextureOf, view3dwin::lastView; a line «models: N static nodes
   captured ... M moving ones left to the game». Tests: game_models_* (3; Corners' neighbour cells mutated -> RED).
   Lesson again: `near` is windef.h's macro (Corners::within). Untested in the game.
3.0.20-beta.3 SHIPPED (f3e5224, update.ini 238e5d9).
   R-B2EM7U (the owner on beta.3, prontera): 165 static nodes captured (8989 faces, 66 textures), their corners on our
   own models 7185 of 7200 (99.8 %) - the capture's world is right; but «2105 moving ones left to the game»: actor
   +0x1F0 is NOT the animation's kind - the game's collision (FUN_004EB3C0/4F0/580: +0x1FC set and +0x1F0 == 0 ->
   the model blocks) - a blocking type; animated = a node's keys of turning (+0xF8, 0x14) or moving (+0x108, 0x20)
   more than one. Fix in 2b.
   PLAN 2b (next beta): [view3d] own_models «Моделі нашим рендером» (off; live; «3D-режим» on «Гра»). Driver: the
   node draw hook, in the 3D view with the switch on and the renderer's word (BlockHeader gameModelsAt, as
   ownGroundAt, Channel noteGameModels/gameModelsLately), for a node sent (g_sent), static, not half see-through
   (+0x124), its actor's alpha 0xFF: not the game's draw - its matrix by the game's own FUN_00409720 (thiscall ecx = the
   out 4x3, stack: node +0xB4, the parent's matrix; ret 8) and its children drawn as the game does (list +0xAC, each
   child's vtable slot 0 thiscall (matrix, a3, a4)); the node's id into the frame's list. At the game's EndScene (before
   the frame's record) Msg::GameModelsFrame: the factors 0x7710E4/E8/EC, the lit way, the ids drawn by us this frame.
   Renderer: drawGameModels beside drawOwnModels (own_ground.cpp layer 3, the own models' DrawSetup modelLike): for
   each id of the frame's list its faces - world -> file places by mapWorld.axes, own_ground::place each corner, the
   colour (lit ints or the colour bytes) x the factors truncated, low byte, alpha 0xFF; pieces by texture, two-sided ->
   no cull; noteGameModels when drawn. Stage 3 later: casters, reflections, model_fade from ours.
4. 3.0.20-beta.4 - stage 2b: the renderer draws the captured still nodes in the 3D view in place of the game's
   ([view3d] own_models «Моделі нашим рендером», off, on «Гра» under «3D-режим»). The game's node draw read whole
   again (decomp FUN_004DE9F0): the node's matrix FUN_00409720(out = local +0xB4 x the parent's = the draw's first
   argument); faces culled on the screen by (x2-x0)(y1-y0) <= (x1-x0)(y2-y0) unless two-sided, the other way round
   when the second argument is set - its actor's render FUN_004E0B20 passes the parity of its scale's axes below 0
   (a mirrored model); the third argument: faces whole within the screen (and within the renderer's +0x58..+0x64 when
   0x7735EC is set) - no visible change, left out; faces by texture into the game's lists FUN_00412020: blended (1)
   when the actor's alpha is not 0xFF or the texture is CTexture +0x10 format 1 (A4R4G4B4, its own alpha); +0x124
   cleared at the end; the children with (the node's matrix, the 2nd, the 3rd). FUN_004f6ad0 (the lit way's call) is
   a getter. The actor's fade (+0x1EC/+0x1ED, 5 a frame down to 0x5A) is its render's, after the node draw: kept.
   Rulings: (a) the renderer culls in software as the game's (gamemodels::shown, the node's `flipped` over the wire)
   with CULL_NONE - the game's own rule, D3D's cull would follow the game's set state; cost if wrong: faces missing
   or showing from behind (seen at once in the 3D view). (b) a node with a texture of its own alpha, or one the
   renderer does not know (gpuTextureOf 0), stays the game's (its blended list is sorted and drawn later than ours) -
   cost: fewer nodes ours. (c) 0x7735EC set -> the game's. (d) the in-process D3D12 renderer (no remote) answers
   gameModelsLately false - only the 64-bit host draws them; cost: none drawn by ours there. (e) the frame's list is
   sent at every flush whose list grew (Context::flush before execute; all node draws come before the game's lists
   are drawn at its scene's end), cleared at modelcensus::frame (the end of present). Fixed: animated() = the keys only
   (R-B2EM7U). Tests: game_models_faces_culled_as_the_game_culls_them (the flat face: culled, flipped shown - as the
   game's <=), the wire round trip with `flipped`, config_view3d_own_models_off_by_default, game_models_frame_*.
   (f) a still node is ours only while placed and coloured as when captured (its mesh, its actor's +0xC0, its own
   +0xB4, its +0x118/+0x144 lists and their first face's values) - else given back to the game for the map; a node
   under an animated one (the census's call stack) is left to the game at its capture. The lights +0x144 are worked
   out by FUN_004DF930 -> FUN_004E0070 (the vertex's place in the map's light grid FUN_004E7510 + its colour, at most
   255) - when, unseen (a virtual call): the guard covers a relight. Cost if wrong: a model left to the game.
   Log: «models: N nodes drawn by our renderer in place of the game's in F frames (X a frame) ... K captured ones left
   to the game ..., M given back to it (moved or coloured anew since)». Untested in the game.
3.0.20-beta.4 SHIPPED (eb64ae9, update.ini 221619b).
5. 3.0.20-beta.5 - R-UQPC3E (the owner on beta.4, prontera 3D - [view3d] own_models not on: no [view3d] in his ini,
   no «drawn by our renderer» line; a baseline: 59.9 fps at the limit 60, the game's side 23 of 1797 over 25 ms, the
   renderer's 127 of 1797 (its dearest the game's draws themselves), models 1.73 ms of the game's frame (10 %),
   colours 98.7-99.9 %, corners 99.3-99.4 %, no node kept for its textures) showed «865 moving» of 1628. The code read
   again: every model runs its animation (FUN_004FA8B0 -> FUN_004E0940 for each actor seen; its kind +0xB0 = 2 from its
   constructor FUN_004E0540, never set from the .rsw) -> FUN_004DE950 works each node's matrix out of its keys: moving
   +0xE8 (16 bytes, FUN_00401520), turning +0xF8 (20, FUN_00401D30), +0x104/+0x108 (32, FUN_00402090: one default put
   in by the setup FUN_004DF080 - the beta.4 rule's «moving keys» there never had more than one; the real moving keys
   +0xE8 were not looked at). Rulings: (a) a node moves when its keys' values differ (gamemodels::keysMove; frames
   alone do not) - cost if wrong: a model drawn still while the game moves it, caught by (b). (b) the guard compares
   the parent's matrix in the world (the draw's first argument x the view's inverse, worked out again when CView +0x94
   changes) within float noise (gamemodels::samePlace: 1e-3 the turn's rows, 0.01 the move) instead of the actor's
   matrix - a node hung on a moving one is given back the frame it moves. Log: «(N of them with several keys all
   alike), ... (M of them hung on a moving one)». Tests: game_models_keys_move_a_node_only_when_their_values_differ,
   game_models_places_alike_within_the_float_noise_only. Untested in the game.
