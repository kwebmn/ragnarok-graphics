#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "core/own_ground.hpp"

namespace uagfx::threatarc {

// 3.0.19-beta.36 (the 3D view's concept https://claude.ai/artifact/RadnVxG35hjE9d8HgdqzAy: «Индикатор угрозы: красная
// дуга у края экрана с той стороны, откуда бьют»; its picture: «Слева красная дуга: Fabre бьёт из-за спины, вне поля
// зрения»): who hit the character lately, and an arc at the screen's edge on the side one outside the view hits from
constexpr uint32_t kHoldMs = 1600;          // shown this long after its last hit, fading
constexpr float kSpan = 0.32f;              // radians each way of its bearing
constexpr float kRadius = 0.46f;            // its outer edge from the middle, of the picture's shorter side
constexpr float kThick = 0.035f;            // and its thickness
constexpr float kAlphaMost = 0.85f;         // at its hit
constexpr uint32_t kColour = 0xFFE0403Au;   // a warm red
constexpr int kSegments = 12;
struct Hits {
    std::unordered_map<uint32_t, uint32_t> last;  // an attacker's last hit at the character (ms)
    void hit(uint32_t id, uint32_t nowMs) { last[id] = nowMs; }
    void gone(uint32_t id) { last.erase(id); }
    // its arc's strength now (0-1): whole at its hit, none kHoldMs after
    float alphaOf(uint32_t id, uint32_t nowMs) const;
    void forget(uint32_t nowMs);  // the ones past kHoldMs
};
// The bearing of (toX, toZ) from (fromX, fromZ) against a view whose forward laid flat is (fx, fz) and its right
// (rx, rz): 0 ahead, pi/2 to the right, -pi/2 to the left, +-pi behind
float bearingOf(float fromX, float fromZ, float toX, float toZ, float fx, float fz, float rx, float rz);
// The arc's triangles (a list) on a picture w x h round its middle at `bearing` (0 the top, clockwise), in its
// colour at `alpha` (0-1) of kAlphaMost, on the screen (z 0, rhw 1)
void arc(float w, float h, float bearing, float alpha, std::vector<own_ground::Tl1>& out);

}  // namespace uagfx::threatarc
