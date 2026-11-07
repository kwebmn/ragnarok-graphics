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

// 3.0.13-beta.19 (R-KVR7K3: a house half see-through - the parts drawn over the body alone, its front and roof
// whole): a draw's screen box and its nearest depth; one over the box of a draw that covered the body the frame before,
// itself before the character, goes with it - the whole house
struct Box {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0, nearest = 0;
};
bool boxOf(const uint8_t* vertices, uint32_t stride, uint32_t vertexCount, Box& out);

// A draw's place in the world (its first vertex back through the camera), rounded - with its texture and size the key
// a model keeps over the frames
uint64_t keyOf(const camera::Projection& p, const float view[12], const uint8_t* vertices, uint32_t vertexCount,
               uintptr_t texture);

// Each model's see-through, gliding kStep a frame toward kSeen while it covers, back to 1 after
constexpr float kSeen = 0.35f, kStep = 0.08f;
class Fader {
public:
    float step(uint64_t key, bool covering, uint32_t frame);  // the draw's alpha now (1: as the game draws it)
    void frameDone(uint32_t frame);                          // models not drawn for a while forgotten
    size_t fading() const { return alpha_.size(); }
    void covered(const Box& b) { if (now_.size() < 64) now_.push_back(b); }  // a draw over the body this frame
    // 3.0.13-beta.19: a draw before the character (`z` the character's) over one that covered it the frame before
    bool alongside(const Box& b, float z) const;

private:
    std::vector<Box> now_, last_;
    struct Entry {
        float alpha = 1.0f;
        uint32_t frame = 0;
    };
    std::unordered_map<uint64_t, Entry> alpha_;
};

}  // namespace uagfx::modelfade
