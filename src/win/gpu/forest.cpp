#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/ai/kinds.hpp"
#include "core/d3d7.hpp"
#include "core/effect_exe.hpp"
#include "core/forest.hpp"
#include "core/game_camera.hpp"
#include "core/trees.hpp"
#include "win/gpu/ai_textures.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

constexpr float kCell = 32.0f;  // the trees' grid, world units
constexpr int kVariants = 3;    // trees of a model, each its own

int64_t cellKey(float x, float z) {
    return int64_t(int32_t(std::floor(x / kCell))) << 32 | int64_t(uint32_t(int32_t(std::floor(z / kCell))));
}

uint32_t mixBits(uint32_t h) {
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

// An instance as forest_vs reads it: the place and turn, the scale and number, the leaves' and the bark's colours.
struct InstanceData {
    float at[4];
    float size[4];
    float leaf[4];
    float bark[4];
};
static_assert(sizeof(InstanceData) == 64, "the forest's instance");
static_assert(sizeof(InstanceData) == sizeof(std::array<float, 16>), "forestData's");

// A buffer the processor writes and the card reads (the forest's: written once a map, its colours now and then).
ID3D12Resource* uploadBuffer(ID3D12Device* device, size_t size) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width = size;
    d.Height = 1;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource* r = nullptr;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_GENERIC_READ,
                                               nullptr, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&r))))
        return nullptr;
    return r;
}

// 2.1: which trees a frame draws. In view: its ball (its middle, its crown's and half its height's reach, the wind's
// sway) in front of the camera and on the screen with a margin of its size; far when it shows under 70 game pixels
// across its half. In the sun's map: its ball within the sun's box.
struct ViewKeep {
    float inv[9], at[3];
    camera::Projection p;
    float width, height;
};

int keepInView(const Renderer::Impl::ForestPlaced& t, const void* with) {
    const ViewKeep& k = *static_cast<const ViewKeep*>(with);
    const float r = std::max(t.radius, t.height * 0.5f) * 1.1f + 2.0f;
    const float x = t.pos[0] - k.at[0], y = t.pos[1] - t.height * 0.5f - k.at[1], z = t.pos[2] - k.at[2];
    const float vx = x * k.inv[0] + y * k.inv[3] + z * k.inv[6], vy = x * k.inv[1] + y * k.inv[4] + z * k.inv[7];
    const float vz = x * k.inv[2] + y * k.inv[5] + z * k.inv[8];
    if (vz + r < k.p.nearZ) return -1;  // behind the camera
    const float zc = std::max(vz, k.p.nearZ + 0.5f);
    const float sx = vx / zc * k.p.f0 + k.p.cx, sy = vy / zc * k.p.f1 + k.p.cy, reach = r * k.p.f0 / zc;
    if (sx + reach < 0.0f || sx - reach > k.width || sy + reach < 0.0f || sy - reach > k.height) return -1;
    return reach < 70.0f && vz > r ? 1 : 0;
}

const float kLeafDefault[3] = {0.30f, 0.47f, 0.18f}, kBarkDefault[3] = {0.36f, 0.27f, 0.20f};

// The game's draw's box in the world (its vertices back through the camera); false when it cannot be.
bool boxOf(const FrameRecord& rec, const DrawCmd& d, const camera::Projection& p, const float* m, float lo[3],
           float hi[3]) {
    const std::vector<uint8_t>& vb = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    if (d.vertexOffset + size_t(d.vertexCount) * stride > vb.size()) return false;
    for (int k = 0; k < 3; ++k) {
        lo[k] = 1e30f;
        hi[k] = -1e30f;
    }
    const uint32_t n = std::min<uint32_t>(d.vertexCount, 4096);
    for (uint32_t i = 0; i < n; ++i) {
        float xyzw[4];
        std::memcpy(xyzw, vb.data() + d.vertexOffset + size_t(i) * stride, sizeof xyzw);
        if (xyzw[3] <= 0.0f) return false;
        const float vz = 1.0f / xyzw[3], vx = (xyzw[0] - p.cx) * vz / p.f0, vy = (xyzw[1] - p.cy) * vz / p.f1;
        const float w[3] = {vx * m[0] + vy * m[3] + vz * m[6] + m[9], vx * m[1] + vy * m[4] + vz * m[7] + m[10],
                            vx * m[2] + vy * m[5] + vz * m[8] + m[11]};
        for (int k = 0; k < 3; ++k) {
            lo[k] = std::min(lo[k], w[k]);
            hi[k] = std::max(hi[k], w[k]);
        }
    }
    return n > 0;
}

}  // namespace

// 2.0: a draw of the game's world that is a tree of the map file (its box within a tree's crown and height) - left
// out, the driver draws that tree; its texture's colour taken for the model's leaves or bark. Until the forest is
// built, the game's crowns are noted (the map file's heights' sign is found from them).
bool Renderer::Impl::hideForTree(const FrameRecord& rec, const DrawCmd& d, const DrawSetup& s) {
    if (s.ui || d.fvf != d3d7::FVF_TL1 || d.vertexCount < 3 || d.effect != uint8_t(effect_exe::Kind::None) ||
        (s.blend && s.dstBlend == d3d7::BLEND_ONE))
        return false;
    const auto tex = d.texture[0] ? textures.find(d.texture[0]) : textures.end();
    const bool leafy = tex != textures.end() && tex->second.leafy;
    if (!forestBuilt && (!leafy || forestSeen.size() >= 48)) return false;
    // 2.1.1: past the first seconds, a draw of a texture no tree had is no tree's - its vertices not read again
    if (forestBuilt && !leafy && GetTickCount64() > forestLearnUntil && !forestTreeTextures.count(d.texture[0]))
        return false;
    if (forestBuilt && d.vertexCount == 4) {  // a sprite's quad: a character, an item - never a tree
        const std::vector<uint8_t>& vb = rec.vertexBytes();
        const size_t stride = FrameRecord::stride(d.fvf);
        if (d.vertexOffset + 4 * stride <= vb.size() &&
            ai::flatQuad(vb.data() + d.vertexOffset, stride, d.vertexCount, d.indexCount))
            return false;
    }
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    float lo[3], hi[3];
    if (!boxOf(rec, d, p, viewToWorld, lo, hi)) return false;
    const float mid[3] = {(lo[0] + hi[0]) * 0.5f, (lo[1] + hi[1]) * 0.5f, (lo[2] + hi[2]) * 0.5f};
    const float half = std::max(hi[0] - lo[0], hi[2] - lo[2]) * 0.5f;
    if (!forestBuilt) {
        if (half > 2.0f && half < 60.0f) forestSeen.push_back(map::Point{mid[0], mid[1], mid[2]});
        return false;
    }
    const auto cell = forestGrid.find(cellKey(mid[0], mid[2]));
    if (cell == forestGrid.end()) return false;
    // 2.0.3: the tree it fits best - the smallest whose crown holds it: a cherry's blossom hung in its crown is the
    // blossom's (2.0.2 gave its colour to the cherry, and the blossom stayed green)
    const ForestPlaced* fit = nullptr;
    for (uint32_t k : cell->second) {
        const ForestPlaced& t = forestPlaced[k];
        const float dx = mid[0] - t.pos[0], dz = mid[2] - t.pos[2];
        if (dx * dx + dz * dz > (t.radius * 1.15f + 1.5f) * (t.radius * 1.15f + 1.5f)) continue;
        if (half > t.radius * 1.5f + 2.0f) continue;
        if (mid[1] > t.pos[1] + 3.0f || mid[1] < t.pos[1] - t.height - 3.0f) continue;  // up along -y
        if (!fit || t.radius < fit->radius) fit = &t;
    }
    if (fit) {
        const ForestPlaced& t = *fit;
        if (tex != textures.end() && tex->second.averaged && t.model < forestColoured.size()) {
            // a blossom's picture is its colour, whatever its alpha (pink or white)
            const forest::Kind kind = t.model < mapWorld.forestModels().size()
                                          ? mapWorld.forestModels()[t.model].shape.kind
                                          : forest::Kind::None;
            const bool asLeaf = leafy || kind == forest::Kind::Blossom;
            const bool trunk = !asLeaf && half < t.radius * 0.5f;
            uint8_t& seen = forestColoured[t.model];
            // 2.0.4: the leaves' picture itself, cut into the cards (forest_ps) - not a cherry's nor a palm's
            if (leafy && kind != forest::Kind::Sakura && kind != forest::Kind::Palm && t.model < forestTexture.size())
                forestTexture[t.model] = d.texture[0];
            // 2.0.4: not a cherry's - its one picture is bark and blossom, its crown came out brown
            if (asLeaf && !(seen & 1) && kind != forest::Kind::Sakura) {
                for (int c = 0; c < 3; ++c) forestLeaf[t.model][size_t(c)] = tex->second.average[c];
                seen |= 1;
                forestColoursDirty = true;
            } else if (trunk && !(seen & 2)) {
                for (int c = 0; c < 3; ++c) forestBark[t.model][size_t(c)] = tex->second.average[c];
                seen |= 2;
                forestColoursDirty = true;
            }
        }
        if (forestTreeTextures.size() < 4096) forestTreeTextures.insert(d.texture[0]);  // 2.1.1
        ++forestHidden;
        return true;
    }
    return false;
}

// 2.0: once a frame - the map read anew: the forest let go; its heights' sign found from the game's crowns; then the
// trees grown and put into buffers of their own (the vertices of each model's kVariants trees, an instance a tree).
void Renderer::Impl::updateForest() {
    mapWorld.poll();
    const bool newMap = mapWorld.serial() != forestSerial;
    if (newMap || forestAnime != effects.animeTrees) {  // 2.0.3: or the other style - grown again, the heights kept
        forestSerial = mapWorld.serial();
        forestAnime = effects.animeTrees;
        for (ID3D12Resource** r : {&forestVb, &forestIb, &forestInstances})
            if (*r) {
                slots[slot].garbage.push_back(*r);
                *r = nullptr;
            }
        forestBuilt = forestLogged = false;
        forestPlaced.clear();
        forestCalls.clear();
        forestGrid.clear();
    }
    if (newMap) {
        forestMismatchLogged = false;
        forestYSign = 0;
        forestSeen.clear();
        forestPlaced.clear();
        forestCalls.clear();
        forestGrid.clear();
        forestLeaf.assign(mapWorld.forestModels().size(), {kLeafDefault[0], kLeafDefault[1], kLeafDefault[2]});
        for (size_t mdl = 0; mdl < mapWorld.forestModels().size(); ++mdl) {  // 2.0.3: blossom pink till seen
            const forest::Kind kind = mapWorld.forestModels()[mdl].shape.kind;
            if (kind == forest::Kind::Sakura || kind == forest::Kind::Blossom) forestLeaf[mdl] = {0.95f, 0.72f, 0.8f};
        }
        forestBark.assign(mapWorld.forestModels().size(), {kBarkDefault[0], kBarkDefault[1], kBarkDefault[2]});
        forestColoured.assign(mapWorld.forestModels().size(), 0);
        forestTexture.assign(mapWorld.forestModels().size(), 0);
    }
    const std::vector<MapWorld::ForestModel>& models = mapWorld.forestModels();
    const std::vector<MapWorld::ForestTree>& trees = mapWorld.forestTrees();
    const map::Axes axes = mapWorld.axes();
    if (!forestBuilt && !models.empty() && !trees.empty() && !axes.known() && !forestMismatchLogged) {
        LOG_INFO("new trees: the maps' axes are not known yet (they are found from the torches seen) - the old new "
                 "trees on %s", mapName.c_str());
        forestMismatchLogged = true;
    }
    if (!forestBuilt && !models.empty() && !trees.empty() && axes.known() && forestSeen.size() >= 6) {
        // the heights' sign: the game's crowns against the file's trees' crowns, as they are and turned
        float best = 1e30f;
        int bestSign = 0;
        for (int sign : {1, -1}) {
            std::vector<float> off;
            for (const map::Point& seen : forestSeen) {
                float nearest = 1e30f, dy = 0.0f;
                for (const MapWorld::ForestTree& t : trees) {
                    const forest::Shape& sh = models[t.model].shape;
                    const float x = float(axes.x) * t.pos[0], z = float(axes.z) * t.pos[2];
                    const float d = (seen.x - x) * (seen.x - x) + (seen.z - z) * (seen.z - z);
                    if (d < nearest) {
                        nearest = d;
                        const float crown = (sh.crownLo[1] + sh.crownHi[1]) * 0.5f * t.scale[1];
                        dy = std::fabs(seen.y - (float(sign) * t.pos[1] + crown));
                    }
                }
                if (nearest < 15.0f * 15.0f) off.push_back(dy);
            }
            if (off.size() < 4) continue;
            std::nth_element(off.begin(), off.begin() + std::ptrdiff_t(off.size() / 2), off.end());
            const float median = off[off.size() / 2];
            if (median < best) {
                best = median;
                bestSign = sign;
            }
        }
        if (bestSign != 0 && best < 12.0f) {
            forestYSign = bestSign;
        } else if (forestSeen.size() >= 48) {
            if (!forestMismatchLogged)
                LOG_INFO("new trees: %s's trees do not match the game's crowns (%.1f units off) - the game's own trees",
                         mapName.c_str(), double(best));
            forestMismatchLogged = true;
            forestSeen.clear();  // tried again with new crowns
        }
    }
    if (!forestBuilt && forestYSign != 0) {
        // the trees in the world, each a model's variant
        for (const MapWorld::ForestTree& t : trees) {
            const forest::Shape& sh = models[t.model].shape;
            ForestPlaced f;
            f.pos[0] = float(axes.x) * t.pos[0];
            f.pos[1] = float(forestYSign) * t.pos[1];
            f.pos[2] = float(axes.z) * t.pos[2];
            f.turn = t.turn * 0.0174533f * float(axes.x * axes.z);
            std::copy(t.scale, t.scale + 3, f.scale);
            const float sx = std::fabs(t.scale[0]), sz = std::fabs(t.scale[2]);
            f.radius = std::max(sh.crownHi[0] - sh.crownLo[0], sh.crownHi[2] - sh.crownLo[2]) * 0.5f * std::max(sx, sz);
            f.height = sh.height * std::fabs(t.scale[1]);
            const uint32_t h = mixBits(uint32_t(int32_t(f.pos[0] * 4.0f)) * 73856093u ^
                                       uint32_t(int32_t(f.pos[2] * 4.0f)) * 19349663u);
            f.model = t.model;
            f.variant = h % kVariants;
            f.seed = float(h >> 8 & 0xffff) / 65535.0f;
            forestPlaced.push_back(f);
        }
        std::stable_sort(forestPlaced.begin(), forestPlaced.end(), [](const ForestPlaced& a, const ForestPlaced& b) {
            return a.model != b.model ? a.model < b.model : a.variant < b.variant;
        });
        for (uint32_t k = 0; k < forestPlaced.size(); ++k) {  // each tree in the cells its crown reaches
            const ForestPlaced& f = forestPlaced[k];
            const float r = f.radius * 1.15f + 1.5f;
            const int x0 = int(std::floor((f.pos[0] - r) / kCell)), x1 = int(std::floor((f.pos[0] + r) / kCell));
            const int z0 = int(std::floor((f.pos[2] - r) / kCell)), z1 = int(std::floor((f.pos[2] + r) / kCell));
            for (int x = x0; x <= x1; ++x)
                for (int z = z0; z <= z1; ++z)
                    forestGrid[cellKey((float(x) + 0.5f) * kCell, (float(z) + 0.5f) * kCell)].push_back(k);
        }
        // the meshes: each model's variants, one after another
        std::vector<forest::Vertex> verts;
        std::vector<uint32_t> indices;
        // first index, count, base vertex, a far tree's count
        std::vector<std::array<uint32_t, 4>> range(models.size() * kVariants);
        for (size_t mdl = 0; mdl < models.size(); ++mdl)
            for (int v = 0; v < kVariants; ++v) {
                const uint32_t seed = uint32_t(mdl) * 7919u + uint32_t(v) * 104729u;
                const forest::Mesh mesh = forest::grow(models[mdl].shape, seed, effects.animeTrees);
                // 2.1: far - its trunk and branches and two leaf cards in five (the cards lie in no order)
                const uint32_t leaves = uint32_t(mesh.indices.size()) - mesh.leafStart;
                const uint32_t farCount = mesh.leafStart + leaves * 2 / 5 / 6 * 6;
                range[mdl * kVariants + size_t(v)] = {uint32_t(indices.size()), uint32_t(mesh.indices.size()),
                                                      uint32_t(verts.size()), farCount};
                verts.insert(verts.end(), mesh.verts.begin(), mesh.verts.end());
                indices.insert(indices.end(), mesh.indices.begin(), mesh.indices.end());
            }
        forestCalls.clear();
        for (uint32_t k = 0; k < forestPlaced.size();) {
            uint32_t e = k;
            while (e < forestPlaced.size() && forestPlaced[e].model == forestPlaced[k].model &&
                   forestPlaced[e].variant == forestPlaced[k].variant)
                ++e;
            const auto& rg = range[size_t(forestPlaced[k].model) * kVariants + forestPlaced[k].variant];
            if (rg[1]) forestCalls.push_back(ForestCall{rg[1], rg[0], rg[2], k, e - k, rg[3]});
            k = e;
        }
        const auto make = [this](ID3D12Resource*& r, uint32_t& bytes, const void* data, size_t size) {
            bytes = uint32_t(size);
            r = size ? uploadBuffer(device, size) : nullptr;
            if (!r) return false;
            void* to = nullptr;
            const D3D12_RANGE none{0, 0};
            if (FAILED(r->Map(0, &none, &to))) return false;
            std::memcpy(to, data, size);
            r->Unmap(0, nullptr);
            return true;
        };
        if (!make(forestVb, forestVbBytes, verts.data(), verts.size() * sizeof(forest::Vertex)) ||
            !make(forestIb, forestIbBytes, indices.data(), indices.size() * sizeof(uint32_t))) {
            LOG_ERROR("new trees: no memory for the forest of %s: the game's own trees", mapName.c_str());
            forestFailed = true;
            return;
        }
        forestInstanceBytes = uint32_t(forestPlaced.size() * sizeof(InstanceData));
        forestInstances = forestInstanceBytes ? uploadBuffer(device, forestInstanceBytes) : nullptr;
        if (!forestInstances) {
            LOG_ERROR("new trees: no memory for the forest of %s: the game's own trees", mapName.c_str());
            forestCalls.clear();
            forestFailed = true;
            return;
        }
        forestColoursDirty = true;
        forestBuilt = true;
        forestTreeTextures.clear();  // 2.1.1: learnt anew for this forest
        forestLearnUntil = GetTickCount64() + 10000;
        LOG_INFO("new trees: %s's forest grown - %zu trees of %zu models, %zu triangles in %zu calls (heights %+d)",
                 mapName.c_str(), forestPlaced.size(), models.size(), indices.size() / 3, forestCalls.size(),
                 forestYSign);
    }
    if (forestBuilt && forestColoursDirty) {  // the instances, their colours as seen so far
        forestColoursDirty = false;
        forestData.resize(forestPlaced.size());
        {
            InstanceData* out = reinterpret_cast<InstanceData*>(forestData.data());
            for (size_t k = 0; k < forestPlaced.size(); ++k) {
                const ForestPlaced& f = forestPlaced[k];
                InstanceData d{};
                d.at[0] = f.pos[0], d.at[1] = f.pos[1], d.at[2] = f.pos[2], d.at[3] = f.turn;
                d.size[0] = f.scale[0], d.size[1] = f.scale[1], d.size[2] = f.scale[2], d.size[3] = f.seed;
                // each tree its own shade, autumn's colours in the season (core/trees.hpp, as 1.7's leaf shading)
                const trees::Shade sh = trees::shadeAt(f.pos[0], f.pos[2], mapSeason);
                // 2.0.4: autumn only on the broadleaf trees, and softer - the firs, palms and the sakura keep their
                // colours (in October Prontera's sakura went brown and orange), a whole crown of orange was too much
                const forest::Kind kind = mapWorld.forestModels()[f.model].shape.kind;
                const bool turns = kind == forest::Kind::Broadleaf || kind == forest::Kind::Willow ||
                                   kind == forest::Kind::Bush;
                const bool blossom = kind == forest::Kind::Sakura || kind == forest::Kind::Blossom;
                for (int c = 0; c < 3; ++c) {
                    float leaf = forestLeaf[f.model][size_t(c)] * (blossom ? 1.0f : sh.tint[c]);
                    if (turns)
                        leaf += (forestLeaf[f.model][size_t(c)] * 1.6f * sh.autumn[c] - leaf) * sh.autumnAmount * 0.55f;
                    d.leaf[c] = leaf;
                    d.bark[c] = forestBark[f.model][size_t(c)];
                }
                std::memcpy(out + k, &d, sizeof d);
            }
        }
    }
}

// 2.1: the frame's trees - each kept by `keep` (left out, near, far), their instances copied into the frame's upload
// in the calls' order, a call for each model's variant and nearness. Only what is in view is drawn, the far trees
// with part of their leaves (on prt_fild08 a frame shows some 40 of its 491 trees).
bool Renderer::Impl::forestPick(int (*keep)(const ForestPlaced&, const void*), const void* with,
                                D3D12_GPU_VIRTUAL_ADDRESS& at, UINT& bytes, std::vector<ForestDraw>& draws) {
    draws.clear();
    if (forestData.size() != forestPlaced.size()) return false;
    std::vector<uint32_t> nearOnes, farOnes;
    std::vector<uint32_t> order;
    for (const ForestCall& call : forestCalls) {
        nearOnes.clear();
        farOnes.clear();
        for (uint32_t k = call.firstInstance; k < call.firstInstance + call.instances; ++k) {
            const int how = keep(forestPlaced[k], with);
            if (how == 0) nearOnes.push_back(k);
            if (how == 1) farOnes.push_back(k);
        }
        const uint32_t model = forestPlaced[call.firstInstance].model;
        if (!nearOnes.empty()) {
            draws.push_back(ForestDraw{call.indexCount, call.firstIndex, call.baseVertex, uint32_t(order.size()),
                                       uint32_t(nearOnes.size()), model});
            order.insert(order.end(), nearOnes.begin(), nearOnes.end());
        }
        if (!farOnes.empty()) {
            draws.push_back(ForestDraw{call.farCount, call.firstIndex, call.baseVertex, uint32_t(order.size()),
                                       uint32_t(farOnes.size()), model});
            order.insert(order.end(), farOnes.begin(), farOnes.end());
        }
    }
    if (order.empty()) return false;
    bytes = UINT(order.size() * sizeof(InstanceData));
    const Upload u = allocUpload(bytes, 16);
    if (!u.cpu) {
        draws.clear();
        return false;
    }
    for (size_t i = 0; i < order.size(); ++i)
        std::memcpy(static_cast<uint8_t*>(u.cpu) + i * sizeof(InstanceData), forestData[order[i]].data(),
                    sizeof(InstanceData));
    at = u.gpu;
    return true;
}

namespace {

// A forest vertex (core/forest.hpp's) and its tree's instance.
const D3D12_INPUT_ELEMENT_DESC kForestLayout[8] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"TEXCOORD", 4, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"TEXCOORD", 5, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1}};

}  // namespace

ID3D12PipelineState* Renderer::Impl::forestPipeline(UINT count) {
    const int index = count >= 8 ? 3 : count >= 4 ? 2 : count >= 2 ? 1 : 0;
    if (forestPipelines[index] || !forestVs || !forestPs) return forestPipelines[index];
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
    p.pRootSignature = root;
    p.VS = {forestVs->GetBufferPointer(), forestVs->GetBufferSize()};
    p.PS = {forestPs->GetBufferPointer(), forestPs->GetBufferSize()};
    p.InputLayout = {kForestLayout, 8};
    p.SampleMask = UINT_MAX;
    p.BlendState.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
    p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    p.RasterizerState.DepthClipEnable = TRUE;
    p.DepthStencilState.DepthEnable = TRUE;
    p.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    p.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    p.NumRenderTargets = 1;
    p.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    p.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    p.SampleDesc.Count = count;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&forestPipelines[index])))) {
        LOG_ERROR("new trees: the forest's pipeline failed: the game's own trees");
        forestPipelines[index] = nullptr;
        forestFailed = true;
    }
    return forestPipelines[index];
}

// The forest's time and wind (forest_vs, forest_sun_vs): the shared wind, or a light breeze of its own.
void Renderer::Impl::forestWind(Constants& c) {
    c.sourceRect[3] = waterTime();
    if (windShared()) {
        c.sourceClamp[0] = windNow.x * windNow.strength;
        c.sourceClamp[1] = windNow.z * windNow.strength;
    } else {
        c.sourceClamp[0] = 0.45f;
        c.sourceClamp[1] = 0.16f;
    }
}

// 2.0: the forest drawn into the world (with its depth) after the grass, before the characters - all its trees in a
// call for each model's variant.
bool Renderer::Impl::applyForest() {
    forestDone = true;
    EffectTime timing(*this, kTimeForest);  // 2.1: its cost in the log and on the Effects page
    ID3D12PipelineState* pso = forestPipeline(upscale ? 1 : samples);
    if (!pso || !forestVb || !forestIb || !forestInstances) return true;
    // 2.0.4: the room for all of it first - the leaves' pictures' tables, the sampler - so that no new list starts in
    // the middle of the calls (it would drop the forest's pipeline and buffers)
    StageSetup leafSampler;
    leafSampler.addressU = leafSampler.addressV = d3d7::TADDRESS_WRAP;
    leafSampler.magFilter = d3d7::TFG_LINEAR;
    leafSampler.minFilter = d3d7::TFN_LINEAR;
    leafSampler.mipFilter = d3d7::TFP_LINEAR;
    if (!makeRoom(leafSampler, leafSampler, 16, UINT(5 + 2 * forestTexture.size()))) return false;
    bindPass(upscale ? kPassWorld : kPassTarget);
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float* m = viewToWorld;
    const float a = m[0], b = m[1], c3 = m[2], d = m[3], e = m[4], g = m[5], h = m[6], i = m[7], k = m[8];
    const float det = a * (e * k - g * i) - b * (d * k - g * h) + c3 * (d * i - e * h);
    if (std::fabs(det) < 1e-8f) return true;
    const float inv[9] = {(e * k - g * i) / det, (c3 * i - b * k) / det, (b * g - c3 * e) / det,
                          (g * h - d * k) / det, (a * k - c3 * h) / det, (c3 * d - a * g) / det,
                          (d * i - e * h) / det, (b * h - a * i) / det, (a * e - b * d) / det};
    Constants c{};
    for (int r = 0; r < 3; ++r) {
        c.reproject[r * 4] = inv[r];
        c.reproject[r * 4 + 1] = inv[3 + r];
        c.reproject[r * 4 + 2] = inv[6 + r];
        c.reproject[r * 4 + 3] = -(inv[r] * m[9] + inv[3 + r] * m[10] + inv[6 + r] * m[11]);
    }
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.targetSize[0] = float(upscale ? renderWidth : width);
    c.targetSize[1] = float(upscale ? renderHeight : height);
    c.scale = float(upscale ? worldScale : scale);
    if (upscale) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    // 2.1: only the trees in view, the far ones with part of their leaves
    ViewKeep view;
    std::copy(inv, inv + 9, view.inv);
    std::copy(m + 9, m + 12, view.at);
    view.p = proj;
    view.width = float(gameWidth);
    view.height = float(gameHeight);
    D3D12_GPU_VIRTUAL_ADDRESS instancesAt = 0;
    UINT instanceBytes = 0;
    std::vector<ForestDraw> draws;
    if (!forestPick(&keepInView, &view, instancesAt, instanceBytes, draws)) return true;
    forestDrawnTrees = instanceBytes / sizeof(InstanceData);
    forestFarTrees = 0;
    for (const ForestDraw& draw : draws)
        for (const ForestCall& call : forestCalls)
            if (call.firstIndex == draw.firstIndex && call.indexCount != draw.indexCount)
                forestFarTrees += draw.instances;
    sunInWorld(c.sourceRect);
    forestWind(c);
    const float night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    c.sourceClamp[2] = mapOutdoors ? 1.0f - 0.85f * night : 0.35f;
    list->SetGraphicsRootSignature(root);
    list->SetPipelineState(pso);
    list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const D3D12_VERTEX_BUFFER_VIEW views[2] = {
        {forestVb->GetGPUVirtualAddress(), forestVbBytes, UINT(sizeof(forest::Vertex))},
        {instancesAt, instanceBytes, UINT(sizeof(InstanceData))}};
    list->IASetVertexBuffers(0, 2, views);
    const D3D12_INDEX_BUFFER_VIEW ib{forestIb->GetGPUVirtualAddress(), forestIbBytes, DXGI_FORMAT_R32_UINT};
    list->IASetIndexBuffer(&ib);
    // 2.0.4: each model's calls with its leaves' own picture (the game's, its AI picture when shown) cut into the
    // cards - footprint 1, the picture's average colour in stage0.x, stage0.y, stage1.x (asfloat) - so the crowns
    // are the game's leaves in the trees' own shapes; a model whose picture is not known yet keeps the drawn leaves.
    uint32_t bound = ~0u;
    for (const ForestDraw& call : draws) {
        const uint32_t model = call.model;
        const uint32_t id = model < forestTexture.size() ? forestTexture[model] : 0;
        const auto tex = id ? textures.find(id) : textures.end();
        const bool textured = tex != textures.end() && tex->second.resource && tex->second.averaged;
        if ((textured ? id : 0u) != bound) {
            bound = textured ? id : 0u;
            Constants k = c;
            if (textured) {
                Texture& t = tex->second;
                const bool aiShown = ai && ai->shown && t.ai && t.aiWorld;
                if (aiShown)
                    transition(t.ai, t.aiState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                else
                    transition(t.resource, t.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                const UINT srv = aiShown ? t.aiSrv : t.srv;
                list->SetGraphicsRootDescriptorTable(kRootTextures, textureTable(srv, srv));
                list->SetGraphicsRootDescriptorTable(kRootSamplers, samplerTable(leafSampler, leafSampler, 16));
                k.footprint = 1.0f;
                std::memcpy(&k.stage0[0], &t.average[0], 4);
                std::memcpy(&k.stage0[1], &t.average[1], 4);
                std::memcpy(&k.stage1[0], &t.average[2], 4);
            }
            list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &k, 0);
        }
        list->DrawIndexedInstanced(call.indexCount, call.instances, call.firstIndex, INT(call.baseVertex),
                                   call.firstInstance);
    }
    boundPipeline = nullptr;  // the game's next draw binds its own
    boundTextures = boundSamplers = ~0ull;
    if (!forestLogged)
        LOG_INFO("new trees: %s's forest drawn - %zu of its %zu trees in view (%zu of them far), %zu of the game's "
                 "tree draws left out", mapName.c_str(), forestDrawnTrees, forestPlaced.size(), forestFarTrees,
                 forestHidden);
    forestLogged = true;
    return !lost;
}

}  // namespace uagfx::gpu
