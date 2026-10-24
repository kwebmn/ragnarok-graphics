#pragma once
#include <cstdint>
#include <vector>

#include "core/game_camera.hpp"
#include "core/map_axes.hpp"

namespace uagfx::own_ground {

// 3.0.4-beta.10 (the user: «сделай 1», «продолжай делать прототип»): the own ground (core/terrain.hpp) drawn by the
// driver in place of the game's ground draws - where the game would put its vertices, in the game's own colours, and
// on past its far plane, where the game cuts its ground off.

// The game's two-texture vertex (FVF_TL2): its screen place, colours and both texture coordinates.
struct Tl2 {
    float x, y, z, rhw;
    uint32_t diffuse, specular;
    float u0, v0, u1, v1;
};
static_assert(sizeof(Tl2) == 40, "the game's FVF_TL2 vertex");

// The camera, world to view: rows into the view's x, y, z (as ground.cpp's seams take it) and its place
struct View {
    float r[9];
    float eye[3];
};
// From the renderer's view -> world (the view's axes in the world, then its place); false: no inverse
bool viewOf(const float viewToWorld[12], View& out);
inline void toView(const View& v, const float w[3], float out[3]) {
    const float x = w[0] - v.eye[0], y = w[1] - v.eye[1], z = w[2] - v.eye[2];
    out[0] = x * v.r[0] + y * v.r[3] + z * v.r[6];
    out[1] = x * v.r[1] + y * v.r[4] + z * v.r[7];
    out[2] = x * v.r[2] + y * v.r[5] + z * v.r[8];
}

// The depth the game gives a view depth zd (ProjectVertex 0x412cb0), and past its far plane, where it has none, the
// room left below 1: the game's up to kSqueeze before the far plane, the rest squeezed in after it - the far ground
// hides its own hills in order and nothing of the game's lies there
constexpr float kSqueeze = 10.0f;
float depthOf(const camera::Projection& p, float zd);

// A vertex of the own ground at its place in the map's file (the axes make it the world's) as the game would put it on
// the screen - its colours and texture coordinates left; false behind the near plane
bool place(const View& v, const camera::Projection& p, const map::Axes& axes, const float file[3], Tl2& out);

// Whether a box of the world (lo, hi) may show: not when all of it lies behind the near plane, past `most` or off one
// side of the screen (width x height game pixels); nearest: its corners' least view depth (0 round the eye)
bool boxInView(const View& v, const camera::Projection& p, int width, int height, const float lo[3], const float hi[3],
               float most, float& nearest);

// The game's own colours of its ground, learnt from its draws: each cell corner's vertex colour (the colour the game
// lights its ground with) and the fog of this frame (the specular's alpha by the view depth)
class Shading {
public:
    void reset(int width, int height, float zoom);  // a map's cells, its corners all unseen
    // a vertex of the game's ground at (x, z) in the file's places: its corner's colour
    void note(float x, float z, uint32_t diffuse);
    // between the four corners round (x, z); one not seen yet takes the mean of those seen (none: white)
    uint32_t diffuseAt(float x, float z) const;
    void beginFrame();
    void fog(float zd, uint32_t specular);  // a vertex of the game's ground this frame: its depth and its specular
    void fit();                             // the frame's fog line, once its vertices are in
    uint32_t specularAt(float zd) const;    // alpha by the line (none seen: 255), the game's colour
    bool fogged() const { return fogged_; }
    // 3.0.4-beta.12: the corners given another colour since the last call (index j * (width + 1) + i, in the order
    // seen) - the renderer makes the colours round them again; mean: the mean the unseen ones take moved by more than
    // a little (all to make again)
    void takeChanged(std::vector<uint32_t>& corners, bool& mean);

private:
    uint32_t cornerOr(int i, int j, uint32_t none) const;
    uint32_t mean() const;
    std::vector<uint32_t> changed_;
    bool tooMany_ = false;
    uint32_t toldMean_ = 0xFFFFFFFFu;
    int width_ = 0, height_ = 0;
    float zoom_ = 1.0f;
    std::vector<uint32_t> corners_;  // (width + 1) x (height + 1), ARGB; 0 not seen
    uint64_t sum_[4] = {0, 0, 0, 0};  // of the corners seen, by channel
    uint32_t seen_ = 0;
    double n_ = 0, sz_ = 0, sa_ = 0, szz_ = 0, sza_ = 0;  // the frame's fog: least squares of alpha on depth
    bool fogged_ = false;
    float fogA_ = 255.0f, fogB_ = 0.0f;
    uint32_t specularRgb_ = 0;
};

}  // namespace uagfx::own_ground
