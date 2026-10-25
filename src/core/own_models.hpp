#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/rsm.hpp"
#include "core/rsw.hpp"
#include "core/terrain.hpp"

namespace uagfx::own_models {

// 3.0.4-beta.14 (the user chose «Дома и деревья вдали»): the map's models - its .rsw's instances of its .rsm files -
// placed as the game places them (rsw::placeInMap) for the renderer to draw past the game's far plane, where the game
// cuts them off. In the file's places, as the own ground (terrain.hpp): chunks by the ground's squares of kChunk cells
// (an instance's place), each a batch a texture; a corner the faces share one vertex (each projected once a picture),
// its normal their mean, its colour white (the renderer's).
// files[i]: instance i's model, null where none was read; textures: the names the batches index, filled in.
terrain::Mesh place(const std::vector<rsw::Model>& instances, const std::vector<const rsm::Model*>& files, int width,
                    int height, float zoom, std::vector<std::string>& textures);

// 3.0.4-beta.33 (R-5ZC9JX, a minute still in prontera: «noted 360600, 0 (0 %) where ours lie»): the game's model
// triangles noted a picture - at most kMostNoted, in turn: the next picture's from where this one's stopped, from the
// first again once this one reached the game's last (noted fewer). The same first ones every picture had the learning
// learn nothing while the camera stood, none of them ours
constexpr uint32_t kMostNoted = 600;
inline uint32_t nextNotedFrom(uint32_t from, uint32_t noted) { return noted < kMostNoted ? 0u : from + noted; }
// 3.0.4-beta.34 (R-37HNEB: beta.33's turn never moved): the turn over the pictures - the renderer starts a picture at
// the present and again at the game's clear right after it; one that saw none of the game's model triangles leaves
// the turn as it was. rounds and most for the log: how often all were noted, the most a picture held
struct NotingTurn {
    uint32_t from = 0, noted = 0, passed = 0;  // this picture's first to note; noted; gone by, noted or not
    uint32_t rounds = 0, most = 0;
    void next() {
        if (!passed) return;
        most = passed > most ? passed : most;
        from = nextNotedFrom(from, noted);
        rounds += from ? 0u : 1u;
        noted = passed = 0;
    }
};

// The game's colours of its models, learnt from its own draws of them near (the own ground's way, own_ground::Shading):
// a triangle of the game's where one of ours lies (its middle, then each corner within 0.3) gives our corners its
// colours - and tells that ours stand where the game's do; the rest lit as the game lights those it drew, a line
// through their colours by how much they face the map's light (each channel a + b max(0, n.L), the faces' side by
// the better fit).
class Learning {
public:
    // a map's own models (place()) and the way to its light (file places, up along -y); kept: the mesh outlives this
    void reset(const terrain::Mesh& models, const float toLight[3]);
    // a triangle of the game's models: its corners in the file's places and their colours; true: one of ours
    bool note(const float corners[3][3], const uint32_t colours[3]);
    void fit();  // the line through the colours noted so far
    uint32_t colourOf(uint32_t vertex) const;  // a vertex of the models: learnt, else lit by the line
    size_t seen() const { return seen_; }       // the game's triangles noted
    size_t matched() const { return matched_; }  // those that are ours
    size_t changes() const { return changes_; }  // 3.0.4-beta.20: the vertices' colours learnt anew so far

private:
    static uint64_t keyOf(int x, int y, int z);
    const terrain::Mesh* mesh_ = nullptr;
    float toLight_[3] = {0, -1, 0};
    struct Triangle {
        uint32_t v[3];
    };
    std::vector<Triangle> triangles_;
    std::unordered_map<uint64_t, uint32_t> middles_;  // a triangle's middle (quarter units) -> the triangle
    std::vector<uint32_t> learnt_;                    // each vertex's colour from the game, 0 none
    double sums_[2][4][5] = {};                       // by side, channel: n, sx, sxx, sc, sxc
    float a_[4] = {255, 255, 255, 255}, b_[4] = {0, 0, 0, 0};
    float side_ = 1.0f;
    size_t seen_ = 0, matched_ = 0, changes_ = 0;
};

}  // namespace uagfx::own_models
