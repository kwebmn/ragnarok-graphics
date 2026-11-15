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
