#pragma once
#include <cstdint>
#include <unordered_map>

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
};
constexpr float kBodyUnits = 45.0f;  // a character sprite's height in the world's units, about
constexpr float kNearer = 10.0f;     // a draw this much nearer than the feet covers the body (not the ground at them)
// view: the game's world -> view (c_j = sum_i w_i * view[3i+j] + view[9+j]); at: the camera's target in the world
Focus focusOf(const camera::Projection& p, const float view[12], const float at[3]);

// The draw's triangles (a list or a strip; indices optional) over one of the body's points, nearer than the feet
bool covers(const uint8_t* vertices, uint32_t stride, uint32_t vertexCount, const uint16_t* indices,
            uint32_t indexCount, bool strip, const Focus& f);

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

private:
    struct Entry {
        float alpha = 1.0f;
        uint32_t frame = 0;
    };
    std::unordered_map<uint64_t, Entry> alpha_;
};

}  // namespace uagfx::modelfade
