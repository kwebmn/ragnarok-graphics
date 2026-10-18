#pragma once
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/texture_catalog.hpp"

namespace uagfx::studio {

// 2.3, uaRO Studio: the compare view's arithmetic (T2) and the list's filter - plain data, tested.

// Where a pane looks: zoom pane pixels a texel of the original, (x, y) the original's texel at its top left.
struct View {
    float zoom = 1;
    float x = 0, y = 0;
};
constexpr float kLeastZoom = 0.25f, kMostZoom = 16.0f;
// zoom times factor (within the bounds), the texel under the cursor staying under it
View zoomAt(View v, float factor, float cursorX, float cursorY);
// The texel of a picture (pw x ph) of an original (w x h) under a pane's pixel, held within the picture; inside: the
// pixel shows the texture (not the ground about it).
void sourceTexel(const View& v, int paneX, int paneY, int w, int h, int pw, int ph, int& sx, int& sy, bool& inside);
// Two pictures (any sizes) at w x h by their nearest texels: |a - b| of each channel times 4, opaque.
std::vector<uint32_t> differenceImage(const uint32_t* a, int aw, int ah, const uint32_t* b, int bw, int bh, int w,
                                      int h);

// What the list knows of a row besides the catalog: its state in the pack, the model that made its picture, its
// quality flags (ai::Flag), whether its material is changed.
struct RowFacts {
    catalog::State state = catalog::State::Missing;
    std::string model;
    uint16_t flags = 0;
    bool overrides = false;
};
// The navigator's place ("" all; "map:<index>", "folder:<prefix>", "state:missing|inpack|own", "model:<name>",
// "flagged", "overrides", "seen:<map>" with the report's keys) and the text typed (a file's name or a key, any case).
struct RowFilter {
    std::string nav, text;
    std::unordered_set<std::string> keys;
};
bool rowShown(const catalog::FileEntry& f, const catalog::PictureOf& p, const RowFacts& facts, const RowFilter& r);

}  // namespace uagfx::studio
