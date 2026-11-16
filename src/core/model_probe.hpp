#pragma once
#include <cstdint>

#include "core/game_camera.hpp"

// 3.0.20-beta.61 (R-HBPQ54 «зникли частини мосту», R-3W4TYM «теж моста немає», the owner's «там где портал, там
// дырка, которая пробивает его насквозь»: faces of models we draw - a bridge's deck over water - not in the picture,
// though the frame lists their nodes and the processor's count says they show): the models' dump looks at the
// picture itself - a face's place on the depth target and its own depth there; the depth read back after our draw
// says whether the face is there, something nearer stands in front of it, or what is behind it shows (the face was
// not drawn). The renderer then draws the missing ones again in other ways and looks again (win/gpu/models_probe.cpp)
namespace uagfx::modelprobe {

constexpr float kMostSpan = 5e-4f;  // a face's depth across one pixel: past it too slanted to judge
constexpr float kSlack = 2e-5f;     // of the depth read, each way

struct Point {
    int x = 0, y = 0;         // the depth target's pixel
    float lo = 0.0f, hi = 0.0f;  // the face's depth over that pixel
    float area = 0.0f;        // the face on the target, pixels (the largest of a texture's faces looked at)
};

// the game's depth of a point `z` deep in the view (its near and far planes)
float depthOf(const camera::Projection& p, float z);
// a face's place to look at - its corners in the view, the target: so many of its pixels a game's one (`scale`), this
// frame's jitter, its size: the pixel under the face's middle and the face's depth across it. False: not to be judged
// (a corner behind the near plane or past where the depth is squeezed, its middle off the target, too slanted)
bool pointOf(const float a[3], const float b[3], const float c[3], const camera::Projection& p, float scale,
             const float jitter[2], int width, int height, Point& out);
enum class Seen : uint8_t { There, Nearer, Farther };
// what the depth read at the point says: the face itself, something in front of it, what lies behind it
Seen judge(const Point& p, float depth);

}  // namespace uagfx::modelprobe
