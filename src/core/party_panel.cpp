#include "core/party_panel.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_icons.hpp"
#include "core/ui_style.hpp"

namespace uagfx::partypanel {
namespace {

// The concept's sizes at scale 1 (v19_cave): the panel 252 wide, 8 in, a row 40 (its round 20, its name 13, its bar 5)
constexpr float kW = 252, kPad = 8, kRowH = 40, kRound = 20, kBarH = 5, kValueW = 76;
constexpr float kHeaderH = 26;  // 3.0.11-beta.79
constexpr float kBuffSide = 14, kBuffGap = 3, kBuffLine = 20;  // 3.0.11-beta.80: an icon, between, its line
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kTile = 0x1B1D24;
constexpr uint32_t kGold = 0xE6CB8A, kText = 0xECEFF5, kMuted = 0x9AA3B5, kHp = 0x5BD36C, kRed = 0xFF5A4E;
constexpr uint32_t kRedText = 0xFF9A8F;
constexpr uint32_t kCast = 0x8E9BFF, kDeadGround = 0x5A1414;  // 3.0.11-beta.81

bool here(const Member& m) { return m.online && m.where.empty(); }

}  // namespace

bool low(const Member& m, int threshold) { return here(m) && m.percent >= 0 && m.percent < threshold; }

Layout layout(size_t members, int left, int top, float scale, bool folded) {
    return layout(std::vector<int>(members, 0), left, top, scale, folded, false);
}

std::vector<int> rowsOf(const std::vector<Member>& members) {
    std::vector<int> out;
    for (const Member& m : members) out.push_back(int(m.buffs.size()));
    return out;
}

Layout layout(const std::vector<int>& buffs, int left, int top, float scale, bool folded, bool compact) {
    const size_t members = buffs.size();
    Layout L;
    L.compact = compact;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    L.x = left;
    L.y = top;
    L.folded = folded;
    if (!members) return L;
    const float w = kW * s, pad = kPad * s;
    L.header = panel::Box{pad, pad * 0.5f, w - 2 * pad, kHeaderH * s};  // 3.0.11-beta.79
    L.fold = panel::Box{L.header.x + L.header.w - 24 * s, L.header.y + 3 * s, 22 * s, L.header.h - 6 * s};
    L.view = panel::Box{L.fold.x - 24 * s, L.fold.y, 22 * s, L.fold.h};  // 3.0.11-beta.84
    L.count = panel::Box{L.view.x - 52 * s, L.header.y, 48 * s, L.header.h};
    L.title = panel::Box{L.header.x + 4 * s, L.header.y, L.count.x - L.header.x - 8 * s, L.header.h};
    const float top0 = L.header.y + L.header.h;
    if (folded) {
        L.glass = panel::Box{0, 0, w, top0 + pad * 0.5f};
        L.w = int(std::ceil(L.glass.w));
        L.h = int(std::ceil(L.glass.h));
        return L;
    }
    float y = top0;
    if (compact) {  // 3.0.11-beta.84: two columns of small cells
        const float gap = 6 * s, cellW = (w - 2 * pad - gap) / 2, cellH = 30 * s;
        for (size_t i = 0; i < members; ++i) {
            Row r;
            r.box = panel::Box{pad + float(i % 2) * (cellW + gap), top0 + float(i / 2) * cellH, cellW, cellH};
            r.round = panel::Box{r.box.x + 2 * s, r.box.y + 6 * s, 16 * s, 16 * s};
            const float x0 = r.round.x + r.round.w + 5 * s;
            r.name = panel::Box{x0, r.box.y + 3 * s, r.box.x + r.box.w - 3 * s - x0, 15 * s};
            r.bar = panel::Box{x0, r.box.y + 20 * s, r.box.x + r.box.w - 3 * s - x0, 4 * s};
            L.rows.push_back(r);
        }
        y = top0 + float((members + 1) / 2) * cellH;
        L.glass = panel::Box{0, 0, w, y + pad};
        L.w = int(std::ceil(L.glass.w));
        L.h = int(std::ceil(L.glass.h));
        return L;
    }
    for (size_t i = 0; i < members; ++i) {
        Row r;
        const bool line = buffs[i] > 0;  // 3.0.11-beta.80: its buffs a line of their own under its bar
        r.box = panel::Box{pad, y, w - 2 * pad, (kRowH + (line ? kBuffLine : 0)) * s};
        y += r.box.h;
        r.round = panel::Box{r.box.x + 2 * s, r.box.y + 6 * s, kRound * s, kRound * s};
        const float x0 = r.round.x + r.round.w + 8 * s;
        r.value = panel::Box{r.box.x + r.box.w - kValueW * s - 4 * s, r.box.y + 5 * s, kValueW * s, 18 * s};
        r.name = panel::Box{x0, r.box.y + 5 * s, r.value.x - 4 * s - x0, 18 * s};
        // 3.0.11-beta.82: its arrow and cells away at the end of its bar's line
        r.where = panel::Box{r.box.x + r.box.w - 44 * s, r.box.y + 22 * s, 40 * s, 14 * s};
        r.bar = panel::Box{x0, r.box.y + 27 * s, r.where.x - 4 * s - x0, kBarH * s};
        if (line) {
            const float side = kBuffSide * s, step = (kBuffSide + kBuffGap) * s;
            const int fit = std::max(0, int((r.box.x + r.box.w - 4 * s - x0 + kBuffGap * s) / step));
            for (int b = 0; b < std::min(buffs[i], fit); ++b)
                r.buffs.push_back(panel::Box{x0 + float(b) * step, r.bar.y + r.bar.h + 5 * s, side, side});
        }
        L.rows.push_back(r);
    }
    L.glass = panel::Box{0, 0, w, y + pad};
    L.w = int(std::ceil(L.glass.w));
    L.h = int(std::ceil(L.glass.h));
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (!L.glass.contains(x, y)) return {};
    if (L.fold.contains(x, y)) return Hit{Part::Fold, -1};
    if (L.view.contains(x, y)) return Hit{Part::View, -1};  // 3.0.11-beta.84
    if (L.header.contains(x, y)) return Hit{Part::Header, -1};
    for (size_t i = 0; i < L.rows.size(); ++i)
        if (L.rows[i].box.contains(x, y)) return Hit{Part::Row, int(i)};
    return Hit{Part::Header, -1};  // its glass between: the panel's
}

}  // namespace uagfx::partypanel
