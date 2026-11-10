#include "core/pick_card.hpp"

#include <algorithm>
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

void put(panel::Picture& out, const panel::Picture& pic, const panel::Box& box) {
    const int ox = int(std::lround(box.x + (box.w - float(pic.width)) / 2));
    const int oy = int(std::lround(box.y + (box.h - float(pic.height)) / 2));
    if (out.scene) {  // 3.0.18-beta.12: recorded for uaUI
        panel::recordImage(out, pic, float(ox), float(oy), 1.0f);
        return;
    }
    for (int y = 0; y < pic.height; ++y)
        for (int x = 0; x < pic.width; ++x) {
            const int tx = ox + x, ty = oy + y;
            if (tx < 0 || ty < 0 || tx >= out.width || ty >= out.height) continue;
            const uint32_t c = pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
            if (!(c >> 24)) continue;
            uint32_t& d = out.px[size_t(ty) * size_t(out.width) + size_t(tx)];
            const uint32_t keep = 255 - (c >> 24);
            const auto ch = [&](int sh) {
                return std::min<uint32_t>(255, ((c >> sh) & 0xFF) + ((d >> sh) & 0xFF) * keep / 255);
            };
            d = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
        }
}

panel::Box iconBox(const panel::Box& row, float s) {
    return panel::Box{row.x + 6 * s, row.y + (row.h - kIcon * s) / 2, kIcon * s, kIcon * s};
}

// 3.0.11-beta.187: a row's icon - none (the servers'): no square, the name from the row's left
bool hasIcon(const Entry& e) { return e.icon && e.icon->width > 0 && e.icon->height > 0; }

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

panel::Box thumb(const Layout& L, int scroll, size_t count) {
    const int most = mostScroll(L, count);
    if (L.bar.w <= 0 || count == 0 || most <= 0) return panel::Box{L.bar.x, L.bar.y, L.bar.w, 0};
    const float h = std::max(L.bar.w * 3, L.bar.h * float(L.rows.size()) / float(count));
    const float at = float(std::clamp(scroll, 0, most)) / float(most);
    return panel::Box{L.bar.x, L.bar.y + (L.bar.h - h) * at, L.bar.w, h};
}

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

void paint(panel::Picture& out, const Layout& L, const std::vector<Entry>& entries, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, uistyle::window(), kWhite, 0.12f, 1.0f * s);
    if (L.rows.empty()) panel::roundRect(out, L.list, 9 * s, kBlack, 0.20f);
    for (size_t i = 0; i < L.rows.size(); ++i) {
        const size_t at = i + size_t(std::max(0, sh.scroll));
        if (at >= entries.size()) break;
        const bool chosen = int(at) == sh.selected, hot = int(at) == sh.hover;
        panel::roundRect(out, L.rows[i], 8 * s, chosen ? kGold : kWhite,
                         chosen ? 0.16f : hot ? 0.07f : 0.035f, chosen ? kGold : kWhite, chosen ? 0.9f : 0.08f,
                         (chosen ? 1.5f : 1.0f) * s);
        if (!hasIcon(entries[at])) continue;
        const panel::Box icon = iconBox(L.rows[i], s);
        panel::roundRect(out, icon, 6 * s, kBlack, 0.30f);
        put(out, *entries[at].icon, icon);
    }
    if (L.bar.w > 0) {  // 3.0.11-beta.187: the scroll's bar and its thumb
        panel::roundRect(out, L.bar, L.bar.w / 2, kWhite, 0.06f);
        const panel::Box t = thumb(L, sh.scroll, entries.size());
        if (t.h > 0) panel::roundRect(out, t, L.bar.w / 2, kWhite, 0.40f);
    }
    const bool can = sh.selected >= 0 && sh.selected < int(entries.size());
    panel::roundRect(out, L.ok, 8 * s, kGold, !can ? 0.05f : sh.hoverPart == int(Part::Ok) ? 0.34f : 0.22f, kGold,
                     can ? 0.8f : 0.25f, 1.0f * s);
    panel::roundRect(out, L.cancel, 8 * s, kWhite, sh.hoverPart == int(Part::Cancel) ? 0.12f : 0.06f, kWhite, 0.16f,
                     1.0f * s);
}

std::vector<Words> words(const Layout& L, const std::vector<Entry>& entries, const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, t.title, kText, 15 * s, true, Align::Left});
    out.push_back(Words{L.message, t.message, kMuted, 12.5f * s, false, Align::Left});
    if (L.rows.empty()) out.push_back(Words{L.list, t.empty, kMuted, 13 * s, false, Align::Center});
    for (size_t i = 0; i < L.rows.size(); ++i) {
        const size_t at = i + size_t(std::max(0, sh.scroll));
        if (at >= entries.size()) break;
        const Entry& e = entries[at];
        const panel::Box icon = iconBox(L.rows[i], s);
        const float from = hasIcon(e) ? icon.x + icon.w + 10 * s : L.rows[i].x + 12 * s;
        const panel::Box name{from, L.rows[i].y, L.rows[i].x + L.rows[i].w - 6 * s - from, L.rows[i].h};
        out.push_back(Words{name, e.refine > 0 ? "+" + std::to_string(e.refine) + " " + e.name : e.name, kText,
                            13 * s, int(at) == sh.selected, Align::Left});
    }
    if (!t.hint.empty()) out.push_back(Words{L.hint, t.hint, kMuted, 11 * s, false, Align::Center});
    out.push_back(Words{L.ok, t.ok, kText, 13.5f * s, true, Align::Center});
    out.push_back(Words{L.cancel, t.cancel, kText, 13.5f * s, false, Align::Center});
    return out;
}

panel::Box iconOf(const panel::Box& row, const Entry& e, float s) {
    return hasIcon(e) ? iconBox(row, s) : panel::Box{};
}

}  // namespace uagfx::pickcard
