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
