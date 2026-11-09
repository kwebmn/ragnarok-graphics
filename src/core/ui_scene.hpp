#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/glint.hpp"

namespace uagfx::panel {
struct Picture;
}

namespace uagfx::uiscene {

// 3.0.18-beta.12 (docs/superpowers/plans/2026-11-09-uaui-migration.md, the ruling): the new interface's painters
// recorded - a picture with a scene (panel::Picture::scene) takes their shapes as these primitives instead of painting
// them; uaUI draws them on the card (win/uaui's <ua-scene>). Places in the picture's pixels; each with the picture's
// clip at the time.
enum class Kind : uint8_t {
    Rect,   // a rounded box filled, a ring `edgeW` wide inside its border (a circle: r half its side)
    Icon,   // uiicons' drawing `icon` w wide at (x, y), its lines `stroke`, `fill` at `opacity`
    Image,  // a picture of the driver's (`image`, kept by its maker) at its own size w x h at (x, y); `r` > 0: within
            // the circle of that radius round its box's middle
    Words,  // a word drawn by GDI (win/picture_draw): its coverage `key`, w x h at (x, y), `fill` its colour
};

struct Prim {
    Kind kind = Kind::Rect;
    float x = 0, y = 0, w = 0, h = 0, r = 0;
    uint32_t fill = 0, edge = 0;  // 0xRRGGBB
    float fillA = 0, edgeA = 0, edgeW = 0;
    float opacity = 1;
    int icon = 0;
    float stroke = 2;
    const panel::Picture* image = nullptr;
    // an Image's pixels as they were when recorded (premultiplied ARGB, iw x ih) and their sum (its texture's key)
    std::shared_ptr<const std::vector<uint32_t>> pixels;
    int iw = 0, ih = 0;
    uint32_t sum = 0;
    std::string key;     // a word's coverage (win/picture_draw's)
    std::vector<uint8_t> cover;  // its pixels w x h (each its coverage 0-255), copied when recorded
    bool halo = false;   // a word's dark outline
    bool clipped = false;
    int cx0 = 0, cy0 = 0, cx1 = 0, cy1 = 0;  // the clip when clipped
};

struct Scene {
    std::vector<Prim> prims;
    int generation = 0;  // the maker's: another one, another scene (its meshes made again)
    float opacity = 1;   // panel::fade's on the whole
    void clear() { prims.clear(), opacity = 1; }
};

// A picture's own scene, copied whole with it (a part's view before keeps its scene as its picture)
struct SceneBox {
    std::unique_ptr<Scene> s;
    SceneBox() = default;
    SceneBox(const SceneBox& o) : s(o.s ? std::make_unique<Scene>(*o.s) : nullptr) {}
    SceneBox& operator=(const SceneBox& o) {
        if (this != &o) s = o.s ? std::make_unique<Scene>(*o.s) : nullptr;
        return *this;
    }
    SceneBox(SceneBox&&) noexcept = default;
    SceneBox& operator=(SceneBox&&) noexcept = default;
};

// A Rect primitive's triangles (its fill and its edge, each with a pixel's smooth fringe) moved by (dx, dy), in the
// game's pixels ((x - half) / k, as the driver's pictures), its colours premultiplied at `opacity`
void mesh(const Prim& p, float dx, float dy, float k, float half, float opacity, std::vector<glint::Vertex>& out);

}  // namespace uagfx::uiscene
