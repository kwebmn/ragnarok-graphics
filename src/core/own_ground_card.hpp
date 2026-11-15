#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "core/game_camera.hpp"
#include "core/map_axes.hpp"
#include "core/own_ground.hpp"
#include "core/terrain.hpp"

namespace uagfx::own_ground::card {

// 3.0.20-beta.23 (the owner's «берем», item 1 - «Своя земля» on the card: in the 3D view some 100000 of its vertices
// placed, coloured, eased and cut on the processor a picture, 2-3 ms): a map's own ground kept on the card once - the
// smooth mesh's vertices, then the plain one's; each vertex's colour (made again as before, sent as it is made) and the
// strips' blends. A picture sends only the numbers of its chunks' vertices, a layer's texture's on each side of the
// squeeze (own_ground::ownFrom): ground_vs puts, eases, lifts and fogs each vertex as ownChunk does on the processor,
// the card cutting the triangles at the near plane and at the squeeze itself

// The card's buffer in uint4: two a vertex (x y z and its plain height; its texture's u v, the lightmap's u v) - the
// smooth mesh's first -, then the colours four a uint4, then the blends four a uint4
struct Layout {
    uint32_t fine = 0;  // the smooth mesh's vertices
    uint32_t all = 0;   // with the plain one's
    uint32_t coloursAt() const { return all * 2; }
    uint32_t blendsAt() const { return coloursAt() + (all + 3) / 4; }
    uint32_t size() const { return blendsAt() + (all + 3) / 4; }
};
Layout layoutOf(const terrain::Mesh& fine, const terrain::Mesh& plain);
constexpr uint32_t kVertexWords = 8;  // uint32 a vertex
// the vertices [from, from + count) of the two meshes (the smooth one's first) as the card keeps them
void vertexWords(const terrain::Mesh& fine, const terrain::Mesh& plain, uint32_t from, uint32_t count, uint32_t* out);
// the blends (terrain::Vertex::blend) of the vertices [from, from + count), a uint32 each
void blendWords(const terrain::Mesh& fine, const terrain::Mesh& plain, uint32_t from, uint32_t count, uint32_t* out);

// Whether a chunk in view has parts nearer than the squeeze (`from`) - its least view depth below it - and as far or
// farther (its greatest), by a margin: the smooth ground eased onto the plain one may leave the chunk's box a little
constexpr float kSidesMargin = 8.0f;
inline bool hasNear(float nearest, float from) { return nearest < from + kSidesMargin; }
inline bool hasPast(float farthest, float from) { return farthest > from - kSidesMargin; }

// A picture's way of putting the own ground's vertices (ownChunk's): the camera, the projection, the map's axes, the
// smooth ground eased onto the plain one from morphFrom to fineDepth, the fog's line (Shading: fogA + fogB * the view
// depth, its colour)
struct Putting {
    View view{};
    camera::Projection p;
    map::Axes axes{1, 1, 1};
    float morphFrom = 0.0f, fineDepth = 0.0f;
    float fogA = 255.0f, fogB = 0.0f;
    uint32_t fogRgb = 0;
};
// the strips a little towards the camera (by layer: the ground, its own band, a neighbour's strip) - on the ground's
// own points, the bands near them, as the seams'
constexpr float kLift[3] = {0.0f, 0.12f, 0.05f};
// The processor's way (ownChunk, 3.0.4-beta.10 on): where the game would put it on the screen, the smooth ground
// (3.0.4-beta.29) eased onto the plain one as it nears the change, a strip's depth lifted, the fog's alpha by the line
// (a strip's specular the blend's colour), both texture coordinates - its diffuse left; false behind the near plane
bool putVertex(const Putting& pu, const terrain::Vertex& v, bool smooth, uint8_t layer, Tl2& out);
// The card's (ground_vs, the same sums): the game's screen x and y and its depth, each times w, w the view's depth -
// the card divides; the depth the near side's (the game's) or the past one's (squeezed), so that a triangle across
// the squeeze cut by the card at cut[0] has one depth on each side; cut[1] the near plane's; the specular
struct Clip {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
    float cut[2] = {0.0f, 0.0f};  // kept where >= 0
    uint32_t specular = 0;
};
Clip clipOf(const Putting& pu, const terrain::Vertex& v, bool smooth, uint8_t layer, bool past);

// A picture's draw: a layer's texture on one side of the squeeze
struct Draw {
    uint8_t layer = 0;
    uint32_t texture = 0;  // the .gnd's; their count: none (drawn white)
    bool past = false;     // past the squeeze
    uint32_t count = 0;    // indices
};
using ParallelFor = std::function<void(size_t count, const std::function<void(size_t)>& body)>;

class Lists {
public:
    // a picture's: the .gnd's textures; blends - the strips too
    void begin(size_t textures, bool blends);
    // a chunk in view: of the plain mesh or the smooth one, the sides of the squeeze it has parts on
    void add(const terrain::Mesh& mesh, bool plain, size_t chunk, bool nearSide, bool pastSide);
    // the draws by layer, texture, the near side before the past one; a draw's batches in the chunks' order
    const std::vector<Draw>& plan();
    // each draw's indices - the vertices' numbers on the card (the plain mesh's after the `fine` smooth ones) - into
    // where(draw), room for its count; a chunk a job of the parallel for (where called from its threads)
    void fill(uint32_t fine, const std::function<uint32_t*(size_t draw)>& where, const ParallelFor& run) const;
    size_t chunks() const { return added_.size(); }

private:
    struct Added {
        const terrain::Mesh* mesh = nullptr;
        bool plain = false;
        size_t chunk = 0;
        bool nearSide = false, pastSide = false;
    };
    struct Part {  // a batch's indices on one side: its draw, its place in the draw
        uint32_t batch = 0, draw = 0, at = 0;
    };
    size_t textures_ = 0;
    bool blends_ = false;
    std::vector<Added> added_;
    std::vector<Draw> draws_;
    std::vector<uint32_t> counts_, drawOf_;  // by group (layer, texture, side)
    std::vector<Part> parts_;
    std::vector<uint32_t> partsFrom_;  // a chunk added's first part (one more at the end)
};

}  // namespace uagfx::own_ground::card
