#include <cmath>
#include <vector>

#include "core/ui_icons.hpp"
#include "core/ui_scene.hpp"
#include "core/window_panel.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// The triangles' covered area, each weighted by its corners' mean alpha (the feather counts half)
float coveredArea(const std::vector<glint::Vertex>& v) {
    float area = 0;
    for (size_t i = 0; i + 2 < v.size(); i += 3) {
        const float a = std::fabs((v[i + 1].x - v[i].x) * (v[i + 2].y - v[i].y) -
                                  (v[i + 2].x - v[i].x) * (v[i + 1].y - v[i].y)) / 2;
        const float alpha = (float(v[i].diffuse >> 24) + float(v[i + 1].diffuse >> 24) + float(v[i + 2].diffuse >> 24)) /
                            (3 * 255.0f);
        area += a * alpha;
    }
    return area;
}

}  // namespace

// 3.0.18-beta.12 (docs/superpowers/plans/2026-11-09-uaui-migration.md, the ruling): a picture with a scene records
// its shapes and leaves its pixels as they are
TEST(ui_scene_records_the_panels_shapes_instead_of_painting) {
    panel::Picture p;
    panel::blank(p, 100, 60);
    uiscene::Scene scene;
    p.scene = &scene;
    panel::roundRect(p, panel::Box{10, 10, 50, 30}, 8, 0x10141C, 0.9f, 0xC9A55B, 1.0f, 1.5f);
    panel::circle(p, 80, 30, 6, 0x5BD36C, 1.0f);
    uiicons::draw(p, uiicons::Icon(0), 4, 4, 16, 0xECEFF5, 0.8f);
    CHECK_EQ(scene.prims.size(), size_t(3));
    const uiscene::Prim& r = scene.prims[0];
    CHECK(r.kind == uiscene::Kind::Rect && r.x == 10 && r.w == 50 && r.r == 8 && r.fill == 0x10141Cu &&
          r.edge == 0xC9A55Bu && r.edgeW == 1.5f);
    CHECK(scene.prims[1].kind == uiscene::Kind::Rect && scene.prims[1].r == 6 && scene.prims[1].w == 12);
    CHECK(scene.prims[2].kind == uiscene::Kind::Icon && scene.prims[2].w == 16 && scene.prims[2].opacity == 0.8f);
    for (uint32_t px : p.px) CHECK(px == 0u);  // nothing painted
    // the picture's clip goes with each shape
    p.clipX0 = 20, p.clipY0 = 0, p.clipX1 = 40, p.clipY1 = 60;
    panel::roundRect(p, panel::Box{0, 0, 100, 60}, 0, 0xFFFFFF, 1.0f);
    CHECK(scene.prims.back().clipped && scene.prims.back().cx0 == 20 && scene.prims.back().cx1 == 40);
}

TEST(ui_scene_meshes_a_rounded_box_as_its_area) {
    uiscene::Prim r;
    r.kind = uiscene::Kind::Rect;
    r.x = 0, r.y = 0, r.w = 40, r.h = 20, r.r = 6;
    r.fill = 0xFFFFFF, r.fillA = 1;
    std::vector<glint::Vertex> out;
    uiscene::mesh(r, 0, 0, 1.0f, 0.0f, 1.0f, out);
    CHECK(!out.empty() && out.size() % 3 == 0);
    const float area = 40 * 20 - (4 - 3.14159265f) * 36;  // the corners' cut
    CHECK(std::fabs(coveredArea(out) - area) < 0.04f * area);
    for (const glint::Vertex& v : out) CHECK(v.x >= -1.01f && v.x <= 41.01f && v.y >= -1.01f && v.y <= 21.01f);
    // an edge alone (no fill): a ring 2 wide inside the border
    uiscene::Prim e = r;
    e.fillA = 0, e.edge = 0xFF0000, e.edgeA = 1, e.edgeW = 2;
    std::vector<glint::Vertex> ring;
    uiscene::mesh(e, 0, 0, 1.0f, 0.0f, 1.0f, ring);
    const float perimeter = 2 * (40 + 20) - 8 * 6 + 2 * 3.14159265f * 6;
    CHECK(std::fabs(coveredArea(ring) - perimeter * 2) < 0.15f * perimeter * 2);
    // premultiplied, at the opacity asked
    std::vector<glint::Vertex> faint;
    uiscene::mesh(r, 0, 0, 1.0f, 0.0f, 0.5f, faint);
    bool half = false;
    for (const glint::Vertex& v : faint) half = half || (v.diffuse >> 24) == 0x80 || (v.diffuse >> 24) == 0x7F;
    CHECK(half);
}

TEST(ui_scene_meshes_a_circle_and_moves_it) {
    uiscene::Prim c;
    c.kind = uiscene::Kind::Rect;
    c.x = 0, c.y = 0, c.w = 20, c.h = 20, c.r = 10;  // a circle: a round box as wide as twice its radius
    c.fill = 0x00FF00, c.fillA = 1;
    std::vector<glint::Vertex> out;
    uiscene::mesh(c, 100, 50, 2.0f, 0.5f, 1.0f, out);  // at (100, 50), k 2: game x = (x + 100 - 0.5) / 2
    float minX = 1e9f;
    for (const glint::Vertex& v : out) minX = std::min(minX, v.x);
    CHECK(std::fabs(minX - (100 - 0.5f - 0.5f) / 2) < 0.05f);  // its fringe half a pixel out
    CHECK(std::fabs(coveredArea(out) * 4 - 3.14159265f * 100) < 0.05f * 3.14159265f * 100);
}
