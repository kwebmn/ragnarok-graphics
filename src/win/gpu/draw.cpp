#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>

#include "core/ai/kinds.hpp"
#include "core/d3d7.hpp"
#include "core/daylight.hpp"
#include "core/effect_exe.hpp"
#include "core/effect_light.hpp"
#include "core/portal.hpp"
#include "core/foliage.hpp"
#include "core/game_camera.hpp"
#include "core/scale.hpp"
#include "core/twin_grid.hpp"
#include "core/water.hpp"
#include "win/gpu/ai_textures.hpp"
#include "win/gpu/draw_constants.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

void Renderer::Impl::prepareGeometry(const FrameRecord& rec) {
    geometryList = listSerial;
    vertexBase = 0;
    indexBase = 0;
    indexBytes = 0;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    if (!v.empty()) {
        const Upload u = allocUpload(v.size(), 16);
        if (!u.cpu) return;  // no memory for this record's vertices: its draws are skipped (vertexBase 0)
        std::memcpy(u.cpu, v.data(), v.size());
        vertexBase = u.gpu;
    }
    const std::vector<uint16_t>& i = rec.indices();
    if (!i.empty()) {
        const Upload u = allocUpload(i.size() * sizeof(uint16_t), 4);
        if (!u.cpu) {
            vertexBase = 0;
            return;
        }
        indexBytes = UINT(i.size() * sizeof(uint16_t));
        std::memcpy(u.cpu, i.data(), indexBytes);
        indexBase = u.gpu;
    }
}

void Renderer::Impl::draw(const FrameRecord& rec, const DrawCmd& d) {
    if (d.vertexCount == 0 || !vertexBase) return;
    if (d.primitive != d3d7::PT_TRIANGLELIST && d.primitive != d3d7::PT_TRIANGLESTRIP) {
        static bool logged = false;  // the client only draws lists and strips (states.md §4)
        if (!logged) LOG_INFO("primitive type %u not drawn", static_cast<unsigned>(d.primitive));
        logged = true;
        return;
    }
    const DrawSetup& s = rec.states()[d.state];
    crumb(seamPass  ? "ground seams' strips"
          : ownPass ? "the own ground"
          : s.ui    ? "the game's interface"
                    : "the game's world");
    // 1.9: the driver's own draws (no hooks, nothing noted); 3.0.4-beta.10 the own ground's
    const bool inner = seamPass || darknessPass || ownPass;
    // 2.6.18: blindness's darkness waits for the world's effects (applyDarkness)
    if (!inner && !s.ui && d.effect == uint8_t(effect_exe::Kind::Darkness) && !darknessLaid) {
        noteDarkness(rec, d, s);
        return;
    }
    if (s.zEnable && s.zWrite) sceneDepthFresh = false;  // 0.39.2: the depth's copy is out of date
    // Stage 4: with several samples an alpha test that keeps the brighter alpha (the client's 0xCF on leaves and
    // fences, no blending) becomes coverage - a soft edge instead of a step. World textures get anisotropy.
    const bool coverage = alphaToCoverage && samples > 1 && s.alphaTest && !s.blend &&
                          (s.alphaFunc == d3d7::CMP_GREATER || s.alphaFunc == d3d7::CMP_GREATEREQUAL);
    const int aniso = s.ui ? 0 : anisotropy;
    // Part 3 (DLSS): the world into its own target with the frame's jitter, the interface into the layer.
    const bool toWorld = upscale && !s.ui;
    const bool toLayer = upscale && s.ui;
    if (upscale) startFrame();
    // 0.23: the game's torch layers - its glow not drawn, its flame noted - are drawn anew once the world is.
    if (!s.ui && torchWanted() && !torchDone && d.vertexCount == 4 && !objectTextures.empty() && takeTorch(rec, d))
        return;
    // 2.6.12: the torches' pass off, the game's flames (drawn as it has them) still show the embers where the fires
    // are
    if (!s.ui && !torchWanted() && crittersNeedFire() && d.vertexCount == 4 && !objectTextures.empty())
        noteCritterFlame(rec, d);
    // 0.28: a firefly the game draws (effect 45, known by the driver) - drawn anew with the torches.
    if (!s.ui && d.effect == uint8_t(effect_exe::Kind::Firefly) && fireflyWanted() && !torchDone &&
        takeFirefly(rec, d))
        return;
    // 0.18, the glow: from the first draw of the world that adds light on, the world's draws write into the glow
    // target too - what adds light adds it there, what covers it takes it away. Once the world is drawn, its glow
    // goes onto the picture before the interface (under DLSS onto the upscaled world, composite()).
    // 0.20, soft shadows: once the frame's ground (two UV sets) is drawn, at the world's first blended draw - the
    // characters' list comes first of those - or at the interface, the shadows go onto what is drawn so far: the
    // map and its models, before the characters and the effects.
    if (!s.ui && d.fvf == d3d7::FVF_TL2) groundDrawn = true;
    if (!seamPass && !ownPass && !s.ui && d.fvf == d3d7::FVF_TL2 && s.zWrite &&
        (seamsWanted() || hillsWanted() || grassCellsWanted()))
        noteGround(rec, d);  // 1.9; 1.15 the smooth hills of the next frame; 2.2.1 the grass's ground
    // 1.16: an opaque draw of the world into the next frame's mirror (2.2: the sun's map is gone)
    if (!inner && !s.ui && !s.blend && s.zEnable && s.zWrite && mirrorCollecting()) noteCaster(rec, d, s);
    // 3.0.4-beta.10: the own ground in place of the game's (own_ground.cpp) - the game's ground draws noted, not drawn,
    // once its pictures are up; drawn itself at the world's first blended draw after them, or at the interface
    if (!inner && !s.ui && d.fvf == d3d7::FVF_TL2 && s.zWrite && ownGroundWanted()) {
        if (!ownState.decided) {
            ownState.decided = true;
            ownState.frame = prepareOwnGround();
        }
        if (ownState.frame) {
            noteOwnGround(rec, d);
            return;
        }
    }
    // 3.0.4-beta.16 (R-35N5QN «когда отматываю до этого места становится темное»): the camera far out, all of the
    // game's world past its far plane - no ground of the game's to note, the own ground drawn all the same
    if (!inner && !ownState.decided && (s.ui || s.blend) && ownGroundWanted()) {
        ownState.decided = true;
        ownState.frame = prepareOwnGround();
    }
    if (!inner && ownState.frame && !ownState.drawn && (s.ui || s.blend) && !drawOwnGround()) return;
    // 3.0.4-beta.20: the own water past the far plane, after the characters (the modern water's copy of the world taken
    // there, as before). 3.0.4-beta.24 (R-PTAMG4 «на корабле видна полоса»): at the start of the game's sorted list -
    // its water and its models' keyed faces back to front, depth not written (uaRO.exe 0x40e4d0) -, not at its first
    // water quad: the faces before that one lay under the far water (a band of water over the ship's deck). The list's
    // first water quad read ahead in the record for its picture, state, colour and waves; or at the interface
    if (!inner && ownState.frame && !ownState.waterDrawn && !s.ui && s.blend && !s.zWrite) {
        noteOwnWaterAhead(rec, size_t(&d - rec.draws().data()));
        if (!drawOwnWater()) return;
    }
    if (!inner && ownState.frame && !ownState.waterDrawn && s.ui && !drawOwnWater()) return;
    // 3.0.4-beta.14: a draw of the game's own models near - the far models' colours and state learnt from it. Not by
    // this picture's choice (ownState.frame): the game draws its models before its ground
    if (!inner && ownState.serial && ownGroundWanted() && effects.ownFarModels && !s.ui && d.fvf == d3d7::FVF_TL1 &&
        s.zWrite && !s.blend && d.effect == 0)
        noteOwnModels(rec, d);
    if (!inner && !s.ui && s.blend && seamsDue() && !applySeams()) return;  // 1.9: before the soft shadows
    if (!inner && !s.ui && s.blend && aoDue() && !applyAo()) return;
    if (!inner && !s.ui && s.blend && groundLifeDue() && !applyGroundLife()) return;  // 1.7: under the grass
    if (!inner && !s.ui && s.blend && grassDue() && !applyGrass()) return;  // 0.81: the grass under the characters
    // 0.32: the characters' shadows likewise - from this draw on the frame's characters stand on the ground - and
    // the game's round ones are in them.
    if (!inner && !s.ui && s.blend && actorShadowsDue() &&
        !applyActorShadows(rec, size_t(&d - rec.draws().data())))
        return;
    if (!s.ui && d.effect == uint8_t(effect_exe::Kind::Shadow) && feetWanted()) noteFoot(rec, d);  // 0.94
    if (!s.ui && actorShadowsDrawn && d.effect == uint8_t(effect_exe::Kind::Shadow)) return;
    // 2.7.40: the game's own snow where ours falls - one snow, not two (weather::hidesGameEffect)
    if (!s.ui && weather::hidesGameEffect(mapWeather, mapWeatherWanted(), d.effectId)) {
        static bool told = false;
        if (!told) LOG_INFO("weather: the game's own snow not drawn - ours falls over %s", mapName.c_str());
        told = true;
        return;
    }
    // 2.7.54: a draw of a map warp - its rings, floor or sparks (portal::partOf) - shaded anew where the game draws it:
    // the new portal (2.7.48-2.7.52 drew one of the driver's own over its place, the game's left out: two portals, or
    // one off its place); the light round it by its floor's middle (notePortal). 2.7.55: its rings and floor only
    const portal::Part warp = !s.ui && d.effect == uint8_t(effect_exe::Kind::Portal)
                                  ? portal::partOf(int(d.effectId), s.blend, s.srcBlend, s.dstBlend, d.vertexCount)
                                  : portal::Part::None;
    const bool warpDrawn = portal::reshaded(warp) && newPortalWanted();
    if (warpDrawn && warpsToldMap != mapName) {  // once a map, for the reports
        warpsToldMap = mapName;
        LOG_INFO("portals: the warps shaded anew on %s - their rings' light spikes and floor at full resolution",
                 mapName.c_str());
    }
    // 0.21: the world done - at the interface - its colours, then its glow.
    if (s.ui && !upscale && !finishWorld()) return;
    if (s.ui && !upscale && glowDrawn && !addGlowToTarget()) return;
    const bool adds = !s.ui && s.blend && s.dstBlend == d3d7::BLEND_ONE;
    if (adds && !glowActive && glowWanted()) startGlow();
    // 2.7.38 (the tester, Magnus by day in Comodo: «все умения, и даже когда кастуется что-то, какие-то очень
    // яркие»): a skill's added light laid over softly, the target's colour its factor (effect_exe::softAdd) - half a
    // Magnus added to Comodo's sand (0.75) burnt it white, as the game's whole one does with Pause
    const bool softAdd = effect_exe::softAdd(effects.skillsSoft, adds, d.effect, s.srcBlend);
    DrawSetup laid = s;
    if (softAdd) laid.srcBlend = d3d7::BLEND_INVDESTCOLOR;
    // 0.26: the fire of skills (a fire-coloured picture added on) lights around it once the world is drawn.
    // 0.38: a portal's draw, for its light round it once the world is drawn - 2.7.54: a map warp's floor
    if (warp == portal::Part::Floor && portalsWanted() && !torchDone) notePortal(rec, d);
    // 0.36: the other skills light too, in their pictures' colours - not the aura, the maps' objects (their own).
    // 2.7.21: an orb of light (Ruwach's, Sight's: effect_light::orbOf by the effect's number) whatever its blend - the
    // game lays it over the world as a sprite, its picture the sprite cache's (the tester: «рувотч не светит»)
    float orb[3];
    const bool orbDraw = !s.ui && s.blend && effect_light::orbOf(d.effectId, orb);
    if (orbDraw && fireWanted() && !torchDone) noteFire(rec, d, 1.0f, orb, true);
    if (adds && !orbDraw && fireWanted() && !torchDone && d.texture[0]) {
        const auto ft = textures.find(d.texture[0]);
        // 2.7.39: a casting lights nothing round it - not the caster, not the ground, not the air
        const bool mine = (d.effect == uint8_t(effect_exe::Kind::None) ||
                           d.effect == uint8_t(effect_exe::Kind::Skill)) &&
                          !effect_exe::isCasting(d.effectId);
        if (ft != textures.end() && ft->second.fire > 0.15f && mine) {
            noteFire(rec, d, ft->second.fire, ft->second.fireColour);
        } else if (ft != textures.end() && effects.skillColours && ft->second.light > skillLightFloor() && mine &&
                   (objectTextures.empty() || objectTextures.of(d.texture[0]) == objects::Kind::None)) {
            noteFire(rec, d, ft->second.light, ft->second.lightColour);
        }
    }
    // 1.25: not the level 99 aura - blurred into the glow it lay as white pools under the players (Prontera)
    // 2.7.39: nor a casting (the light round the caster's feet) - its halo lay as a white haze round the caster
    const bool toGlow = !s.ui && glowActive && d.effect != uint8_t(effect_exe::Kind::Aura) &&
                        !effect_exe::isCasting(d.effectId);
    // 0.19, modern water: a cell of a map's water (core/water.hpp) is drawn by its own shader, from the picture and
    // the depth the frame had before its first water (copyScene); without them, as the game draws it.
    // 3.0.4-beta.20: and the own water past the far plane (ownWater 1, drawOwnWater)
    bool water = false;
    if (!s.ui && waterWanted() && d.fvf == d3d7::FVF_TL1 && (d.vertexCount == 4 || ownWater == 1) && s.blend &&
        s.srcBlend == d3d7::BLEND_SRCALPHA && s.dstBlend == d3d7::BLEND_INVSRCALPHA) {
        const std::vector<uint8_t>& v = rec.vertexBytes();
        const size_t stride = FrameRecord::stride(d.fvf);
        water = (ownWater == 1 || (d.vertexOffset + 4 * stride <= v.size() &&
                                   water::isWaterQuad(v.data() + d.vertexOffset, stride, d.vertexCount, d.indexCount,
                                                      d.primitive == d3d7::PT_TRIANGLESTRIP))) &&
                copyScene();
        if (water && mirrorDue() && !applyMirror()) return;  // 1.16: the world mirrored, before the first water
    }
    // One quad facing the screen: a sprite (a character, a monster, an item). 0.22.3: under DLSS such a draw, and an
    // effect, writes its coverage into the world's alpha - DLSS's mask of pixels whose history it is not to take.
    bool quad = false;
    if (!s.ui && (ai || toWorld) && d.vertexCount == 4) {
        const std::vector<uint8_t>& v = rec.vertexBytes();
        const size_t stride = FrameRecord::stride(d.fvf);
        quad = d.vertexOffset + 4 * stride <= v.size() &&
               ai::flatQuad(v.data() + d.vertexOffset, stride, d.vertexCount, d.indexCount);
    }
    const bool mask = toWorld && biasMask && (quad || adds);
    // 0.24: a puff of the chimneys' smoke (core/map_objects.hpp) - drawn billowing by the smoke's shaders.
    bool smoke = false;
    if (!s.ui && !water && smokeWanted() && d.vertexCount == 4 && !objectTextures.empty() &&
        objectTextures.of(d.texture[0]) == objects::Kind::Smoke)
        smoke = d.vertexOffset + 4 * FrameRecord::stride(d.fvf) <= rec.vertexBytes().size();
    // 0.31: a cell of lava (core/water.hpp) on a map that has it - by the lava's shaders; its middle lights around.
    bool lava = false;
    if (!s.ui && !water && !smoke && lavaWanted() && d.fvf == d3d7::FVF_TL1 && (d.vertexCount == 4 || ownWater == 2) &&
        s.blend && s.srcBlend == d3d7::BLEND_SRCALPHA && s.dstBlend == d3d7::BLEND_INVSRCALPHA && lavaMap()) {
        const std::vector<uint8_t>& v = rec.vertexBytes();
        const size_t stride = FrameRecord::stride(d.fvf);
        lava = ownWater == 2 || (d.vertexOffset + 4 * stride <= v.size() &&
                                 water::isLavaQuad(v.data() + d.vertexOffset, stride, d.vertexCount, d.indexCount,
                                                   d.primitive == d3d7::PT_TRIANGLESTRIP));
        if (lava && !torchDone && !ownWater) noteLava(rec, d);  // the lava's light: the game's own near
        if (lava && !lavaLogged) LOG_INFO("lava: %s's drawn as living lava", mapName.c_str());
        lavaLogged = lavaLogged || lava;
    }
    // 0.30: a face of one of the forest's beams of light (known by the driver) - by the beams' shaders; its middle and
    // foot noted for the dust and the light where it falls.
    const bool ray = !s.ui && !water && !smoke && d.effect == uint8_t(effect_exe::Kind::Ray) && raysWanted() &&
                     !torchDone && d.vertexOffset + size_t(d.vertexCount) * FrameRecord::stride(d.fvf) <=
                                       rec.vertexBytes().size();
    if (ray) noteRay(rec, d);
    // 0.67, soft particles: an effect facing the screen - added light, a skill's or a portal's picture - fades
    // where it goes into what is behind it (the frame's depth so far, copied once while nothing writes it). Not the
    // characters (their feet stand in the ground), not what writes depth itself. Copied once a picture: what is
    // drawn later (the characters) only hides the effect by the depth test - the ground and the walls are there.
    const bool effectDraw = effect_exe::softEffect(adds, d.effect);  // 2.7.13: not a hit flash
    bool soft = !s.ui && !water && !smoke && !ray && !lava && !warpDrawn && quad && effects.softParticles && s.blend &&
                s.zEnable && !s.zWrite && effectDraw && s.srcBlend != d3d7::BLEND_ZERO &&
                s.srcBlend != d3d7::BLEND_DESTCOLOR && softCopied >= 0;
    if (soft && !softCopied) softCopied = ensureScene("soft particles") && copySceneDepth() ? 1 : -1;
    soft = soft && softCopied > 0;
    // 1.17, living characters: a sprite of the game's sprite cache (a character, a monster) facing the screen, blended
    // over - lit from its silhouette's sides; in the real sun's shadows (their map made) darker
    bool sprite = false;
    if (spriteLightWanted() && !s.ui && !water && !smoke && !ray && !lava && !warpDrawn && !adds && !d.twin &&
        s.blend && d.vertexCount == 4 && d.texture[0]) {
        const auto it = textures.find(d.texture[0]);
        if (it != textures.end() && !it->second.spriteCells.empty() && !it->second.leafy) {
            const std::vector<uint8_t>& v = rec.vertexBytes();
            const size_t stride = FrameRecord::stride(d.fvf);
            sprite = quad || (d.vertexOffset + 4 * stride <= v.size() &&
                              ai::flatQuad(v.data() + d.vertexOffset, stride, d.vertexCount, d.indexCount));
        }
    }
    const DrawShader shader = smoke       ? DrawShader::Smoke
                              : ray       ? DrawShader::Ray
                              : lava      ? DrawShader::Lava
                              : warpDrawn ? DrawShader::Portal
                                          : DrawShader::Game;
    ID3D12PipelineState* pso = pipelineFor(laid, d.fvf, coverage, toLayer, toGlow, water, mask, shader);
    if (!pso) return;
    // AI textures have mip levels of their own, sampled also where the game uses none (its sprite cache, mipmaps
    // off in its options): four times the texels, shrunk on the screen, would shimmer.
    StageSetup sampled[2] = {s.stage[0], s.stage[1]};
    bool aiDrawn[2] = {false, false};  // the stage's texture is the AI's
    if (ai && ai->shown && !(d.twin && s.ui))
        for (int i = 0; i < 2; ++i) {
            const auto it = d.texture[i] ? textures.find(d.texture[i]) : textures.end();
            const bool copy = it != textures.end() && it->second.ai && !it->second.aiHidden;  // 2.4.3: A/B keeps it
            aiDrawn[i] = copy && (s.ui ? it->second.aiUi : it->second.aiWorld) &&
                         (!s.ui || aiInterfaceAt(scale));  // 0.89: not the interface at the game's own size
            // 2.2.1: its kind on in the file now; 2.2.2: a sprite cache's by the kind of the cell drawn (monsters
            // off alone are the game's own at once)
            if (aiDrawn[i] && it->second.aiKind == uint8_t(ai::Kind::Sprites))
                aiDrawn[i] = ai->cellShown(spriteSourceOf(it->second.spriteCells, it->second.width, it->second.height,
                                                          rec.vertexBytes(), d.vertexOffset,
                                                          FrameRecord::stride(d.fvf), d.vertexCount));
            else if (aiDrawn[i])
                aiDrawn[i] = ai->kindShown(it->second.aiKind);
            const bool mips = sampled[i].mipFilter == d3d7::TFP_POINT || sampled[i].mipFilter == d3d7::TFP_LINEAR;
            if (aiDrawn[i] && !mips) sampled[i].mipFilter = d3d7::TFP_LINEAR;
        }
    // A new list, if this draw needs one, starts before the draw records anything; the record's geometry then goes
    // into the new list's slot (1B review M4: the old slot is reused while the new list may still read it).
    // 1.8, the materials: a draw of the ground or a model (it writes depth, not a sprite, not leaves) - its texture's
    // material made first, in a pass of its own, then read with it (t2).
    Texture* matTex = nullptr;
    if (materialsWanted() && !s.ui && !water && !smoke && !lava && !ray && !soft && !quad && !adds && !d.twin &&
        (s.zWrite || seamPass) && d.texture[0] && d.texture[0] != white) {
        const auto it = textures.find(d.texture[0]);
        if (it != textures.end() && !it->second.leafy && ensureMaterial(it->first, it->second, aiDrawn[0]))
            matTex = &it->second;
    }
    // 1.15: smooth hills - a draw of the ground (its own or a seam's strip) reads the corners' normals (t3).
    // 3.0.4-beta.26: not the own ground's - its light is baked into its colours (drawOwnGround)
    bool hilly = hillsWanted() && !s.ui && d.fvf == d3d7::FVF_TL2 && !water && !d.twin && (s.zWrite || seamPass) &&
                 !ownPass;
    // 2.2.4: DLSS's (upscaleMipBias) - not for what its mask keeps from the history (sprites, effects): their finer
    // texels it cannot gather, they would only shimmer
    const float mipBias = toWorld && !mask ? worldMipBias() : 0.0f;
    if (!makeRoom(sampled[0], sampled[1], aniso, water || soft || matTex || hilly ? 5 : 2, mipBias, true)) return;
    if (geometryList != listSerial) {  // also after a list that failed to close: its slot is used again (0.4.1 review)
        flushDraws();  // 2.5: the draws gathered so far are of the vertices uploaded before these
        prepareGeometry(rec);
        if (!vertexBase) return;
    }
    hilly = hilly && ensureHills();
    // Stage 6: a window tile whose twin was made at this scale is drawn from the twin, a texel per target pixel;
    // otherwise (Pause, another scale, no twin) exactly as before.
    const bool twin = d.twin && s.ui && twinMatches(d.twinScale, scale) && textures.count(d.twin);
    // AI textures: what the draw shows - the ground, its lightmap, a sprite, an effect, the rest of the world.
    ai::DrawFacts facts;
    if (ai && !s.ui) {
        facts.uvSets = d.fvf == d3d7::FVF_TL2 ? 2 : 1;
        facts.additive = s.blend && s.dstBlend == d3d7::BLEND_ONE;
        facts.flatQuad = quad;
    }
    Texture* t[2] = {nullptr, nullptr};
    UINT srv[2] = {0, 0};
    for (int i = 0; i < 2; ++i) {
        const uint32_t id = i == 0 && twin ? d.twin : d.texture[i];
        auto it = textures.find(id ? id : white);
        if (it == textures.end()) it = textures.find(white);
        if (it == textures.end()) return;
        t[i] = &it->second;
        // A world texture the network has done is drawn in its place; one it has not, waits its turn. In the
        // interface (0.14.1): its big pictures - the loading screen, the login's background.
        if (ai && !s.ui && id && id != white) {
            facts.stage = i;
            ai->noteWorldUse(id, ai::drawKind(facts));
        } else if (ai && s.ui && !twin && i == 0 && id && id != white && aiInterfaceAt(scale)) {
            ai->noteUiUse(id);
        }
        if (aiDrawn[i]) {
            transition(t[i]->ai, t[i]->aiState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            srv[i] = t[i]->aiSrv;
        } else {
            transition(t[i]->resource, t[i]->state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            srv[i] = t[i]->srv;
        }
    }
    const D3D12_GPU_DESCRIPTOR_HANDLE samplers = samplerTable(sampled[0], sampled[1], aniso, mipBias);
    bindPass(toWorld ? (toGlow ? kPassWorldGlow : kPassWorld)
                     : toLayer ? kPassLayer : toGlow ? kPassTargetGlow : kPassTarget);
    worldDrawn = worldDrawn || toWorld;
    layerDrawn = layerDrawn || toLayer;
    glowDrawn = glowDrawn || (toGlow && adds);
    // Stage 4 review: the interface's edges fall on pixel centres (whole D3D7 positions, x * k + 0.5); with every
    // sample there it is drawn exactly as with one sample, while the world keeps its smoothed edges.
    if (samples > 1) placeSamples(s.ui);
    // 2.5: the tables' descriptors are copied now (makeRoom made room in the ring) and set when the draw is issued -
    // by itself or gathered with the draws before it (flushDraws)
    const uint64_t textureKey = uint64_t(srv[0]) << 32 | srv[1];
    if (textureKey != lastTextureKey) {
        lastTextures = textureTable(srv[0], srv[1]);
        lastTextureKey = textureKey;
    }
    PendingDraw next;
    next.textureKey = textureKey;
    next.textures = lastTextures;
    next.samplers = samplers;
    const auto sceneOf = [&](UINT t2, UINT t3, UINT t5) {
        next.scene = sceneTable(t2, t3, t2, t5);
        next.hasScene = true;
        next.sceneKey = uint64_t(t2) << 40 | uint64_t(t3) << 20 | uint64_t(t5 == kNoSrv ? t2 : t5);
    };
    if (water) {  // the picture and the depth under the water (t2, t3)
        transition(sceneColor, sceneColorState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        transition(sceneDepth, sceneDepthState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        if (mirrorMade) transition(mirror, mirrorState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);  // 1.16 (t5)
        sceneOf(sceneColorSrv, sceneDepthSrv, mirrorMade ? mirrorSrv : kNoSrv);
        if (!waterCells++) LOG_INFO("water: the map's water found, drawn anew");
    } else if (matTex) {  // 1.8: its material (t2); 1.15 the hills' normals (t3)
        transition(matTex->mat, matTex->matState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        sceneOf(matTex->matSrv, hilly ? hillsDataSrv : matTex->matSrv, kNoSrv);
    } else if (hilly) {  // 1.15: the hills' normals (t3)
        sceneOf(hillsDataSrv, hillsDataSrv, kNoSrv);
    } else if (soft) {  // the depth behind the effect (t3)
        transition(sceneColor, sceneColorState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        transition(sceneDepth, sceneDepthState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        sceneOf(sceneColorSrv, sceneDepthSrv, kNoSrv);
    }
    Constants c{};
    c.targetSize[0] = float(toWorld ? renderWidth : width);
    c.targetSize[1] = float(toWorld ? renderHeight : height);
    if (toWorld) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    packStage(s.stage[0], d.keyed[0], c.stage0);
    packStage(s.stage[1], d.keyed[1], c.stage1);
    // 0.22.6: the ground's lightmap (its 256x256 atlas of 8x8 cells) sampled smoothly. 3.0.4-beta.26: and the own
    // ground's (its atlas 2048 wide, the cells laid as the game lays them: terrain::lightmapAtlas)
    if (effects.smoothLight && !s.ui && d.fvf == d3d7::FVF_TL2 && t[1] &&
        ((t[1]->width == 256 && t[1]->height == 256) || (ownPass && d.texture[1] && d.texture[1] == ownState.atlas)))
        c.stage1[1] |= 1u << 25;
    c.alphaTest = s.alphaTest ? uint32_t(s.alphaFunc) | uint32_t(s.alphaRef) << 8 | 1u << 16 : 0u;
    if (coverage) c.alphaTest |= 1u << 17;
    if (toLayer && !s.blend) c.alphaTest |= 1u << 18;  // part 3: opaque in the layer (0.8 review I2)
    // 0.18: its colour into the glow target - light it adds, or a factor it multiplies the light under it by.
    if (toGlow && (adds || (s.blend && (s.srcBlend == d3d7::BLEND_ZERO || s.srcBlend == d3d7::BLEND_DESTCOLOR))))
        c.alphaTest |= 1u << 19;
    if (toGlow && adds && d.effect == uint8_t(effect_exe::Kind::Skill)) c.alphaTest |= 1u << 22;  // 2.7.36: 40 %
    if (softAdd && s.srcBlend == d3d7::BLEND_SRCALPHA) c.alphaTest |= 1u << 23;  // 2.7.38: its light times its alpha
    if (soft) {  // 0.67: its fade (shaders.cpp shade())
        c.alphaTest |= 1u << 20;
        if (s.srcBlend == d3d7::BLEND_ONE) c.alphaTest |= 1u << 21;  // added or premultiplied: the colour too
        const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
        const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
        std::memcpy(c.projection, proj, sizeof proj);
    }
    c.fogEnable = gameFogDrawn(s.fog, effects.noGameFog, mapOutdoors) ? 1u : 0u;  // 2.6.10
    c.fogColor[0] = float((s.fogColor >> 16) & 255) / 255.0f;
    c.fogColor[1] = float((s.fogColor >> 8) & 255) / 255.0f;
    c.fogColor[2] = float(s.fogColor & 255) / 255.0f;
    c.fogColor[3] = 1.0f;
    uint8_t how = 0;  // 2.3, uaRO Studio's X-ray (L1): the texture's state as a colour over it, through the fog's slot
    if (xray && ai && d.texture[0] && ai->howOf(d.texture[0], how)) {
        static const float kColours[3][3] = {{0.25f, 0.85f, 0.35f}, {0.25f, 0.55f, 1.0f}, {1.0f, 0.25f, 0.25f}};
        const int k = how == uint8_t(studio::How::Pack) ? 0 : how == uint8_t(studio::How::Made) ? 1
                      : how == uint8_t(studio::How::Missing) ? 2 : -1;
        for (int i = 0; i < 3; ++i) c.fogColor[i] = k < 0 ? 0.6f : kColours[k][i];  // grey: the game's by a rule
        c.fogColor[3] = 0.35f;
        c.fogEnable = 2;
    }
    c.colorKey = s.colorKey ? 1u : 0u;
    c.uiMode = uint32_t(uiFilterMode(s.ui, scale, uiSharp ? UiFilter::Sharp : UiFilter::Smooth));
    // The AI's picture has the detail the game pixels lack: sampled as it is, not one tap per game pixel.
    if (aiDrawn[0]) c.uiMode = 0;
    if (d.effect == uint8_t(effect_exe::Kind::Panel)) c.uiMode = 0;  // 3.0.3-beta.8: at the target's pixels
    if (twin) {  // the tile's first game pixel is the window's place plus the tile's (spec §6)
        float quad[4];
        uiPixels(rec, d, quad);
        c.uiMode = 3;
        c.twin[0] = float(tileOrigin(int(quad[0]) - d.twinTile[0], d.twinTile[0], scale, d.twinScale));
        c.twin[1] = float(tileOrigin(int(quad[1]) - d.twinTile[1], d.twinTile[1], scale, d.twinScale));
        c.twin[2] = float(t[0]->width);
        c.twin[3] = float(t[0]->height);
    } else if (c.uiMode != 0) {
        uiPixels(rec, d, c.uiPixels);
    }
    c.scale = float(toWorld ? worldScale : scale);
    if (!s.ui && d.fvf == d3d7::FVF_TL2 && c.uiMode == 0 && effects.relief && effects.reliefStrength > 0) {
        c.stage1[1] |= 1u << 26;  // 0.71: the ground's relief (shaders.cpp relief())
        c.uiPixels[0] = float(std::clamp(effects.reliefStrength, 0, 200)) / 100.0f;
        c.uiPixels[1] = t[0] ? float(t[0]->width) : 0.0f;  // 1.5.10: the game's texels, also under an AI picture
        // 2.6.7: shade()'s measure of how well a pixel sees the relief (seen) takes the pixel's place in the view -
        // by the projection, which only the materials, the hills and the seams set: without them it was 0, seen 0
        // and no relief at all (the tester's check of it alone)
        const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
        const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
        std::memcpy(c.projection, proj, sizeof proj);
    }
    // 2.6.19: leaves as the game draws a tree's - cut by its alpha test, not blended over: never a sprite or an effect,
    // whatever its picture looks like (the Pyramids' Poporings shed leaves)
    // 3.0.4-beta.18: not the own far models' - a draw of theirs holds many trees (their shade baked in, drawOwnModels)
    const bool leaves = !s.ui && !twin && c.uiMode == 0 && t[0] && t[0]->leafy && d.fvf != d3d7::FVF_TL2 && !s.blend &&
                        !ownPass;
    float swayTop = 0.0f, swayBottom = 0.0f;  // 0.64: leaves in the wind
    if (leaves && effects.foliage && effects.foliageStrength > 0 && yExtent(rec, d, swayTop, swayBottom)) {
        c.twin[0] = foliage::kSwing * float(effects.foliageStrength) / 100.0f;  // 2.7.30
        c.twin[1] = swayBottom;
        c.twin[2] = swayBottom - swayTop;
        c.twin[3] = waterTime();
    }
    if (leaves) {  // 1.7
        LeafLook look = leafLook();
        leafShading(rec, d, look, c);
        if (effects.leafFall) noteCrown(rec, d);
    }
    if (matTex) materialConstants(d.fvf == d3d7::FVF_TL2, c);  // 1.8
    if (hilly) hillsConstants(c);  // 1.15
    if (sprite && !matTex && c.uiMode == 0) spriteConstants(c, t[0] ? t[0]->width : 0);  // 1.17
    if (seamPass) {  // 1.9: a strip across a seam of the ground - its alpha from the noise over the world (shade())
        c.stage1[1] |= 1u << 31;
        const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
        const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
        std::memcpy(c.projection, proj, sizeof proj);
        std::memcpy(c.reproject, viewToWorld, sizeof c.reproject);
        if (seamsView) {  // 2.7.8: [debug] seams_view - own bands cyan, the neighbours' strips magenta (as the X-ray)
            static const float kOwn[3] = {0.1f, 0.9f, 1.0f}, kTheirs[3] = {1.0f, 0.15f, 0.85f};
            for (int k = 0; k < 3; ++k) c.fogColor[k] = seamOwnBand ? kOwn[k] : kTheirs[k];
            c.fogColor[3] = 0.85f;
            c.fogEnable = 2;
        }
    }
    if (water) {  // the game's camera, the waves' time and the page's settings (water_ps)
        std::memcpy(c.reproject, viewToWorld, sizeof c.reproject);
        const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
        const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
        std::memcpy(c.projection, proj, sizeof proj);
        c.sharpen = waterTime();
        c.footprint = float(effects.waterWaves) / 100.0f;
        c.twin[0] = float(effects.waterReflections) / 100.0f;
        c.twin[1] = float(effects.waterClarity) / 100.0f;
        c.twin[3] = waterLife();  // 1.7
        if (mirrorMade) c.stage0[1] |= 1u << 27;  // 1.16: the world mirrored (t5)
        if (!ownWater) noteWaterLevel(rec, d);  // 3.0.4-beta.20: the game's own water's, not the far water's
        if (ripplesWanted()) {  // 0.94: the three newest steps (x, z, age), one in each free vector
            float* slot[3] = {c.uiPixels, c.sourceRect, c.sourceClamp};
            int n = 0;
            const float now = waterTime();
            const std::deque<footsteps::Step>& steps = footTracker.steps();
            for (auto it = steps.rbegin(); it != steps.rend() && n < 3; ++it) {
                const float age = now - it->born;
                if (age < 0.0f || age > kRippleLife) continue;
                slot[n][0] = it->x;
                slot[n][1] = it->z;
                slot[n][2] = age;
                ++n;
            }
            c.twin[2] = float(n);
        }
    }
    if (smoke) {  // its depth, the camera backwards, the time and the density (smoke_ps)
        const uint8_t* v = rec.vertexBytes().data() + d.vertexOffset;
        const size_t stride = FrameRecord::stride(d.fvf);
        float rhw = 0.0f;
        for (size_t k = 0; k < 4; ++k) {
            float w = 0.0f;
            std::memcpy(&w, v + k * stride + 12, sizeof w);
            rhw += w * 0.25f;
        }
        c.twin[0] = rhw > 0.0f ? 1.0f / rhw : 1.0f;
        static const float kSame[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};  // the camera unknown: the view itself
        std::memcpy(c.reproject, viewToWorldValid ? viewToWorld : kSame, sizeof c.reproject);
        const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
        const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
        std::memcpy(c.projection, proj, sizeof proj);
        c.twin[3] = waterTime();
        c.footprint = float(std::clamp(effects.smokeDensity, 0, 200)) / 100.0f;
    }
    if (ray || lava) {  // 0.30, 0.31: the camera backwards, the time, the brightness (their depth: their pixels')
        static const float kSame[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
        std::memcpy(c.reproject, viewToWorldValid ? viewToWorld : kSame, sizeof c.reproject);
        const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
        const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
        std::memcpy(c.projection, proj, sizeof proj);
        c.twin[3] = waterTime();
        c.footprint = float(std::clamp(ray ? effects.raysBrightness : effects.lavaBrightness, 0, 200)) / 100.0f;
    }
    if (warpDrawn) {  // 2.7.54: the part, the time (2.7.56: the spikes breathe) and the strength (portal_draw_ps)
        c.sourceRect[3] = float(int(warp));
        c.twin[3] = waterTime();
        c.footprint = float(std::clamp(effects.portalsStrength, 0, 200)) / 100.0f;
    }
    // 2.5: a draw whose constants carry none of the world's extras (shade()'s bits: relief, leaves, material,
    // patches, seams; hills, the sprites' light; soft particles) takes the lean shader - the
    // pipeline is chosen here, once the constants are known, and set before the draw
    const bool lean = shader == DrawShader::Game && !water &&
                      (c.stage1[1] & (1u << 26 | 1u << 27 | 1u << 28 | 1u << 30 | 1u << 31)) == 0 &&
                      (c.stage0[1] & (1u << 26 | 1u << 28)) == 0 && (c.alphaTest & 1u << 20) == 0;
    if (lean) {
        ID3D12PipelineState* leanPso = pipelineFor(laid, d.fvf, coverage, toLayer, toGlow, water, mask, shader, true);
        if (leanPso) pso = leanPso;
    }
    // 2.5: the draw goes on the gathered call when its state is the same and its vertices follow (draw_batch.hpp);
    // by itself as before: merging off, the game's own indices, a quad without the pattern
    next.pso = pso;
    next.c = c;
    next.topology = d.primitive == d3d7::PT_TRIANGLESTRIP ? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
                                                          : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    next.indexCount = d.indexCount;
    next.firstIndex = d.firstIndex;
    const UINT stride = FrameRecord::stride(d.fvf);
    batch::Draw shape{batch::kindOf(d.primitive, d.vertexCount, d.indexCount != 0), d.vertexOffset, d.vertexCount,
                      stride};
    if (!mergeDraws || (shape.kind == batch::Kind::Quads && !ensureQuadIndices())) shape.kind = batch::Kind::Single;
    ++drawsGathered;
    if (pendingBatch.joins(shape) && next.sameState(pendingDraw)) {
        pendingBatch.add(shape);
        return;
    }
    flushDraws();
    pendingDraw = next;
    pendingBatch.start(shape);
    if (shape.kind == batch::Kind::Single) flushDraws();
}

// 1.7: the trees' settings for a draw of leaves (draw_constants.hpp leafShading).
LeafLook Renderer::Impl::leafLook() const {
    LeafLook l;
    l.volume = effects.treeVolume;
    l.through = effects.leafLight;
    l.tints = effects.treeTints;
    sunOnScreen(l.sun);
    const float night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    l.throughLight = trees::throughLight(daylight::at(effects.dayCycle ? effects.dayNow : -1).length, night);
    l.season = effects.treeSeasons ? mapSeason : trees::Season::Green;  // 2.4: autumn's and Lutie's a box of their own
    l.viewToWorld = viewToWorldValid ? viewToWorld : nullptr;
    l.projection = camera::projectionFor(gameWidth, gameHeight);
    return l;
}


// 1.7: the water's life (water_ps waterLife's bits): 2 a swamp's bubbles (1 the fish and 4 the surf's swash gone in
// 2.7.2).
float Renderer::Impl::waterLife() const {
    // 2.4: and the water's own parts, each a box of its own: 8 the foam along the shore, 16 the sun's glints, 32 the
    // light on the bed
    return float((effects.swamp && mapPlace.swamp ? 2 : 0) + (effects.waterFoam ? 8 : 0) +
                 (effects.waterGlints ? 16 : 0) +
                 (effects.caustics ? 32 : 0));
}

// 1.7: the water's level in the world (its first corner) - the mirror's plane, the creatures' water. 2.7.3: and the
// highest any corner has come on this map (waterTop) - the grass's line: the game's waves lift the water and let it
// fall, and a shallow bed between their low and high grew blades and lost them over and over (cmd_fild01's pond).
void Renderer::Impl::noteWaterLevel(const FrameRecord& rec, const DrawCmd& d) {
    if (!viewToWorldValid || waterLevelNoted) return;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const float* m = viewToWorld;
    float top = 0.0f;
    int seen = 0;
    for (uint32_t k = 0; k < 4 && k < d.vertexCount; ++k) {
        if (d.vertexOffset + k * stride + 16 > v.size()) break;
        float xyzw[4];
        std::memcpy(xyzw, v.data() + d.vertexOffset + k * stride, sizeof xyzw);
        if (xyzw[3] <= 0.0f) continue;
        const float vz = 1.0f / xyzw[3], vx = (xyzw[0] - p.cx) * vz / p.f0, vy = (xyzw[1] - p.cy) * vz / p.f1;
        const float y = vx * m[1] + vy * m[4] + vz * m[7] + m[10];
        if (k == 0) waterLevel = y;
        top = seen ? std::min(top, y) : y;  // up is -y
        ++seen;
    }
    if (!seen) return;
    waterTop = waterLevelKnown ? std::min(waterTop, top) : top;
    waterLevelNoted = true;
    waterLevelKnown = true;
}

}  // namespace uagfx::gpu
