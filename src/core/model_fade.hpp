#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "core/game_camera.hpp"

namespace uagfx::modelfade {

// 3.0.13-beta.17 (the owner's pick 1 of «50 идей»: «Здания и деревья перед персонажем становятся прозрачными ...
// плавное затухание»): [effects] fade_models - a draw of the game's models (opaque, writing depth: TL1 x, y, z, rhw,
// colour at 16) that covers the character's body nearer than it goes see-through, back again when it no longer does

// The character on the screen: its feet (the camera's target, projected), its depth (view z) and its body's height in
// pixels (kBodyUnits tall at that depth)
struct Focus {
    bool known = false;
    float x = 0, y = 0, z = 0, h = 0;
    // 3.0.13-beta.20 (R-UQ7AGY: Prontera's fountain behind the character went see-through - the camera looks down, a
    // tall thing behind is nearer along its view): the projection, the camera, the camera's place and how far the
    // character is from it along the ground (the world's x, z; y is up) - a model covers only nearer along the ground
    camera::Projection p;
    float view[12] = {};
    float camX = 0, camZ = 0, ground = 0;
};
constexpr float kBodyUnits = 45.0f;  // a character sprite's height in the world's units, about
// a draw this much nearer than the feet covers the body; 3.0.13-beta.18 (R-TAA3SB: a wall right before the character
// and a roof over its feet stayed whole): 3, not 10 - the ground over the feet lies behind them anyway
constexpr float kNearer = 3.0f;
constexpr int kPoints = 4;
constexpr float kBodyAt[kPoints] = {0.1f, 0.35f, 0.6f, 0.9f};  // .18: 0.1 - the feet's own, a roof over them
// view: the game's world -> view (c_j = sum_i w_i * view[3i+j] + view[9+j]); at: the camera's target in the world
Focus focusOf(const camera::Projection& p, const float view[12], const float at[3]);

// The draw's triangles (a list or a strip; indices optional) over one of the body's points, nearer than the feet
bool covers(const uint8_t* vertices, uint32_t stride, uint32_t vertexCount, const uint16_t* indices,
            uint32_t indexCount, bool strip, const Focus& f);

// A model's place in the world (the middle of its piece's vertices back through the camera) and its texture - the
// model it is over the frames; 3.0.13-beta.21 (the owner: «когда иду за фонтаном прозрачность "двигается",
// мигает»): matched by the nearest place within kSameModel, not a rounded key - a moving model (the fountain's
// water) or a place on the rounding's edge made a new model each frame, starting from whole again
struct Model {
    uint64_t id = 0;
    float w[3] = {0, 0, 0};
};
// .22 (the owner: «за статуей с рукой хожу, она мигает всеравно»): the texture alone and the middle of all its vertices
// - a draw's first vertex and its count change as the game leaves out faces turned away while one walks
constexpr float kSameModel = 40.0f;
constexpr float kHouse = 40.0f;  // .23: a house's parts' middles, about; .25: 40, not 60

// Each model's see-through, gliding toward kSeen while it covers, back to 1 after; .21: it keeps covering a while
// after it last did - the body's points slipping off the model's edges as one walks made it pulse. 3.0.13-beta.25
// (R-8KN4Z3 at 118 frames a second: a glide of 8 frames was 70 ms, a blink): by the clock - kDownMs to fade, kUpMs
// back, held kHoldMs
constexpr float kSeen = 0.35f;
constexpr float kDownMs = 250.0f, kUpMs = 400.0f;
constexpr uint64_t kHoldMs = 600;
// .25: a piece beside one that covered goes with it only near the body on the screen (kNearBody body heights to the
// sides and above) - lamp posts and benches round the character faded with a tree over it
constexpr float kNearBody = 1.5f;
class Fader {
public:
    float step(const Model& m, bool covering, uint32_t frame);  // the draw's alpha now (1: as the game draws it)
    void frameDone(uint32_t frame);                          // models not drawn for a while forgotten
    void clock(uint64_t ms);  // .25: the time of the frame about to be drawn
    size_t fading() const { return alpha_.size(); }
    uint32_t madeAnew() const { return made_; }  // .22: models taken up anew so far, for the log
    // a draw over the body this frame
    void covered(const Model& m) { if (now_.size() < 64) now_.push_back(m); }
    // 3.0.13-beta.19: the whole house - a model beside one that covered the body the frame before; .23 (R-QH3H48
    // «когда хожу, мигает заборы вокруг чара»): beside in the world, kHouse along the ground from its middle - not
    // over its box on the screen, which took the long fences round the square in and out at each step
    bool alongside(const Model& m) const;

    // 3.0.13-beta.24 (R-2KHTSU: «вдали и вблизи заборы и все остальное мигает» - trees and benches far off faded with
    // the one over the character): a draw (TL1, 32 bytes a vertex) by its pieces - triangles joined by a corner or a
    // corner's place, each its own model, as one draw of the game may hold many. true: some piece see-through - its
    // triangles in `faded` (a list), the rest in `solid`, each vertex's alpha; false: the draw as the game made it
    struct Split {
        std::vector<uint16_t> solid, faded;
        std::vector<float> alpha;
    };
    bool fade(const uint8_t* vertices, uint32_t vertexCount, const uint16_t* indices, uint32_t indexCount, bool strip,
              uintptr_t texture, const Focus& f, uint32_t frame, Split& out);

private:
    uint32_t root(uint32_t i);
    struct Tri {
        uint16_t i[3];
        bool covers;
    };
    struct Piece {
        double sum[3] = {0, 0, 0};
        uint32_t n = 0;
        float nearest = 1e9f, alpha = 1.0f;
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;  // .25: its box on the screen
        bool covers = false;
    };
    std::vector<Tri> tris_;  // .24: one draw's, kept between draws to spare the allocations
    std::vector<uint32_t> parent_;
    std::vector<int> pieceOf_;
    std::vector<Piece> pieces_;
    std::unordered_map<uint64_t, uint32_t> twins_;
    std::vector<Model> now_, last_;
    uint32_t made_ = 0;
    uint64_t nowMs_ = 0, lastMs_ = 0;
    float dtMs_ = 16.7f;
    struct Entry {
        Model m;
        float alpha = 1.0f;
        uint32_t frame = 0;
        uint64_t coveredMs = 0;
    };
    std::vector<Entry> alpha_;
};

}  // namespace uagfx::modelfade
