#include "core/pick_card.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::pickcard {
namespace {

// At scale 1: the card 400 wide, 18 in; its title 30, its line 22; a row 40 (7 shown at most); the buttons 38
constexpr float kW = 400, kPad = 18, kTitleH = 30, kMessageH = 22, kRowH = 40, kButtonH = 38, kGap = 10, kIcon = 28;
constexpr size_t kMostRows = 7;
constexpr float kBarW = 4, kBarGap = 8;  // 3.0.11-beta.187: the scroll's bar and its room beside the rows
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kText = 0xEEF2F8, kMuted = 0x9AA3B5;
constexpr uint32_t kBlack = 0x000000;

}  // namespace

Layout layout(int targetW, int targetH, float scale, size_t count) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s, row = kRowH * s;
    L.title = panel::Box{pad, pad, w - 2 * pad, kTitleH * s};
    L.message = panel::Box{pad, pad + kTitleH * s, w - 2 * pad, kMessageH * s};
    const float top = pad + kTitleH * s + kMessageH * s + 8 * s;
    const size_t shown = std::min(count, kMostRows);
    L.list = panel::Box{pad, top, w - 2 * pad, row * float(std::max<size_t>(1, shown))};
    const float bar = count > kMostRows ? (kBarW + kBarGap) * s : 0.0f;  // 3.0.11-beta.187
    if (bar > 0) L.bar = panel::Box{pad + L.list.w - kBarW * s, top, kBarW * s, L.list.h};
    for (size_t i = 0; i < shown; ++i)
        L.rows.push_back(panel::Box{pad, top + float(i) * row, w - 2 * pad - bar, row - 4 * s});
    float y = top + L.list.h + 6 * s;
    L.hint = panel::Box{pad, y, w - 2 * pad, 16 * s};
    y += 20 * s;
    const float bw = (w - 2 * pad - kGap * s) / 2;
    L.ok = panel::Box{pad, y, bw, kButtonH * s};
    L.cancel = panel::Box{pad + bw + kGap * s, y, bw, kButtonH * s};
    y += kButtonH * s + pad;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::max(0, targetH / 2 - L.h / 2);
    return L;
}

int mostScroll(const Layout& L, size_t count) { return std::max(0, int(count) - int(L.rows.size())); }

int autoChoice(size_t entries, int chosen) {
    (void)chosen;  // one row: whatever the list says, it is that one
    return entries == 1 ? 0 : -1;
}

Hit hitTest(const Layout& L, int scroll, size_t count, float x, float y) {
    for (size_t i = 0; i < L.rows.size(); ++i) {
        const int row = int(i) + scroll;
        if (row >= int(count)) break;
        if (L.rows[i].contains(x, y)) return Hit{Part::Row, row};
    }
    if (L.ok.contains(x, y)) return Hit{Part::Ok, -1};
    if (L.cancel.contains(x, y)) return Hit{Part::Cancel, -1};
    return Hit{};
}

Place placeOf(const std::map<std::string, std::string>& names, const std::string& row) {
    std::string code;
    for (const char c : row) code += char(std::tolower(static_cast<unsigned char>(c)));
    for (const char* end : {".gat", ".rsw"})
        if (code.size() > 4 && code.compare(code.size() - 4, 4, end) == 0) code.resize(code.size() - 4);
    const auto named = names.find(code);
    if (named == names.end() || named->second.empty()) return Place{row, ""};
    std::string name;
    for (size_t i = 0; i < named->second.size(); ++i) {
        name += named->second[i];
        if (named->second[i] == ',' && i + 1 < named->second.size() && named->second[i + 1] != ' ') name += ' ';
    }
    return Place{name, code};
}

}  // namespace uagfx::pickcard
