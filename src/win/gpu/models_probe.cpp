#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/game_models_place.hpp"
#include "core/model_probe.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

// 3.0.20-beta.61 (R-HBPQ54 «зникли частини мосту» on gef_fild07, R-3W4TYM «теж моста немає» on yuno_fild01, the
// owner's «там где портал, там дырка, которая пробивает его насквозь» in Prontera): faces of models we draw are not
// in the picture - a bridge's deck over water, a lane of its paving - though the frame lists their nodes, the card
// keeps their faces and the processor's count of the card's rules says they show. What the code says is drawn and
// what the picture holds part somewhere between the draw and the card; the models' dump (every report runs it) now
// looks at the picture itself. For each texture of our still nodes near the player: its largest faces' places on the
// depth target (core/model_probe), the depth read back there right after our draw - the face itself, something
// nearer, or what is behind it (the face not drawn). The ones not drawn are drawn again in turn - without the
// material, with the game's own picture in place of its HD copy, without the alpha test, placed by the processor in
// place of the card - and looked at after each: the first way that brings a face names what loses it. A report's
// frame alone; the card waits for each reading (a few milliseconds).
namespace uagfx::gpu {

namespace {

constexpr float kProbeReach = 150.0f;  // of the player, as the dump's own
constexpr UINT64 kPlace = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
constexpr int kNoPicture = 1, kNoMaterial = 2;  // ModelProbe::flags

}  // namespace

// the points' depths as the picture holds them now: copied a texel each and read at once
bool Renderer::Impl::probeDepths() {
    ModelProbe& mp = modelProbe;
    mp.depths.assign(mp.points.size(), 2.0f);
    flushDraws();
    if (!open || !copySceneDepth()) return false;
    const UINT64 size = UINT64(ModelProbe::kMostPoints) * kPlace;
    if (!mp.buffer) {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC d{};
        d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        d.Width = size;
        d.Height = 1;
        d.DepthOrArraySize = 1;
        d.MipLevels = 1;
        d.SampleDesc.Count = 1;
        d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COPY_DEST,
                                                   nullptr, __uuidof(ID3D12Resource),
                                                   reinterpret_cast<void**>(&mp.buffer)))) {
            mp.buffer = nullptr;
            return false;
        }
    }
    transition(sceneDepth, sceneDepthState, D3D12_RESOURCE_STATE_COPY_SOURCE);
    for (size_t i = 0; i < mp.points.size(); ++i) {
        D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
        dst.pResource = mp.buffer;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint.Offset = UINT64(i) * kPlace;
        dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32_FLOAT;
        dst.PlacedFootprint.Footprint.Width = 1;
        dst.PlacedFootprint.Footprint.Height = 1;
        dst.PlacedFootprint.Footprint.Depth = 1;
        dst.PlacedFootprint.Footprint.RowPitch = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
        src.pResource = sceneDepth;
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        const UINT x = UINT(mp.points[i].x), y = UINT(mp.points[i].y);
        const D3D12_BOX box{x, y, 0, x + 1, y + 1, 1};
        list->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);
    }
    transition(sceneDepth, sceneDepthState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    const UINT64 before = fenceValue;
    submit();
    if (fenceValue == before) return false;  // the list did not reach the card
    waitIdle();
    if (lost) return false;
    uint8_t* data = nullptr;
    const D3D12_RANGE all{0, SIZE_T(size)};
    const bool read = SUCCEEDED(mp.buffer->Map(0, &all, reinterpret_cast<void**>(&data))) && data;
    if (read) {
        for (size_t i = 0; i < mp.points.size(); ++i) std::memcpy(&mp.depths[i], data + i * kPlace, sizeof(float));
        const D3D12_RANGE none{0, 0};
        mp.buffer->Unmap(0, &none);
    }
    return begin() && read;  // the frame's next draws need a list
}

// the runs still missing drawn again in the step's way
void Renderer::Impl::probeAgain(int step, const own_ground::Shading& fog) {
    ModelProbe& mp = modelProbe;
    const gamemodels::Frame& frame = gameModelsNow.frame();
    DrawSetup st = ownState.modelLike;
    st.cull = d3d7::CULL_NONE;
    if (step >= 3) st.alphaTest = st.colorKey = false;
    mp.rec.reset();
    mp.counts.clear();
    const uint32_t state = mp.rec.addState(st);
    const int flags = step >= 2 ? kNoPicture | kNoMaterial : kNoMaterial;
    const auto issue = [&](uint32_t texture, const void* vertices, uint32_t count, UINT faces) {
        DrawCmd cmd;
        cmd.state = state;
        cmd.texture[0] = texture;
        cmd.primitive = d3d7::PT_TRIANGLELIST;
        cmd.fvf = d3d7::FVF_TL1;
        mp.rec.draw(cmd, vertices, count, nullptr, 0);
        mp.counts.push_back(faces);
    };
    if (step < 4) {  // from the card: a texture's faces a draw, their numbers eight a vertex (as drawGameModelsCard)
        std::vector<uint32_t> done;
        for (const ModelProbe::Run& r : mp.runs) {
            if (!r.missing || std::find(done.begin(), done.end(), r.texture) != done.end()) continue;
            done.push_back(r.texture);
            mp.packed.clear();
            for (const ModelProbe::Run& o : mp.runs)
                if (o.missing && o.texture == r.texture)
                    for (uint32_t f = 0; f < o.count; ++f) mp.packed.push_back(o.first + f);
            const UINT faces = UINT(mp.packed.size());
            size_t packed = (mp.packed.size() + 7) / 8;
            if (packed == 4) packed = 5;  // never four: draw() takes those for a sprite's quad
            mp.packed.resize(packed * 8, 0u);
            issue(r.texture, mp.packed.data(), uint32_t(packed), faces);
        }
    } else {  // by the processor: each node's missing textures placed as the game's draw would place them
        own_ground::View view;
        if (!own_ground::viewOf(viewToWorld, view)) return;
        gamemodels::Picture pic;
        pic.view = view;
        pic.p = camera::projectionFor(gameWidth, gameHeight);
        pic.how = gamemodels::colouringOf(frame);
        pic.fog = fog.fogged() ? &fog : nullptr;
        pic.plain = fog.specularAt(0.0f);
        gamemodels::Placed out;
        std::vector<gamemodels::Corner> scratch;
        std::vector<uint32_t> done;
        for (const ModelProbe::Run& r : mp.runs) {
            if (!r.missing || std::find(done.begin(), done.end(), r.node) != done.end()) continue;
            done.push_back(r.node);
            const auto found = gameModels.find(r.node);
            if (found == gameModels.end()) continue;
            const gamemodels::Node& n = found->second;
            std::vector<bool> only(n.textures.size(), false);
            for (const ModelProbe::Run& o : mp.runs)
                if (o.missing && o.node == r.node)
                    for (size_t t = 0; t < n.textures.size(); ++t)
                        if (n.textures[t] == o.texture) only[t] = true;
            out.clear();
            gamemodels::place(n, pic, scratch, out, &only, nullptr);
            for (size_t t = 0; t < out.textures(); ++t) {
                const std::vector<own_ground::Tl1>& v = out.vertices(t);
                if (v.empty() || v.size() > 65535) continue;
                issue(out.texture(t), v.data(), uint32_t(v.size()), 0);
            }
        }
    }
    if (mp.rec.draws().empty()) return;
    flushDraws();  // the draws gathered so far are of the record's vertices before these
    prepareGeometry(mp.rec);
    if (vertexBase) {
        mp.flags = flags;
        ownPass = true;
        cardModelsDraw = step < 4;
        if (cardModelsDraw) cardFog = &fog;
        cardMatrices = 0;  // still nodes alone
        for (size_t k = 0; k < mp.rec.draws().size(); ++k) {
            cardCount = mp.counts[k];
            draw(mp.rec, mp.rec.draws()[k]);
        }
        flushDraws();
        ownPass = cardModelsDraw = false;
        cardCount = 0;
        cardFog = nullptr;
        mp.flags = 0;
    }
    geometryList = 0;  // the game's record's vertices go up again with its next draw
}

void Renderer::Impl::probeModels(const own_ground::Shading& fog) {
    ModelProbe& mp = modelProbe;
    const gamemodels::Frame& frame = gameModelsNow.frame();
    if (!frame.dump || mp.done) return;
    mp.done = true;
    own_ground::View view;
    if (!cardModelsReady() || !ownState.modelLikeKnown || !viewToWorldValid || !open || lost ||
        !own_ground::viewOf(viewToWorld, view) || !ensureScene("models dump")) {
        LOG_INFO("models dump: the picture not looked at - our models are not drawn from the card now");
        return;
    }
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const float k = float(upscale ? worldScale : scale);
    const float jit[2] = {upscale ? jitter[0] : 0.0f, upscale ? jitter[1] : 0.0f};
    mp.runs.clear();
    mp.points.clear();
    const std::vector<uint32_t>& words = gameCards.words();
    const bool leavesApart = leavesOwnWay();
    // 3.0.20-beta.65 (R-64G2U9: Prontera's frame holds hundreds of nodes near the player - cherry petals, lamps,
    // paving): every near run looked over, the ones with the largest faces on the picture kept (a house's walls,
    // a bridge's deck), not the frame's first ones
    struct Found {
        ModelProbe::Run run;
        modelprobe::Point best[2];
        float area;
    };
    std::vector<Found> largest;
    for (const uint32_t id : frame.ids) {
        const auto found = gameModels.find(id);
        const std::vector<gamemodels::CardRun>* runs = gameCards.runsOfNode(id);
        if (found == gameModels.end() || !runs || found->second.moving) continue;
        for (const gamemodels::CardRun& r : *runs) {
            if (!r.count || (leavesApart && leafyTexture(r.texture))) continue;  // the leaves sway: not theirs
            if (std::fabs((r.lo[0] + r.hi[0]) / 2 - frame.dumpAt[0]) > kProbeReach ||
                std::fabs((r.lo[2] + r.hi[2]) / 2 - frame.dumpAt[1]) > kProbeReach)
                continue;
            modelprobe::Point best[2];
            int bests = 0;
            for (uint32_t f = 0; f < r.count && f < 512; ++f) {
                const size_t at = size_t(r.first + f) * gamemodels::kCardFaceWords;
                if (at + gamemodels::kCardFaceWords > words.size()) break;
                float v[3][3];
                for (int c = 0; c < 3; ++c) {
                    float w[3];
                    std::memcpy(w, &words[at + size_t(4 * c)], sizeof w);
                    own_ground::toView(view, w, v[c]);
                }
                const uint32_t bits = words[at + 18];  // 1 two-sided, 2 its node mirrored - model_vs's rule
                if (!(bits & 1u) && gamemodels::turnedInView(v[0], v[1], v[2], p.f0 * p.f1) == ((bits & 2u) != 0))
                    continue;
                modelprobe::Point pt;
                if (!modelprobe::pointOf(v[0], v[1], v[2], p, k, jit, sceneWidth, sceneHeight, pt)) continue;
                if (bests < 2) {
                    best[bests++] = pt;
                } else {
                    const int least = best[0].area < best[1].area ? 0 : 1;
                    if (pt.area > best[least].area) best[least] = pt;
                }
            }
            if (!bests) continue;
            Found f;
            f.run.node = id, f.run.texture = r.texture, f.run.first = r.first, f.run.count = r.count;
            f.run.points = size_t(bests);
            f.best[0] = best[0], f.best[1] = best[bests > 1 ? 1 : 0];
            f.area = std::max(best[0].area, bests > 1 ? best[1].area : 0.0f);
            largest.push_back(f);
        }
    }
    std::sort(largest.begin(), largest.end(), [](const Found& a, const Found& b) { return a.area > b.area; });
    for (Found& f : largest) {
        if (mp.points.size() + f.run.points > ModelProbe::kMostPoints) break;
        f.run.firstPoint = mp.points.size();
        for (size_t b = 0; b < f.run.points; ++b) mp.points.push_back(f.best[b]);
        mp.runs.push_back(f.run);
    }
    const size_t nearRuns = largest.size();
    const DrawSetup& like = ownState.modelLike;
    char state[240];
    std::snprintf(state, sizeof state,
                  "alpha test %d (func %u, ref %u), colour key %d, depth %d/%d (func %u), blend %d, stage 0 colour "
                  "%u alpha %u; %u sample(s), alpha to coverage %d",
                  int(like.alphaTest), unsigned(like.alphaFunc), unsigned(like.alphaRef), int(like.colorKey),
                  int(like.zEnable), int(like.zWrite), unsigned(like.zFunc), int(like.blend),
                  unsigned(like.stage[0].colorOp), unsigned(like.stage[0].alphaOp), unsigned(upscale ? 1 : samples),
                  int(alphaToCoverage));
    if (mp.points.empty()) {
        LOG_INFO("models dump: the picture looked at - no face of our still nodes near the player to judge by (%zu "
                 "nodes this frame); our models' state: %s", frame.ids.size(), state);
        return;
    }
    if (!probeDepths()) {
        LOG_INFO("models dump: the picture not looked at - its depth could not be read back");
        return;
    }
    size_t missing = 0, hidden = 0;
    for (ModelProbe::Run& r : mp.runs) {
        for (size_t i = 0; i < r.points; ++i) {
            const modelprobe::Point& pt = mp.points[r.firstPoint + i];
            const float depth = mp.depths[r.firstPoint + i];
            const modelprobe::Seen seen = modelprobe::judge(pt, depth);
            if (seen == modelprobe::Seen::There) {
                ++r.there[0];
            } else if (seen == modelprobe::Seen::Nearer) {
                ++r.nearer;
            } else if (!r.missing) {
                r.missing = true;
                r.want[0] = pt.lo, r.want[1] = pt.hi, r.found = depth;
            }
        }
        missing += r.missing;
        hidden += r.nearer != 0;
    }
    const size_t missingFirst = missing;
    int reached = 0;
    std::vector<int> cameBy(mp.runs.size(), 0);  // the step that brought a missing run; 0 none
    for (int step = 1; step < ModelProbe::kSteps && missing; ++step) {
        probeAgain(step, fog);
        if (!probeDepths()) break;
        reached = step;
        missing = 0;
        for (size_t n = 0; n < mp.runs.size(); ++n) {
            ModelProbe::Run& r = mp.runs[n];
            if (!r.missing) continue;
            for (size_t i = 0; i < r.points; ++i)
                if (modelprobe::judge(mp.points[r.firstPoint + i], mp.depths[r.firstPoint + i]) ==
                    modelprobe::Seen::There)
                    ++r.there[step];
            if (r.there[step] + r.nearer >= int(r.points)) {
                r.missing = false;
                cameBy[n] = step;
            }
            missing += r.missing;
        }
    }
    static const char* const kWay[ModelProbe::kSteps] = {
        "our own draw", "without its material", "with the game's own picture (no HD copy)", "without the alpha test",
        "placed by the processor, not the card"};
    LOG_INFO("models dump: the picture looked at where our faces should be - %zu texture(s) of our still nodes near "
             "the player (the largest on the picture of %zu), %zu place(s) on the %dx%d depth: %zu not in the picture "
             "after our draw, %zu behind something nearer; our models' state: %s",
             mp.runs.size(), nearRuns, mp.points.size(), sceneWidth, sceneHeight, missingFirst, hidden, state);
    size_t told = 0;
    for (size_t n = 0; n < mp.runs.size() && told < 24; ++n) {
        const ModelProbe::Run& r = mp.runs[n];
        if (r.there[0] == int(r.points)) continue;
        ++told;
        const auto t = textures.find(r.texture);
        char facts[160] = "not kept by the renderer";
        if (t != textures.end())  // its holes: a place on one shows what is behind it with the face drawn all right
            std::snprintf(facts, sizeof facts, "%dx%d, %d %% of it holes%s%s%s", t->second.width, t->second.height,
                          int(t->second.holes), t->second.leafy ? ", leafy" : "",
                          t->second.ai ? ", an HD copy" : "", t->second.mat ? ", a material" : "");
        const bool lost = r.there[0] + r.nearer < int(r.points);  // some of it not drawn
        std::string ways;
        for (int step = 1; lost && step <= reached; ++step) {
            ways += std::string("; drawn again ") + kWay[step] + ": " + std::to_string(r.there[step]) + " there";
            if (cameBy[n] == step) break;
        }
        LOG_INFO("models dump: ours %08X texture %u (%s), %u faces - %zu place(s): %d there, %d behind something "
                 "nearer%s%s%s",
                 r.node, r.texture, facts, r.count, r.points, r.there[0], r.nearer,
                 lost ? (std::string("; NOT DRAWN at the rest - its depth ") + std::to_string(r.want[0]) + ".." +
                         std::to_string(r.want[1]) + ", the picture's " + std::to_string(r.found) +
                         " (what is behind it)").c_str()
                      : "",
                 ways.c_str(),
                 !lost                               ? ""
                 : cameBy[n]                         ? " - THAT WAY BRINGS IT"
                 : reached == ModelProbe::kSteps - 1 ? " - NO WAY BROUGHT IT"
                                                     : " - the other ways not tried");
    }
}

}  // namespace uagfx::gpu
