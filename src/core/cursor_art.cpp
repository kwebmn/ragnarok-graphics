#include "core/cursor_art.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace uagfx::cursorart {
namespace {

struct P {
    float x, y;
};

// A piece of a shape, its signed distance (negative inside) in the box's units
struct Piece {
    enum class Kind : uint8_t { Polygon, Capsule, Box, Circle, Ring, Arc } kind;
    std::vector<P> pts;  // the polygon's; the capsule's two ends; the box's corners; the circle's / ring's middle
    float r = 0;         // the capsule's, the box's corners', the circle's radius; the ring's middle radius
    float w = 0;         // the ring's width
    float a0 = 0, a1 = 0;  // the arc's angles (radians, y down)
};

float segmentDistance(P p, P a, P b) {
    const float dx = b.x - a.x, dy = b.y - a.y, l2 = dx * dx + dy * dy;
    const float t = l2 > 0 ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / l2, 0.0f, 1.0f) : 0.0f;
    return std::hypot(p.x - a.x - t * dx, p.y - a.y - t * dy);
}

float distanceOf(const Piece& c, P p) {
    switch (c.kind) {
        case Piece::Kind::Polygon: {
            float d = 1e9f;
            bool inside = false;
            for (size_t i = 0, j = c.pts.size() - 1; i < c.pts.size(); j = i++) {
                const P a = c.pts[i], b = c.pts[j];
                d = std::min(d, segmentDistance(p, a, b));
                if ((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) inside = !inside;
            }
            return inside ? -d : d;
        }
        case Piece::Kind::Capsule: return segmentDistance(p, c.pts[0], c.pts[1]) - c.r;
        case Piece::Kind::Box: {
            const float cx = (c.pts[0].x + c.pts[1].x) / 2, cy = (c.pts[0].y + c.pts[1].y) / 2;
            const float hx = std::fabs(c.pts[1].x - c.pts[0].x) / 2, hy = std::fabs(c.pts[1].y - c.pts[0].y) / 2;
            const float r = std::min({c.r, hx, hy});
            const float qx = std::fabs(p.x - cx) - (hx - r), qy = std::fabs(p.y - cy) - (hy - r);
            return std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - r;
        }
        case Piece::Kind::Circle: return std::hypot(p.x - c.pts[0].x, p.y - c.pts[0].y) - c.r;
        case Piece::Kind::Ring: return std::fabs(std::hypot(p.x - c.pts[0].x, p.y - c.pts[0].y) - c.r) - c.w / 2;
        case Piece::Kind::Arc: {  // a ring between two angles, its ends round
            const float dx = p.x - c.pts[0].x, dy = p.y - c.pts[0].y;
            float a = std::atan2(dy, dx);
            while (a < c.a0) a += 6.2831853f;
            if (a <= c.a1) return std::fabs(std::hypot(dx, dy) - c.r) - c.w / 2;
            const P e0{c.pts[0].x + c.r * std::cos(c.a0), c.pts[0].y + c.r * std::sin(c.a0)};
            const P e1{c.pts[0].x + c.r * std::cos(c.a1), c.pts[0].y + c.r * std::sin(c.a1)};
            return std::min(std::hypot(p.x - e0.x, p.y - e0.y), std::hypot(p.x - e1.x, p.y - e1.y)) - c.w / 2;
        }
    }
    return 1e9f;
}

Piece polygon(std::vector<P> pts) { return Piece{Piece::Kind::Polygon, std::move(pts)}; }
Piece capsule(P a, P b, float r) { return Piece{Piece::Kind::Capsule, {a, b}, r}; }
Piece box(P a, P b, float r) { return Piece{Piece::Kind::Box, {a, b}, r}; }
Piece circle(P c, float r) { return Piece{Piece::Kind::Circle, {c}, r}; }
Piece ring(P c, float r, float w) { return Piece{Piece::Kind::Ring, {c}, r, w}; }
Piece arc(P c, float r, float w, float a0, float a1) { return Piece{Piece::Kind::Arc, {c}, r, w, a0, a1}; }

// A shape: its body (one silhouette, edged), the marks over it (in the edge's colour), its point
struct Design {
    std::vector<Piece> body, marks;
    P hot{12, 12};
    bool red = false;  // «not there»: its body red
    uint32_t fillRgb = 0, edgeRgb = 0;  // 3.0.19-beta.10: its own colours in every look (0: the look's)
};

Design designOf(Shape s) {
    Design d;
    switch (s) {
        case Shape::Arrow:
            d.body = {polygon(
                {{2, 2}, {2, 19.2f}, {6.2f, 15.3f}, {9.1f, 21.6f}, {12, 20.4f}, {9.2f, 14.2f}, {15.1f, 14.2f}})};
            d.hot = {2, 2};
            break;
        case Shape::Hand:  // a forefinger pointing up, the knuckles of the others, the thumb
            d.body = {capsule({9, 3.2f}, {9, 12}, 2.0f), capsule({12.4f, 9.4f}, {12.4f, 12}, 1.8f),
                      capsule({15.6f, 10.4f}, {15.6f, 12.6f}, 1.7f), capsule({18.4f, 11.8f}, {18.4f, 13.6f}, 1.5f),
                      capsule({6.6f, 15.2f}, {4, 12.4f}, 1.7f), box({6.8f, 11}, {19.9f, 21.6f}, 3.4f)};
            d.marks = {capsule({10.7f, 12.6f}, {10.7f, 14.4f}, 0.45f), capsule({14, 13.2f}, {14, 14.8f}, 0.45f),
                       capsule({17.1f, 13.8f}, {17.1f, 15.2f}, 0.45f)};
            d.hot = {9, 1.2f};
            break;
        case Shape::Grab:  // a hand closing on a thing: its four fingers bent over the palm, the thumb
            d.body = {box({5, 7.4f}, {19.4f, 20.6f}, 4.2f), circle({7.4f, 7.6f}, 2.3f), circle({11, 6.6f}, 2.4f),
                      circle({14.6f, 6.8f}, 2.3f), circle({17.6f, 8.2f}, 2.1f),
                      capsule({5.4f, 15.4f}, {3, 12.6f}, 1.8f)};
            d.marks = {capsule({9.2f, 9.6f}, {9.2f, 11.4f}, 0.45f), capsule({12.8f, 9.4f}, {12.8f, 11.2f}, 0.45f),
                       capsule({16.2f, 9.8f}, {16.2f, 11.6f}, 0.45f)};
            break;
        case Shape::Talk:  // a speech balloon, its three dots
            d.body = {box({2.4f, 4}, {21.6f, 17.2f}, 5), polygon({{6, 15}, {5, 21.4f}, {11.6f, 16.4f}})};
            d.marks = {circle({7.6f, 10.6f}, 1.45f), circle({12, 10.6f}, 1.45f), circle({16.4f, 10.6f}, 1.45f)};
            d.hot = {12, 11};
            break;
        case Shape::Attack:  // a sword, its point up and left
            d.body = {polygon({{2, 2}, {7.2f, 3.6f}, {16.6f, 13}, {13, 16.6f}, {3.6f, 7.2f}}),
                      capsule({11.8f, 18.4f}, {18.4f, 11.8f}, 1.5f), capsule({16.4f, 16.4f}, {19.6f, 19.6f}, 1.4f),
                      circle({20.8f, 20.8f}, 1.9f)};
            d.marks = {capsule({5.2f, 5.2f}, {12.8f, 12.8f}, 0.45f)};
            d.hot = {2, 2};
            break;
        case Shape::Rotate:  // the camera turned: two arcs round, each with its head
            d.body = {arc({12, 12}, 7.6f, 2.6f, 3.6f, 5.75f), arc({12, 12}, 7.6f, 2.6f, 0.46f, 2.6f),
                      polygon({{19.3f, 5.7f}, {20.8f, 11.2f}, {15.4f, 9.6f}}),
                      polygon({{4.7f, 18.3f}, {3.2f, 12.8f}, {8.6f, 14.4f}})};
            break;
        case Shape::Door:  // a warp: a door, its panel and its knob
            d.body = {box({5.6f, 2.2f}, {18.4f, 21.8f}, 2.4f)};
            d.marks = {box({8.1f, 4.8f}, {15.9f, 11.6f}, 1.0f), box({8.1f, 13.2f}, {15.9f, 19.4f}, 1.0f),
                       circle({14.2f, 12.4f}, 0.9f)};
            break;
        case Shape::Invalid:  // not there: a ring struck through
            d.body = {ring({12, 12}, 8.4f, 3.0f), capsule({6.3f, 17.7f}, {17.7f, 6.3f}, 1.5f)};
            d.red = true;
            break;
        case Shape::Target:  // a target: a ring, its four ticks, its middle
            d.body = {ring({12, 12}, 6.8f, 2.0f), capsule({12, 1.6f}, {12, 5.6f}, 1.0f),
                      capsule({12, 18.4f}, {12, 22.4f}, 1.0f), capsule({1.6f, 12}, {5.6f, 12}, 1.0f),
                      capsule({18.4f, 12}, {22.4f, 12}, 1.0f), circle({12, 12}, 1.5f)};
            break;
        case Shape::TargetHot:  // 3.0.19-beta.10: over a monster - the target bolder, red in a dark red rim, any look
            d.body = {ring({12, 12}, 6.8f, 3.4f), capsule({12, 1.6f}, {12, 5.4f}, 1.4f),
                      capsule({12, 18.6f}, {12, 22.4f}, 1.4f), capsule({1.6f, 12}, {5.4f, 12}, 1.4f),
                      capsule({18.6f, 12}, {22.4f, 12}, 1.4f), circle({12, 12}, 2.2f)};
            d.fillRgb = 0xFF4A3Du;
            d.edgeRgb = 0x9C1C14u;
            break;
        case Shape::kCount: break;
    }
    return d;
}

float union_(const std::vector<Piece>& pieces, P p) {
    float d = 1e9f;
    for (const Piece& c : pieces) d = std::min(d, distanceOf(c, p));
    return d;
}

uint32_t mix(uint32_t a, uint32_t b, float t) {  // 0xRRGGBB, t of b
    uint32_t out = 0;
    for (int s = 0; s <= 16; s += 8)
        out |= uint32_t(std::lround(float((a >> s) & 255) * (1 - t) + float((b >> s) & 255) * t)) << s;
    return out;
}

// `rgb` at `a` over the premultiplied pixel
void over(uint32_t& px, uint32_t rgb, float a) {
    if (a <= 0) return;
    const float keep = 1 - a;
    uint32_t out = uint32_t(std::lround(a * 255 + float(px >> 24) * keep)) << 24;
    for (int s = 0; s <= 16; s += 8)
        out |= uint32_t(std::lround(float((rgb >> s) & 255) * a + float((px >> s) & 255) * keep)) << s;
    px = out;
}

}  // namespace

Shape shapeOf(int action) {
    switch (action) {
        case 1: return Shape::Talk;
        case 2: return Shape::Hand;
        case 4: return Shape::Rotate;
        case 5:
        case 6: return Shape::Attack;
        case 7:
        case 12: return Shape::Door;
        case 8: return Shape::Invalid;
        case 9: return Shape::Grab;
        case 10:
        case 11: return Shape::Target;
        default: return Shape::Arrow;
    }
}

Shape lookShapeOf(int action) {
    switch (action) {
        case 1: return Shape::Talk;
        case 2: return Shape::Hand;
        case 5:
        case 6: return Shape::TargetHot;
        case 7:
        case 12: return Shape::Door;
        case 8: return Shape::Invalid;
        case 9: return Shape::Grab;
        default: return Shape::Target;
    }
}

Art make(Shape shape, CursorLook look, float size) {
    Art art;
    if (look == CursorLook::Game || size <= 0 || shape >= Shape::kCount) return art;
    const Design d = designOf(shape);
    const bool dark = look == CursorLook::Dark;
    const uint32_t fill =
        d.fillRgb ? d.fillRgb : d.red ? (dark ? 0xD9534Au : 0xE5484Du) : dark ? 0x151A24u : 0xF7F8FAu;
    const uint32_t edge = d.edgeRgb ? d.edgeRgb : dark ? 0xE6CB8Au : 0x10141Cu;
    const uint32_t marks = d.red || d.edgeRgb ? edge : dark ? 0xE6CB8Au : 0x10141Cu;
    const float k = size / 24.0f, edgePx = std::max(1.0f, 1.4f * k);
    const float shadowX = 1.2f * k, shadowY = 1.6f * k, soft = std::max(1.5f, 2.0f * k);
    const int pad = int(std::ceil(soft + std::max(shadowX, shadowY) + 1));
    const int side = int(std::ceil(size)) + 2 * pad;
    art.picture.width = art.picture.height = side;
    art.picture.px.assign(size_t(side) * size_t(side), 0u);
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x) {
            const P p{(float(x) + 0.5f - float(pad)) / k, (float(y) + 0.5f - float(pad)) / k};
            uint32_t& px = art.picture.px[size_t(y) * size_t(side) + size_t(x)];
            // the shadow: the body moved right and down, soft
            const float sh = union_(d.body, P{p.x - shadowX / k, p.y - shadowY / k}) * k;
            over(px, 0x000000, 0.32f * std::clamp((soft - sh) / (2 * soft), 0.0f, 1.0f));
            const float b = union_(d.body, p) * k;
            const float cover = std::clamp(0.5f - b, 0.0f, 1.0f);
            if (cover > 0) {
                const float inner = std::clamp(-b - edgePx + 0.5f, 0.0f, 1.0f);  // past the edge: the fill
                over(px, mix(edge, fill, inner), cover);
            }
            if (!d.marks.empty()) {
                const float m = union_(d.marks, p) * k;
                over(px, marks, std::clamp(0.5f - m, 0.0f, 1.0f) * (cover > 0 ? 1.0f : 0.0f));
            }
        }
    art.hotX = float(pad) + d.hot.x * k;
    art.hotY = float(pad) + d.hot.y * k;
    return art;
}

}  // namespace uagfx::cursorart
