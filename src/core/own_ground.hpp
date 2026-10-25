#pragma once
#include <cstdint>
#include <vector>

#include "core/game_camera.hpp"
#include "core/gnd.hpp"
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
// 3.0.4-beta.14: its one-texture vertex (FVF_TL1) - the models' draws
struct Tl1 {
    float x, y, z, rhw;
    uint32_t diffuse, specular;
    float u, v;
};
static_assert(sizeof(Tl1) == 32, "the game's FVF_TL1 vertex");

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
// 3.0.4-beta.22 (R-ZZN7GY «всеравно осталась черная полоса на воде»): the view depth the own far models and water are
// drawn from - where the squeeze begins: what the game draws past it (its water, its models up to the far plane) lies
// behind the own ground's squeezed depth there, though nearer, and is hidden by it (the game's water between 1490 and
// 1500 behind the own bed: a black strip, the far water beginning at 1500)
inline float ownFrom(const camera::Projection& p) { return p.farZ - kSqueeze; }
// 3.0.4-beta.32 (R-Q3W727: a minute still in prontera, 3571 of 3600 pictures made anew for the models' colours): the
// colours learnt from the game's models (own_models::Learning::changes, made then and now) reach the far models at
// most every kModelsColoursEvery pictures - the game lights its swaying cherry trees, turning signs and clock hands
// anew every picture, and the far models stand still
constexpr int kModelsColoursEvery = 60;
inline bool modelsColoursDue(size_t made, size_t now, int picturesSince) {
    return made != now && picturesSince >= kModelsColoursEvery;
}

// A vertex of the own ground at its place in the map's file (the axes make it the world's) as the game would put it on
// the screen - its colours and texture coordinates left; false behind the near plane
bool place(const View& v, const camera::Projection& p, const map::Axes& axes, const float file[3], Tl2& out);
// 3.0.4-beta.14: the same for a point already in the view
bool placeView(const camera::Projection& p, const float view[3], Tl2& out);

// 3.0.4-beta.14 (the own models): the part of a triangle past the view depth `from` (its corners' depths zd) - what the
// game cuts off at its far plane - as up to four corners, each the shares of the triangle's three it is made of (in
// order round it); 0: none of it
int pastDepth(const float zd[3], float from, float out[4][3]);
// 3.0.4-beta.23: the part this side of `to` alike - the own ground's triangles cut where its depth is squeezed
// (ownFrom), each piece one depth (the GPU carries a depth straight across a triangle, and the game's depth at one
// corner with the squeezed one at another made it nearer than either: walls in front of roofs, R-W7A5XC)
int nearDepth(const float zd[3], float to, float out[4][3]);

// Whether a box of the world (lo, hi) may show: not when all of it lies behind the near plane, past `most` or off one
// side of the screen (width x height game pixels); nearest: its corners' least view depth (0 round the eye);
// farthest (3.0.4-beta.14), when given: their greatest
bool boxInView(const View& v, const camera::Projection& p, int width, int height, const float lo[3], const float hi[3],
               float most, float& nearest, float* farthest = nullptr);

// 3.0.4-beta.26 (the user chose «Свет своего мира»): the smooth hills' light (shaders.cpp hillLight) worked out by the
// own ground itself, all over it - the game's hills grid ends with the game's cells. Each corner of the cells its
// normal (up along -y, one long), the cells' slopes round it averaged ((width + 1) x (height + 1), 3 floats each); a
// place between them (file places); the light of a slope against a flat ground's, as the shader works it out: the
// way to the sun, the light all round (ambient), the strength (0 none, 1 the whole) - 0.6 to 1.4 before the strength
std::vector<float> cornerNormals(const gnd::Ground& g);
void normalAt(const std::vector<float>& corners, int width, int height, float zoom, float x, float z, float out[3]);
float reliefLight(const float n[3], const float toSun[3], float ambient, float strength);
// 3.0.4-beta.30: that light a corner (made once a light: a map's corners), a place's between its cell's four - the
// normal and the light at each point cost 73 ns, the light between the corners a few; no corners' lights: 1
std::vector<float> cornerLights(const std::vector<float>& normals, const float toSun[3], float ambient, float strength);
float lightAt(const std::vector<float>& lights, int width, int height, float zoom, float x, float z);

// 3.0.4-beta.20: what of a map's own world the game's memory has room for - uaRO.exe is 32-bit and not large address
// aware: 2 GB for the game, the driver and the card's driver together (lighthalzen's own world takes 214 MB as it is
// read, ecl_fild01's far models 115 MB). free: what was left with all of it made; models: the far models' bytes; fine:
// the smooth ground's (the plain one, n = 1, stays). At least kKeepFree left: the models let go first, then the smooth
// ground, then all of it (the game's own ground)
enum class Room : uint8_t { Whole, NoModels, Coarse, None };
constexpr uint64_t kKeepFree = uint64_t(384) << 20;
Room roomFor(uint64_t free, uint64_t models, uint64_t fine);

// The game's own colours of its ground, learnt from its draws: each cell corner's vertex colour (the colour the game
// lights its ground with) and the fog of this frame (the specular's alpha by the view depth)
class Shading {
public:
    void reset(int width, int height, float zoom);  // a map's cells, its corners all unseen
    // a vertex of the game's ground at (x, z) in the file's places: its corner's colour. 3.0.4-beta.15: `own` - the
    // cell's first corner (its least x and z): its colour the corner's; any other only fills a corner not seen yet
    // (the game colours a corner in each cell round it, and taken in turn it changed four times a picture)
    void note(float x, float z, uint32_t diffuse, bool own);
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
