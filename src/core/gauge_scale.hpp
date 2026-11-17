#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// 3.0.20-beta.84 (R-7WTVBF «когда отдаляю, то полоска хп выглядит огромной. чтото можно придумать чтобы смотрелось
// хорошо»): the bars under the characters - uaRO.exe's gauges (core/world_windows: 60 x 9, 60 x 5, 60 x 6 pixels
// whatever the camera) - smaller once the camera is further out than the game's own views (the driver lets it go to
// 1400, the game's classic ones end near 500): their faces moved toward each gauge's top middle as they are drawn
namespace uagfx::gaugescale {

constexpr float kFullAt = 500.0f;  // the camera's distance up to which the bars keep the game's size
constexpr float kLeast = 0.6f;     // the smallest they get (the camera from about 833 out)
// 1 up to kFullAt, kFullAt / distance further out, kLeast at least; 1 for a distance that makes no sense
float scaleAt(float distance);
// `count` transformed vertices (x, y the first two floats, `stride` bytes apart) moved toward (ax, ay) by `scale`
void scaleVertices(uint8_t* vertices, uint32_t count, size_t stride, float ax, float ay, float scale);
// the gauge (boxes: x, y, width, height in the game's pixels) whose box, a pixel round it, holds the vertices' middle;
// -1 none
int boxOf(const uint8_t* vertices, uint32_t count, size_t stride, const std::vector<std::array<int, 4>>& boxes);
// each gauge's anchor (x, y): the top middle of its stack - the gauges one under another (the same middle, each
// within 3 pixels under the one over it), as an actor's HP/SP one and its cast's are
std::vector<std::array<float, 2>> anchors(const std::vector<std::array<int, 4>>& boxes);
// the gauges' classes among the world's windows
bool gaugeClass(uint32_t vtable);

}  // namespace uagfx::gaugescale
