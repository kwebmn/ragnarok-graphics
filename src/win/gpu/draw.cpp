#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>

#include "core/ai/kinds.hpp"
#include "core/d3d7.hpp"
#include "core/effect_exe.hpp"
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
    if (!s.ui && s.blend && aoDue() && !applyAo()) return;
    if (!s.ui && s.blend && sunShadowDue() && !applySunShadows()) return;  // 0.77: likewise
    if (!s.ui && s.blend && grassDue() && !applyGrass()) return;  // 0.81: the grass under the characters
    if (!s.ui && s.blend && cloudsDue() && !applyClouds()) return;  // 0.33: the clouds' shadows, likewise
    // 0.32: the characters' shadows likewise - from this draw on the frame's characters stand on the ground - and
    // the game's round ones are in them.
    if (!s.ui && s.blend && actorShadowsDue() && !applyActorShadows(rec, size_t(&d - rec.draws().data()))) return;
    if (!s.ui && d.effect == uint8_t(effect_exe::Kind::Shadow) && feetWanted()) noteFoot(rec, d);  // 0.94
    if (!s.ui && actorShadowsDrawn && d.effect == uint8_t(effect_exe::Kind::Shadow)) return;
    // 0.21: the world done - at the interface - its colours, then its glow.
    if (s.ui && !upscale && !finishWorld()) return;
    if (s.ui && !upscale && glowDrawn && !addGlowToTarget()) return;
    const bool adds = !s.ui && s.blend && s.dstBlend == d3d7::BLEND_ONE;
    if (adds && !glowActive && glowWanted()) startGlow();
    // 0.26: the fire of skills (a fire-coloured picture added on) lights around it once the world is drawn.
    // 0.38: a portal's draw: its middle, for its glow and its swirl once the world is drawn.
    if (!s.ui && d.effect == uint8_t(effect_exe::Kind::Portal) && portalsWanted() && !torchDone) notePortal(rec, d);
    // 0.36: the other skills light too, in their pictures' colours - not the aura, the maps' objects (their own).
    if (adds && fireWanted() && !torchDone && d.texture[0]) {
        const auto ft = textures.find(d.texture[0]);
        const bool mine = d.effect == uint8_t(effect_exe::Kind::None) || d.effect == uint8_t(effect_exe::Kind::Skill);
        if (ft != textures.end() && ft->second.fire > 0.15f && mine) {
            noteFire(rec, d, ft->second.fire, ft->second.fireColour);
        } else if (ft != textures.end() && effects.skillColours && ft->second.light > 0.2f && mine &&
                   (objectTextures.empty() || objectTextures.of(d.texture[0]) == objects::Kind::None)) {
            noteFire(rec, d, ft->second.light, ft->second.lightColour);
        }
    }
    const bool toGlow = !s.ui && glowActive;
    // 0.19, modern water: a cell of a map's water (core/water.hpp) is drawn by its own shader, from the picture and
    // the depth the frame had before its first water (copyScene); without them, as the game draws it.
    bool water = false;
    if (!s.ui && waterWanted() && d.fvf == d3d7::FVF_TL1 && d.vertexCount == 4 && s.blend &&
        s.srcBlend == d3d7::BLEND_SRCALPHA && s.dstBlend == d3d7::BLEND_INVSRCALPHA) {
        const std::vector<uint8_t>& v = rec.vertexBytes();
        const size_t stride = FrameRecord::stride(d.fvf);
        water = d.vertexOffset + 4 * stride <= v.size() &&
                water::isWaterQuad(v.data() + d.vertexOffset, stride, d.vertexCount, d.indexCount,
                                   d.primitive == d3d7::PT_TRIANGLESTRIP) &&
                copyScene();
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
    if (!s.ui && !water && !smoke && lavaWanted() && d.fvf == d3d7::FVF_TL1 && d.vertexCount == 4 && s.blend &&
        s.srcBlend == d3d7::BLEND_SRCALPHA && s.dstBlend == d3d7::BLEND_INVSRCALPHA && lavaMap()) {
        const std::vector<uint8_t>& v = rec.vertexBytes();
        const size_t stride = FrameRecord::stride(d.fvf);
        lava = d.vertexOffset + 4 * stride <= v.size() &&
               water::isLavaQuad(v.data() + d.vertexOffset, stride, d.vertexCount, d.indexCount,
                                 d.primitive == d3d7::PT_TRIANGLESTRIP);
        if (lava && !torchDone) noteLava(rec, d);
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
    const bool effectDraw = adds || d.effect == uint8_t(effect_exe::Kind::Skill) ||
                            d.effect == uint8_t(effect_exe::Kind::Portal);
    bool soft = !s.ui && !water && !smoke && !ray && !lava && quad && effects.softParticles && s.blend &&
                s.zEnable && !s.zWrite && effectDraw && s.srcBlend != d3d7::BLEND_ZERO &&
                s.srcBlend != d3d7::BLEND_DESTCOLOR && softCopied >= 0;
    if (soft && !softCopied) softCopied = ensureScene("soft particles") && copySceneDepth() ? 1 : -1;
    soft = soft && softCopied > 0;
    const DrawShader shader = smoke ? DrawShader::Smoke
                              : ray ? DrawShader::Ray
                              : lava ? DrawShader::Lava
                                     : DrawShader::Game;
    ID3D12PipelineState* pso = pipelineFor(s, d.fvf, coverage, toLayer, toGlow, water, mask, shader);
    if (!pso) return;
    // AI textures have mip levels of their own, sampled also where the game uses none (its sprite cache, mipmaps
    // off in its options): four times the texels, shrunk on the screen, would shimmer.
    StageSetup sampled[2] = {s.stage[0], s.stage[1]};
    bool aiDrawn[2] = {false, false};  // the stage's texture is the AI's
    if (ai && ai->shown && !(d.twin && s.ui))
        for (int i = 0; i < 2; ++i) {
            const auto it = d.texture[i] ? textures.find(d.texture[i]) : textures.end();
            aiDrawn[i] = it != textures.end() && it->second.ai && (s.ui ? it->second.aiUi : it->second.aiWorld) &&
                         (!s.ui || aiInterfaceAt(scale));  // 0.89: not the interface at the game's own size
            const bool mips = sampled[i].mipFilter == d3d7::TFP_POINT || sampled[i].mipFilter == d3d7::TFP_LINEAR;
            if (aiDrawn[i] && !mips) sampled[i].mipFilter = d3d7::TFP_LINEAR;
        }
    // A new list, if this draw needs one, starts before the draw records anything; the record's geometry then goes
    // into the new list's slot (1B review M4: the old slot is reused while the new list may still read it).
    if (!makeRoom(sampled[0], sampled[1], aniso, water || soft ? 5 : 2)) return;
    if (geometryList != listSerial) {  // also after a list that failed to close: its slot is used again (0.4.1 review)
        prepareGeometry(rec);
        if (!vertexBase) return;
    }
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
    const D3D12_GPU_DESCRIPTOR_HANDLE samplers = samplerTable(sampled[0], sampled[1], aniso);
    bindPass(toWorld ? (toGlow ? kPassWorldGlow : kPassWorld)
                     : toLayer ? kPassLayer : toGlow ? kPassTargetGlow : kPassTarget);
    worldDrawn = worldDrawn || toWorld;
    layerDrawn = layerDrawn || toLayer;
    glowDrawn = glowDrawn || (toGlow && adds);
    // Stage 4 review: the interface's edges fall on pixel centres (whole D3D7 positions, x * k + 0.5); with every
    // sample there it is drawn exactly as with one sample, while the world keeps its smoothed edges.
    if (samples > 1) placeSamples(s.ui);
    if (!boundPipeline) {  // a fresh list or after present(): the root signature and its tables are gone
        list->SetGraphicsRootSignature(root);
        boundTextures = ~0ull;
        boundSamplers = ~0ull;
    }
    if (pso != boundPipeline) {
        list->SetPipelineState(pso);
        boundPipeline = pso;
    }
    const uint64_t textureKey = uint64_t(srv[0]) << 32 | srv[1];
    if (textureKey != boundTextures) {
        list->SetGraphicsRootDescriptorTable(kRootTextures, textureTable(srv[0], srv[1]));
        boundTextures = textureKey;
    }
    if (samplers.ptr != boundSamplers) {
        list->SetGraphicsRootDescriptorTable(kRootSamplers, samplers);
        boundSamplers = samplers.ptr;
    }
    if (water) {  // the picture and the depth under the water (t2, t3)
        transition(sceneColor, sceneColorState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        transition(sceneDepth, sceneDepthState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        list->SetGraphicsRootDescriptorTable(kRootScene, sceneTable(sceneColorSrv, sceneDepthSrv, sceneColorSrv));
        if (!waterCells++) LOG_INFO("water: the map's water found, drawn anew");
    } else if (soft) {  // the depth behind the effect (t3)
        transition(sceneColor, sceneColorState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        transition(sceneDepth, sceneDepthState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        list->SetGraphicsRootDescriptorTable(kRootScene, sceneTable(sceneColorSrv, sceneDepthSrv, sceneColorSrv));
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
    // 0.22.6: the ground's lightmap (its 256x256 atlas of 8x8 cells) sampled smoothly.
    if (effects.smoothLight && !s.ui && d.fvf == d3d7::FVF_TL2 && t[1] && t[1]->width == 256 && t[1]->height == 256)
        c.stage1[1] |= 1u << 25;
    c.alphaTest = s.alphaTest ? uint32_t(s.alphaFunc) | uint32_t(s.alphaRef) << 8 | 1u << 16 : 0u;
    if (coverage) c.alphaTest |= 1u << 17;
    if (toLayer && !s.blend) c.alphaTest |= 1u << 18;  // part 3: opaque in the layer (0.8 review I2)
    // 0.18: its colour into the glow target - light it adds, or a factor it multiplies the light under it by.
    if (toGlow && (adds || (s.blend && (s.srcBlend == d3d7::BLEND_ZERO || s.srcBlend == d3d7::BLEND_DESTCOLOR))))
        c.alphaTest |= 1u << 19;
    if (soft) {  // 0.67: its fade (shaders.cpp shade())
        c.alphaTest |= 1u << 20;
        if (s.srcBlend == d3d7::BLEND_ONE) c.alphaTest |= 1u << 21;  // added or premultiplied: the colour too
        const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
        const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
        std::memcpy(c.projection, proj, sizeof proj);
    }
    c.fogEnable = s.fog ? 1u : 0u;
    c.fogColor[0] = float((s.fogColor >> 16) & 255) / 255.0f;
    c.fogColor[1] = float((s.fogColor >> 8) & 255) / 255.0f;
    c.fogColor[2] = float(s.fogColor & 255) / 255.0f;
    c.fogColor[3] = 1.0f;
    c.colorKey = s.colorKey ? 1u : 0u;
    c.uiMode = uint32_t(uiFilterMode(s.ui, scale, uiSharp ? UiFilter::Sharp : UiFilter::Smooth));
    // The AI's picture has the detail the game pixels lack: sampled as it is, not one tap per game pixel.
    if (aiDrawn[0]) c.uiMode = 0;
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
    }
    float swayTop = 0.0f, swayBottom = 0.0f;  // 0.64: leaves in the wind
    if (!s.ui && !twin && c.uiMode == 0 && effects.foliage && effects.foliageStrength > 0 && t[0] && t[0]->leafy &&
        d.fvf != d3d7::FVF_TL2 && yExtent(rec, d, swayTop, swayBottom)) {
        c.twin[0] = 1.2f * float(effects.foliageStrength) / 100.0f;
        c.twin[1] = swayBottom;
        c.twin[2] = swayBottom - swayTop;
        c.twin[3] = waterTime();
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
    list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
    list->IASetPrimitiveTopology(d.primitive == d3d7::PT_TRIANGLESTRIP ? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
                                                                       : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const UINT stride = FrameRecord::stride(d.fvf);
    D3D12_VERTEX_BUFFER_VIEW vb{vertexBase + d.vertexOffset, d.vertexCount * stride, stride};
    list->IASetVertexBuffers(0, 1, &vb);
    if (d.indexCount) {
        D3D12_INDEX_BUFFER_VIEW ib{indexBase, indexBytes, DXGI_FORMAT_R16_UINT};
        list->IASetIndexBuffer(&ib);
        list->DrawIndexedInstanced(d.indexCount, 1, d.firstIndex, 0, 0);
    } else {
        list->DrawInstanced(d.vertexCount, 1, 0, 0);
    }
}

}  // namespace uagfx::gpu
