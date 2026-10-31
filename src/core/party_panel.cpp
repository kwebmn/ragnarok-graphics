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

// 3.0.11-beta.80: a premultiplied picture inside a circle (its middle at the box's; `square`: the whole picture), over
// what is there
void putRound(panel::Picture& out, const panel::Picture& pic, const panel::Box& box, float opacity,
              bool square = false) {
    const float cx = box.x + box.w / 2, cy = box.y + box.h / 2, r = box.w / 2;
    const int ox = int(panel::roundOf(cx - float(pic.width) / 2)), oy = int(panel::roundOf(cy - float(pic.height) / 2));
    for (int y = 0; y < pic.height; ++y)
        for (int x = 0; x < pic.width; ++x) {
            const int tx = ox + x, ty = oy + y;
            if (tx < 0 || ty < 0 || tx >= out.width || ty >= out.height) continue;
            const float k = (square ? 1.0f
                                    : std::clamp(r + 0.5f - panel::length(float(tx) + 0.5f - cx, float(ty) + 0.5f - cy),
                                                 0.0f, 1.0f)) *
                            opacity;
            uint32_t c = pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
            if (k <= 0 || !(c >> 24)) continue;
            if (k < 1) {
                const auto ch = [&](int sh) { return uint32_t(float((c >> sh) & 0xFF) * k + 0.5f); };
                c = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
            }
            uint32_t& d = out.px[size_t(ty) * size_t(out.width) + size_t(tx)];
            const uint32_t keep = 255 - (c >> 24);
            uint32_t mixed = 0;
            for (int sh = 0; sh <= 24; sh += 8)
                mixed |= std::min(255u, ((c >> sh) & 0xFF) + ((d >> sh) & 0xFF) * keep / 255) << sh;
            d = mixed;
        }
}

}  // namespace

bool low(const Member& m, int threshold) { return here(m) && m.percent >= 0 && m.percent < threshold; }

Layout layout(size_t members, int left, int top, float scale, bool folded) {
    return layout(std::vector<int>(members, 0), left, top, scale, folded);
}

std::vector<int> rowsOf(const std::vector<Member>& members) {
    std::vector<int> out;
    for (const Member& m : members) out.push_back(int(m.buffs.size()));
    return out;
}

Layout layout(const std::vector<int>& buffs, int left, int top, float scale, bool folded) {
    const size_t members = buffs.size();
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    L.x = left;
    L.y = top;
    L.folded = folded;
    if (!members) return L;
    const float w = kW * s, pad = kPad * s;
    L.header = panel::Box{pad, pad * 0.5f, w - 2 * pad, kHeaderH * s};  // 3.0.11-beta.79
    L.fold = panel::Box{L.header.x + L.header.w - 24 * s, L.header.y + 3 * s, 22 * s, L.header.h - 6 * s};
    L.count = panel::Box{L.fold.x - 52 * s, L.header.y, 48 * s, L.header.h};
    L.title = panel::Box{L.header.x + 4 * s, L.header.y, L.count.x - L.header.x - 8 * s, L.header.h};
    const float top0 = L.header.y + L.header.h;
    if (folded) {
        L.glass = panel::Box{0, 0, w, top0 + pad * 0.5f};
        L.w = int(std::ceil(L.glass.w));
        L.h = int(std::ceil(L.glass.h));
        return L;
    }
    float y = top0;
    for (size_t i = 0; i < members; ++i) {
        Row r;
        const bool line = buffs[i] > 0;  // 3.0.11-beta.80: its buffs a line of their own under its bar
        r.box = panel::Box{pad, y, w - 2 * pad, (kRowH + (line ? kBuffLine : 0)) * s};
        y += r.box.h;
        r.round = panel::Box{r.box.x + 2 * s, r.box.y + 6 * s, kRound * s, kRound * s};
        const float x0 = r.round.x + r.round.w + 8 * s;
        r.value = panel::Box{r.box.x + r.box.w - kValueW * s - 4 * s, r.box.y + 5 * s, kValueW * s, 18 * s};
        r.name = panel::Box{x0, r.box.y + 5 * s, r.value.x - 4 * s - x0, 18 * s};
        r.bar = panel::Box{x0, r.box.y + 27 * s, r.box.x + r.box.w - 4 * s - x0, kBarH * s};
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
    if (L.header.contains(x, y)) return Hit{Part::Header, -1};
    for (size_t i = 0; i < L.rows.size(); ++i)
        if (L.rows[i].box.contains(x, y)) return Hit{Part::Row, int(i)};
    return Hit{Part::Header, -1};  // its glass between: the panel's
}

void paint(panel::Picture& out, const Layout& L, const std::vector<Member>& members, int threshold) {
    const float s = L.s;
    if (L.header.w <= 0) return;
    panel::roundRect(out, L.glass, 10 * s, kCard, uistyle::panel(), kWhite, 0.09f, 1.0f * s);
    if (!L.rows.empty())  // 3.0.11-beta.79: the header's line under it
        panel::roundRect(out, panel::Box{L.header.x, L.header.y + L.header.h - 1 * s, L.header.w, 1 * s}, 0, kWhite,
                         0.10f);
    for (size_t i = 0; i < L.rows.size() && i < members.size(); ++i) {
        const Row& r = L.rows[i];
        const Member& m = members[i];
        const bool bad = low(m, threshold);
        if (m.dead)  // 3.0.11-beta.81: dead - its row dark red
            panel::roundRect(out, r.box, 7 * s, kDeadGround, 0.55f, kRed, 0.6f, 1.0f * s);
        else if (bad)
            panel::roundRect(out, r.box, 7 * s, kRed, 0.10f, kRed, 0.85f, 1.2f * s);  // the row edged red
        panel::circle(out, r.round.x + r.round.w / 2, r.round.y + r.round.h / 2, r.round.w / 2, kTile, 1.0f);
        if (m.face && m.face->width > 0)  // 3.0.11-beta.80: its face, faint when it is not here (or dead)
            putRound(out, *m.face, r.round, here(m) && !m.dead ? 1.0f : 0.45f);
        if (m.leader) {  // the crown after its name
            const float nameW = panel::wordsWidth((bad ? "! " : "") + m.name, 13 * s, true);
            const float icon = 13 * s, x = std::min(r.name.x + nameW + 4 * s, r.name.x + r.name.w - icon);
            uiicons::draw(out, uiicons::Icon::Mvp, x, r.name.y + (r.name.h - icon) / 2, icon, kGold, 1.0f, 2.2f);
        }
        for (size_t b = 0; b < r.buffs.size() && b < m.buffs.size(); ++b) {  // 3.0.11-beta.80: its buffs
            const panel::Box& box = r.buffs[b];
            const Buff& f = m.buffs[b];
            if (f.glow > 0)  // 3.0.11-beta.81: one's own about to end - a gold edge, pulsing
                panel::roundRect(out, panel::Box{box.x - 2 * s, box.y - 2 * s, box.w + 4 * s, box.h + 4 * s}, 3 * s,
                                 kGold, 0.25f * f.glow, kGold, std::min(1.0f, f.glow), 1.5f * s);
            if (f.icon && f.icon->width > 0) putRound(out, *f.icon, box, 1.0f, true);
            if (f.left >= 0) {  // its time left: a thin line under it, red at its last tenth
                const float part = std::clamp(f.left, 0.0f, 1.0f);
                panel::roundRect(out, panel::Box{box.x, box.y + box.h + 1 * s, box.w, 2 * s}, 0, kBlack, 0.5f);
                panel::roundRect(out, panel::Box{box.x, box.y + box.h + 1 * s, std::max(1.0f, box.w * part), 2 * s},
                                 0, part < 0.1f ? kRed : kGold, 1.0f);
            }
        }
        if (m.castPart >= 0 && here(m)) {  // 3.0.11-beta.81: its cast - a line under its bar
            const panel::Box line{r.bar.x, r.bar.y + r.bar.h + 1.5f * s, r.bar.w, 2.5f * s};
            panel::roundRect(out, line, line.h / 2, kBlack, 0.45f);
            const float done = std::max(line.h, line.w * std::clamp(m.castPart, 0.0f, 1.0f));
            panel::roundRect(out, panel::Box{line.x, line.y, done, line.h}, line.h / 2, kCast, 1.0f);
        }
        if (!here(m) || m.dead) continue;  // elsewhere, offline, dead: no bar
        panel::roundRect(out, r.bar, r.bar.h / 2, kBlack, 0.45f);
        if (m.percent > 0) {
            const float part = std::clamp(float(m.percent) / 100.0f, 0.0f, 1.0f);
            panel::roundRect(out, panel::Box{r.bar.x, r.bar.y, std::max(r.bar.h, r.bar.w * part), r.bar.h}, r.bar.h / 2,
                             bad ? kRed : kHp, 1.0f);
        }
    }
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
    }
    for (size_t i = 0; i < L.rows.size() && i < members.size(); ++i) {
        const Row& r = L.rows[i];
        const Member& m = members[i];
        const bool bad = low(m, threshold);
        const bool wide = !m.name.empty() && (static_cast<unsigned char>(m.name[0]) & 0xE0) == 0xC0;  // UTF-8 of 2
        const std::string first = m.name.empty() ? "?" : m.name.substr(0, wide ? 2 : 1);
        if (!m.face || m.face->width <= 0) out.push_back(Words{r.round, first, kMuted, 10.5f * s, true, Align::Center});
        const std::string shown = bad ? "! " + m.name : m.name;
        out.push_back(Words{r.name, shown, bad || m.dead ? kRedText : here(m) ? kText : kMuted, 13 * s, true,
                            Align::Left});
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
    }
    return out;
}

}  // namespace uagfx::partypanel
