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

std::vector<Words> words(const Layout& L, const std::vector<Member>& members, int threshold,
                         const std::string& offline, const std::string& title, const std::string& dead) {
    std::vector<Words> out;
    const float s = L.s;
    if (L.header.w > 0) {  // 3.0.11-beta.79: «Група», online of all, ▾ / ▸
        int online = 0;
        for (const Member& m : members) online += m.online;
        out.push_back(Words{L.title, title, kMuted, 12 * s, true, Align::Left});
        out.push_back(Words{L.count, std::to_string(online) + "/" + std::to_string(members.size()), kMuted, 12 * s,
                            false, Align::Right});
        out.push_back(Words{L.fold, L.folded ? "\xE2\x96\xB8" : "\xE2\x96\xBE", kText, 12 * s, false, Align::Center});
        out.push_back(Words{L.view, L.compact ? "\xE2\x96\xA4" : "\xE2\x96\xA6", kText, 12 * s, false,
                            Align::Center});  // 3.0.11-beta.84: ▤ the list, ▦ compact
    }
    for (size_t i = 0; i < L.rows.size() && i < members.size(); ++i) {
        const Row& r = L.rows[i];
        const Member& m = members[i];
        const bool bad = low(m, threshold);
        const bool wide = !m.name.empty() && (static_cast<unsigned char>(m.name[0]) & 0xE0) == 0xC0;  // UTF-8 of 2
        const std::string first = m.name.empty() ? "?" : m.name.substr(0, wide ? 2 : 1);
        if (!m.face || m.face->width <= 0)
            out.push_back(Words{r.round, first, kMuted, (L.compact ? 9 : 10.5f) * s, true, Align::Center});
        const std::string shown = bad ? "! " + m.name : m.name;
        out.push_back(Words{r.name, shown, bad || m.dead ? kRedText : here(m) ? kText : kMuted,
                            (L.compact ? 11.5f : 13) * s, true, Align::Left});
        if (L.compact) continue;  // 3.0.11-beta.84: a cell - its name and bar alone
        // 3.0.11-beta.81: after its name - its cast's skill (blue), else its class and level (faint)
        const std::string after = !m.casting.empty() && m.castPart >= 0 ? m.casting : m.job;
        const float nameW = panel::wordsWidth(shown, 13 * s, true) + 6 * s;
        if (!after.empty() && nameW + 24 * s < r.name.w)
            out.push_back(Words{panel::Box{r.name.x + nameW, r.name.y + 1 * s, r.name.w - nameW, r.name.h}, after,
                                after == m.job ? kMuted : kCast, 11.5f * s, false, Align::Left});
        std::string value;
        uint32_t rgb = kMuted;
        if (m.dead && m.online && !dead.empty()) {  // 3.0.11-beta.81
            value = dead;
            rgb = kRedText;
        } else if (!m.online) value = offline;
        else if (!m.where.empty()) value = m.where;
        else if (m.percent >= 0) {
            value = std::to_string(m.percent) + " %";
            rgb = bad ? kRedText : kText;
        }
        if (!value.empty())
            out.push_back(Words{r.value, value, rgb, (here(m) ? 13 : 12) * s, here(m), Align::Right});
        if (m.cells >= 0 && here(m))  // 3.0.11-beta.82: its cells away, by its arrow
            out.push_back(Words{panel::Box{r.where.x + r.where.h, r.where.y, r.where.w - r.where.h, r.where.h},
                                std::to_string(m.cells), kMuted, 11 * s, false, Align::Right});
    }
    return out;
}

}  // namespace uagfx::partypanel
