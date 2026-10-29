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

void scaled(panel::Picture& out, const std::vector<uint32_t>& sheet, int sheetW, int sheetH, int boxW, int boxH,
            const Fit& f) {
    out.width = std::max(0, boxW);
    out.height = std::max(0, boxH);
    out.px.assign(size_t(out.width) * size_t(out.height), kGround);
    if (f.s <= 0 || sheet.size() < size_t(sheetW) * size_t(sheetH)) return;
    for (int y = 0; y < out.height; ++y) {
        const float sy0 = (float(y) - f.y) / f.s, sy1 = (float(y) + 1 - f.y) / f.s;
        if (sy1 <= 0 || sy0 >= float(sheetH)) continue;
        for (int x = 0; x < out.width; ++x) {
            const float sx0 = (float(x) - f.x) / f.s, sx1 = (float(x) + 1 - f.x) / f.s;
            if (sx1 <= 0 || sx0 >= float(sheetW)) continue;
            // the sheet's pixels under this one (a box's average: the tiles' small things kept smooth)
            const int ax = std::clamp(int(sx0), 0, sheetW - 1), bx = std::clamp(int(std::ceil(sx1)), ax + 1, sheetW);
            const int ay = std::clamp(int(sy0), 0, sheetH - 1), by = std::clamp(int(std::ceil(sy1)), ay + 1, sheetH);
            uint32_t r = 0, g = 0, b = 0, n = 0;
            for (int py = ay; py < by; ++py)
                for (int px = ax; px < bx; ++px) {
                    const uint32_t c = sheet[size_t(py) * size_t(sheetW) + size_t(px)];
                    r += (c >> 16) & 0xFF;
                    g += (c >> 8) & 0xFF;
                    b += c & 0xFF;
                    ++n;
                }
            out.px[size_t(y) * size_t(out.width) + size_t(x)] = 0xFF000000 | (r / n) << 16 | (g / n) << 8 | (b / n);
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
