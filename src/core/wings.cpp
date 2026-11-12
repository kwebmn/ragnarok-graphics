#include "core/wings.hpp"

#include <algorithm>
#include <cmath>
#include <map>

#include "core/updates.hpp"

namespace uagfx::wings {
namespace {

struct Stop {
    float t;
    uint8_t r, g, b;
};
using Stops = std::vector<Stop>;

// The concept's gradients (its renderer's), darkest to lightest
const Stops& stopsOf(Look look) {
    static const Stops none;
    static const Stops fire{{0, 90, 10, 0}, {0.35f, 200, 50, 10}, {0.7f, 255, 150, 30}, {1, 255, 240, 150}};
    static const Stops ice{{0, 20, 50, 120}, {0.4f, 60, 140, 220}, {0.75f, 150, 220, 250}, {1, 240, 252, 255}};
    static const Stops valkyrie{{0, 150, 105, 30}, {0.3f, 225, 185, 80}, {0.55f, 240, 236, 225}, {1, 255, 255, 255}};
    static const Stops gold{{0, 120, 70, 10}, {0.45f, 215, 160, 40}, {0.8f, 250, 225, 130}, {1, 255, 250, 220}};
    static const Stops sakura{{0, 150, 60, 100}, {0.45f, 235, 140, 180}, {0.8f, 255, 205, 225}, {1, 255, 240, 246}};
    static const Stops crimson{{0, 25, 0, 5}, {0.4f, 110, 10, 20}, {0.75f, 200, 40, 40}, {1, 255, 140, 110}};
    switch (look) {
        case Look::Fire: return fire;
        case Look::Ice: return ice;
        case Look::Valkyrie: return valkyrie;
        case Look::Gold: return gold;
        case Look::Sakura: return sakura;
        case Look::Crimson: return crimson;
        default: return none;
    }
}

uint32_t gradient(const Stops& s, float t) {
    for (size_t k = 0; k + 1 < s.size(); ++k) {
        if (t > s[k + 1].t && k + 2 < s.size()) continue;
        const float span = s[k + 1].t - s[k].t;
        const float u = span > 0 ? std::clamp((t - s[k].t) / span, 0.0f, 1.0f) : 0.0f;
        const auto mix = [u](uint8_t a, uint8_t b) { return uint32_t(std::lround(float(a) + float(b - a) * u)); };
        return mix(s[k].r, s[k + 1].r) << 16 | mix(s[k].g, s[k + 1].g) << 8 | mix(s[k].b, s[k + 1].b);
    }
    return 0;
}

float brightness(uint32_t argb) {
    return (0.299f * float((argb >> 16) & 255) + 0.587f * float((argb >> 8) & 255) + 0.114f * float(argb & 255)) / 255;
}

// CP949: 인간족\몸통 - the human bodies' folder (under data\sprite)
const std::string kBodies = "\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xF6\xC5\xEB\\";

}  // namespace

// 3.0.18-beta.232 (the owner's «давай»): views the 2007 client has names for (it has up to about 326), of items only
// the GM has - .230's (365, 382, 613, 595, 568, 477, 438, 562) were past them: «Cannot find File» in every plain client
Look lookOfView(int view) {
    switch (view) {
        case 260: return Look::Angel;     // Angel Blessing 5207
        case 233: return Look::Fallen;    // Helm of Darkness 5181
        case 129: return Look::Demon;     // Wings of Diablo 5044, Refined Wing of Diablo 5328
        case 221: return Look::Fire;      // Phoenix Crown 5152, 5271
        case 301: return Look::Ice;       // Soul Wing 5297, Soul Ring 5257
        case 300: return Look::Valkyrie;  // Valkyrie Feather Band 5256
        case 261: return Look::Gold;      // Emperor's Laurel Crown 5205, Sacred Palma 5320, Emperor Wreath 5533
        case 325: return Look::Sakura;    // Flower Crown 5303
        case 271: return Look::Crimson;   // Red Deviruchi Hat 5227
        default: return Look::None;
    }
}

Look lookOfViews(const int views[3]) {
    for (int i = 0; i < 3; ++i)
        if (const Look l = lookOfView(views[i]); l != Look::None) return l;
    return Look::None;
}

const char* lookName(Look look) {
    static const char* const kNames[] = {"none", "angel", "fallen", "demon", "fire", "ice",
                                         "valkyrie", "gold", "sakura", "crimson"};
    const size_t i = size_t(look);
    return i < sizeof kNames / sizeof kNames[0] ? kNames[i] : "none";
}

Set setOf(Look look) {
    switch (look) {
        case Look::Fallen: return Set::Fallen;
        case Look::Demon:
        case Look::Crimson: return Set::Dark;
        default: return Set::Angel;
    }
}

void recolour(spritefile::Sprite& s, Look look) {
    const Stops& stops = stopsOf(look);
    if (stops.empty()) return;
    std::map<uint32_t, float> lum;  // the distinct colours (RGB) the pictures show
    for (const spritefile::Image& im : s.palette)
        for (uint32_t p : im.argb)
            if (p >> 24) lum.emplace(p & 0xFFFFFF, brightness(p));
    if (lum.empty()) return;
    float lo = 1, hi = 0;
    for (const auto& [rgb, l] : lum) lo = std::min(lo, l), hi = std::max(hi, l);
    std::map<uint32_t, uint32_t> to;
    for (const auto& [rgb, l] : lum) to[rgb] = gradient(stops, hi > lo ? (l - lo) / (hi - lo) : 1.0f);
    for (spritefile::Image& im : s.palette)
        for (uint32_t& p : im.argb)
            if (p >> 24) p = (p & 0xFF000000u) | to[p & 0xFFFFFF];
}

std::string garmentEntry(Set set, const std::string& bodySprite, const char* ext) {
    const std::string key = updates::lookupKey(bodySprite), folder = updates::lookupKey(kBodies);
    const size_t at = key.find(folder);  // .235: wherever it starts (lookupKey keeps the length)
    if (at == std::string::npos || (at > 0 && key[at - 1] != '\\')) return {};
    std::string rest = bodySprite.substr(at + folder.size());  // <sex>\<body>.spr
    const size_t dot = rest.rfind('.');
    if (dot == std::string::npos || rest.find('\\') == std::string::npos) return {};
    rest = rest.substr(0, dot);
    if (set == Set::Dark) return std::string("dark.") + ext;
    return std::string(set == Set::Fallen ? "fallen\\" : "angel\\") + rest + "." + ext;
}

std::string dataPath(const std::string& resource) {
    if (resource.empty()) return {};
    const std::string key = updates::lookupKey(resource);
    if (key.compare(0, 5, "data\\") == 0) return resource;
    if (key.compare(0, 7, "sprite\\") == 0) return "data\\" + resource;
    return "data\\sprite\\" + resource;
}

bool pastTable(int view, uint32_t first, uint32_t last) {
    if (view <= 0) return false;
    if (!first || last < first) return true;
    return uint32_t(view) >= (last - first) / 4;
}

std::vector<Quad> quads(const spritefile::Act& garment, const spritefile::Act& body, const spritefile::Sprite& s,
                        size_t action, size_t frame, const Transform& t) {
    std::vector<Quad> out;
    if (action >= garment.actions.size() || garment.actions[action].empty()) return out;
    const auto& gframes = garment.actions[action];
    const spritefile::Frame& g = gframes[frame % gframes.size()];
    int ax = 0, ay = 0;  // the body's anchor less the garment's
    if (action < body.actions.size() && !body.actions[action].empty() && !g.anchors.empty()) {
        const auto& bframes = body.actions[action];
        const spritefile::Frame& b = bframes[frame % bframes.size()];
        if (!b.anchors.empty()) {
            ax = b.anchors[0].first - g.anchors[0].first;
            ay = b.anchors[0].second - g.anchors[0].second;
        }
    }
    for (const spritefile::Layer& l : g.layers) {
        const std::vector<spritefile::Image>& pics = l.type == 1 ? s.truecolour : s.palette;
        if (l.index < 0 || size_t(l.index) >= pics.size()) continue;
        const spritefile::Image& im = pics[size_t(l.index)];
        if (im.w <= 0 || im.h <= 0) continue;
        const float w = float(im.w) * std::fabs(l.scaleX), h = float(im.h) * std::fabs(l.scaleY);
        Quad q;
        q.x0 = t.x + (float(l.x + ax) - w / 2) * t.scale;
        q.y0 = t.y + (float(l.y + ay) - h / 2) * t.scale;
        q.x1 = q.x0 + w * t.scale;
        q.y1 = q.y0 + h * t.scale;
        q.image = l.index;
        q.type = l.type;
        q.mirror = l.mirror;
        q.tint = l.tint;
        out.push_back(q);
    }
    return out;
}

bool facesAway(size_t action) {
    const size_t side = action % 8;
    return side >= 3 && side <= 5;
}

float lerpAt(float y, float y0, float v0, float y1, float v1) {
    if (std::fabs(y1 - y0) < 1e-6f) return v0;
    return v0 + (v1 - v0) * (y - y0) / (y1 - y0);
}

int Frame::begin(uint32_t actor, bool away) {
    jobs_.push_back(Job{actor, away, 0, 0});
    return int(jobs_.size()) - 1;
}

void Frame::face(int job, uint32_t vertices) {
    if (job < 0 || size_t(job) >= jobs_.size() || !vertices) return;
    if (byVertices_.emplace(vertices, job).second) ++jobs_[size_t(job)].faces;
}

Frame::Emit Frame::drawn(uint32_t vertices) {
    Emit e;
    const auto it = byVertices_.find(vertices);
    if (it == byVertices_.end()) return e;
    Job& j = jobs_[size_t(it->second)];
    ++j.drawn;
    if (!j.away && j.drawn == 1) e.before = it->second;
    if (j.away && j.drawn == j.faces) e.after = it->second;
    return e;
}

void Frame::clear() {
    jobs_.clear();
    byVertices_.clear();
}

uint32_t Frame::actor(int job) const {
    return job >= 0 && size_t(job) < jobs_.size() ? jobs_[size_t(job)].actor : 0;
}

}  // namespace uagfx::wings
