#include "core/world_map.hpp"

#include <algorithm>
#include <cmath>

#include "core/json.hpp"

namespace uagfx::worldmap {

bool parse(const std::string& text, Sheet& out) {
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error) || v.type() != json::Value::Object) return false;
    Sheet s;
    s.w = int(v["W"].asInt(0));
    s.h = int(v["H"].asInt(0));
    if (s.w <= 0 || s.h <= 0 || s.w > 8192 || s.h > 8192) return false;
    for (const json::Value& t : v["world"].items()) {
        Tile tile{t["id"].asString(), int(t["l"].asInt(0)), int(t["t"].asInt(0)), int(t["w"].asInt(0)),
                  int(t["h"].asInt(0))};
        if (!tile.map.empty() && tile.w > 0 && tile.h > 0) s.world.push_back(std::move(tile));
    }
    for (const json::Value& d : v["dungeon"].items())
        if (!d["id"].asString().empty()) s.dungeons.push_back(Dungeon{d["id"].asString(), d["group"].asString()});
    if (s.world.empty()) return false;
    out = std::move(s);
    return true;
}

void put(std::vector<uint32_t>& sheet, int sheetW, int sheetH, const Tile& t, const std::vector<uint32_t>& argb,
         int w, int h) {
    if (w <= 0 || h <= 0 || argb.size() < size_t(w) * size_t(h) || sheet.size() < size_t(sheetW) * size_t(sheetH))
        return;
    for (int y = std::max(0, t.t); y < std::min(sheetH, t.t + t.h); ++y)
        for (int x = std::max(0, t.l); x < std::min(sheetW, t.l + t.w); ++x) {
            // the picture's pixels under this one of the tile
            const int x0 = (x - t.l) * w / t.w, x1 = std::max(x0 + 1, (x - t.l + 1) * w / t.w);
            const int y0 = (y - t.t) * h / t.h, y1 = std::max(y0 + 1, (y - t.t + 1) * h / t.h);
            uint32_t r = 0, g = 0, b = 0, n = 0, all = 0;
            for (int py = y0; py < std::min(y1, h); ++py)
                for (int px = x0; px < std::min(x1, w); ++px) {
                    const uint32_t c = argb[size_t(py) * size_t(w) + size_t(px)];
                    ++all;
                    if ((c & 0xFFFFFF) == 0xFF00FF || (c >> 24) == 0) continue;  // the game's see-through
                    r += (c >> 16) & 0xFF;
                    g += (c >> 8) & 0xFF;
                    b += c & 0xFF;
                    ++n;
                }
            if (n * 2 < all || n == 0) continue;  // mostly see-through: the sea shows
            sheet[size_t(y) * size_t(sheetW) + size_t(x)] = 0xFF000000 | (r / n) << 16 | (g / n) << 8 | (b / n);
        }
}

Fit fit(int sheetW, int sheetH, int boxW, int boxH) {
    Fit f;
    if (sheetW <= 0 || sheetH <= 0 || boxW <= 0 || boxH <= 0) return f;
    f.s = std::min(float(boxW) / float(sheetW), float(boxH) / float(sheetH));
    f.x = (float(boxW) - float(sheetW) * f.s) / 2;
    f.y = (float(boxH) - float(sheetH) * f.s) / 2;
    return f;
}

Fit view(int sheetW, int sheetH, int boxW, int boxH, float zoom, float cx, float cy) {
    Fit f = fit(sheetW, sheetH, boxW, boxH);
    if (f.s <= 0) return f;
    f.s *= std::clamp(zoom, 1.0f, kMostZoom);
    if (cx < 0 || cy < 0) {
        cx = float(sheetW) / 2;
        cy = float(sheetH) / 2;
    }
    const float w = float(sheetW) * f.s, h = float(sheetH) * f.s;
    const float x = float(boxW) / 2 - cx * f.s, y = float(boxH) / 2 - cy * f.s;
    f.x = w <= float(boxW) ? (float(boxW) - w) / 2 : std::clamp(x, float(boxW) - w, 0.0f);
    f.y = h <= float(boxH) ? (float(boxH) - h) / 2 : std::clamp(y, float(boxH) - h, 0.0f);
    return f;
}

void zoomAt(int sheetW, int sheetH, int boxW, int boxH, float& zoom, float& cx, float& cy, float px, float py,
            float by) {
    const Fit was = view(sheetW, sheetH, boxW, boxH, zoom, cx, cy);
    if (was.s <= 0) return;
    const float sx = (px - was.x) / was.s, sy = (py - was.y) / was.s;  // the sheet's point under the pointer
    zoom = std::clamp(zoom * by, 1.0f, kMostZoom);
    const float s = fit(sheetW, sheetH, boxW, boxH).s * zoom;
    cx = sx + (float(boxW) / 2 - px) / s;  // that point under the pointer again
    cy = sy + (float(boxH) / 2 - py) / s;
}

void viewOn(int sheetW, int sheetH, int boxW, int boxH, float l, float t, float r, float b, float& zoom, float& cx,
            float& cy) {
    const float base = fit(sheetW, sheetH, boxW, boxH).s;
    const float w = std::max(r - l, 1.0f) * 1.3f, h = std::max(b - t, 1.0f) * 1.3f;  // a margin round it
    zoom = base > 0 ? std::clamp(std::min(float(boxW) / (w * base), float(boxH) / (h * base)), 1.0f, kMostZoom) : 1;
    cx = (l + r) / 2;
    cy = (t + b) / 2;
}

void scaled(panel::Picture& out, const std::vector<uint32_t>& sheet, int sheetW, int sheetH, int boxW, int boxH,
            const Fit& f) {
    out.width = std::max(0, boxW);
    out.height = std::max(0, boxH);
    out.px.assign(size_t(out.width) * size_t(out.height), kGround);
    if (f.s <= 0 || sheetW <= 0 || sheetH <= 0 || sheet.size() < size_t(sheetW) * size_t(sheetH)) return;
    if (f.s > 1) {  // 3.0.11-beta.29: zoomed in - each pixel between its four nearest of the sheet
        // each column's two sheet pixels and their weight once (every row alike); off the sheet: the ground
        struct Pair {
            int a, b;
            uint32_t w;  // of b, in 256ths
        };
        const auto pairOf = [&](int i, float origin, int size, Pair& p) {
            float v = (float(i) + 0.5f - origin) / f.s - 0.5f;
            if (v < -0.5f || v > float(size) - 0.5f) return false;
            v = std::clamp(v, 0.0f, float(size - 1));  // the edges' own colour to the very edge
            p.a = int(v);
            p.b = std::min(p.a + 1, size - 1);
            p.w = uint32_t((v - float(p.a)) * 256);
            return true;
        };
        std::vector<Pair> cols(size_t(out.width));
        std::vector<uint8_t> on(size_t(out.width));
        for (int x = 0; x < out.width; ++x) on[size_t(x)] = pairOf(x, f.x, sheetW, cols[size_t(x)]);
        // two channels at once (red and blue, then green): 255 x 256 fits each one's 16 bits
        const auto mix = [](uint32_t p, uint32_t q, uint32_t w) {
            const uint32_t rb = (((p & 0xFF00FFu) * (256 - w) + (q & 0xFF00FFu) * w) >> 8) & 0xFF00FFu;
            const uint32_t g = (((p & 0x00FF00u) * (256 - w) + (q & 0x00FF00u) * w) >> 8) & 0x00FF00u;
            return rb | g;
        };
        for (int y = 0; y < out.height; ++y) {
            Pair row{};
            if (!pairOf(y, f.y, sheetH, row)) continue;
            const uint32_t* top = sheet.data() + size_t(row.a) * size_t(sheetW);
            const uint32_t* low = sheet.data() + size_t(row.b) * size_t(sheetW);
            uint32_t* dst = out.px.data() + size_t(y) * size_t(out.width);
            for (int x = 0; x < out.width; ++x) {
                if (!on[size_t(x)]) continue;
                const Pair& c = cols[size_t(x)];
                dst[x] = 0xFF000000u | mix(mix(top[c.a], top[c.b], c.w), mix(low[c.a], low[c.b], c.w), row.w);
            }
        }
        return;
    }
    // the sheet's pixels under each column and row once (a box's average: the tiles' small things kept smooth)
    const auto spanOf = [&](int i, float origin, int size, int& from, int& to) {
        const float v0 = (float(i) - origin) / f.s, v1 = (float(i) + 1 - origin) / f.s;
        if (v1 <= 0 || v0 >= float(size)) return false;
        from = std::clamp(int(v0), 0, size - 1);
        to = std::clamp(int(std::ceil(v1)), from + 1, size);
        return true;
    };
    std::vector<int> ax(size_t(out.width)), bx(size_t(out.width));
    std::vector<uint8_t> on(size_t(out.width));
    for (int x = 0; x < out.width; ++x) on[size_t(x)] = spanOf(x, f.x, sheetW, ax[size_t(x)], bx[size_t(x)]);
    for (int y = 0; y < out.height; ++y) {
        int ay = 0, by = 0;
        if (!spanOf(y, f.y, sheetH, ay, by)) continue;
        uint32_t* dst = out.px.data() + size_t(y) * size_t(out.width);
        for (int x = 0; x < out.width; ++x) {
            if (!on[size_t(x)]) continue;
            uint32_t r = 0, g = 0, b = 0, n = 0;
            for (int py = ay; py < by; ++py) {
                const uint32_t* src = sheet.data() + size_t(py) * size_t(sheetW);
                for (int px = ax[size_t(x)]; px < bx[size_t(x)]; ++px) {
                    const uint32_t c = src[px];
                    r += (c >> 16) & 0xFF;
                    g += (c >> 8) & 0xFF;
                    b += c & 0xFF;
                    ++n;
                }
            }
            dst[x] = 0xFF000000 | (r / n) << 16 | (g / n) << 8 | (b / n);
        }
    }
}

int tileAt(const Sheet& sheet, const Fit& f, float x, float y) {
    if (f.s <= 0) return -1;
    const float sx = (x - f.x) / f.s, sy = (y - f.y) / f.s;
    int best = -1;
    for (size_t i = 0; i < sheet.world.size(); ++i) {
        const Tile& t = sheet.world[i];
        if (sx < float(t.l) || sy < float(t.t) || sx >= float(t.l + t.w) || sy >= float(t.t + t.h)) continue;
        if (best < 0 || t.w * t.h < sheet.world[size_t(best)].w * sheet.world[size_t(best)].h) best = int(i);
    }
    return best;
}

const Tile* tileOf(const Sheet& sheet, const std::string& map) {
    for (const Tile& t : sheet.world)
        if (t.map == map) return &t;
    return nullptr;
}

bool cellOnSheet(const Tile& t, int mapW, int mapH, int x, int y, float& sx, float& sy) {
    if (mapW <= 0 || mapH <= 0 || x < 0 || y < 0 || x >= mapW || y >= mapH) return false;
    sx = float(t.l) + (float(x) + 0.5f) / float(mapW) * float(t.w);
    sy = float(t.t) + (float(mapH) - float(y) - 0.5f) / float(mapH) * float(t.h);  // north up
    return true;
}

}  // namespace uagfx::worldmap
