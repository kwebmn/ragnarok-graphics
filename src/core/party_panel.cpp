#include "core/party_panel.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_icons.hpp"

namespace uagfx::partypanel {
namespace {

// The concept's sizes at scale 1 (v19_cave): the panel 252 wide, 8 in, a row 40 (its round 20, its name 13, its bar 5)
constexpr float kW = 252, kPad = 8, kRowH = 40, kRound = 20, kBarH = 5, kValueW = 76;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kTile = 0x1B1D24;
constexpr uint32_t kGold = 0xE6CB8A, kText = 0xECEFF5, kMuted = 0x9AA3B5, kHp = 0x5BD36C, kRed = 0xFF5A4E;
constexpr uint32_t kRedText = 0xFF9A8F;

bool here(const Member& m) { return m.online && m.where.empty(); }

}  // namespace

bool low(const Member& m, int threshold) { return here(m) && m.percent >= 0 && m.percent < threshold; }

Layout layout(size_t members, int left, int top, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    L.x = left;
    L.y = top;
    if (!members) return L;
    const float w = kW * s, pad = kPad * s;
    for (size_t i = 0; i < members; ++i) {
        Row r;
        r.box = panel::Box{pad, pad + float(i) * kRowH * s, w - 2 * pad, kRowH * s};
        r.round = panel::Box{r.box.x + 2 * s, r.box.y + 6 * s, kRound * s, kRound * s};
        const float x0 = r.round.x + r.round.w + 8 * s;
        r.value = panel::Box{r.box.x + r.box.w - kValueW * s - 4 * s, r.box.y + 5 * s, kValueW * s, 18 * s};
        r.name = panel::Box{x0, r.box.y + 5 * s, r.value.x - 4 * s - x0, 18 * s};
        r.bar = panel::Box{x0, r.box.y + 27 * s, r.box.x + r.box.w - 4 * s - x0, kBarH * s};
        L.rows.push_back(r);
    }
    L.glass = panel::Box{0, 0, w, 2 * pad + float(members) * kRowH * s};
    L.w = int(std::ceil(L.glass.w));
    L.h = int(std::ceil(L.glass.h));
    return L;
}

void paint(panel::Picture& out, const Layout& L, const std::vector<Member>& members, int threshold) {
    const float s = L.s;
    if (L.rows.empty()) return;
    panel::roundRect(out, L.glass, 10 * s, kCard, 0.86f, kWhite, 0.09f, 1.0f * s);
    for (size_t i = 0; i < L.rows.size() && i < members.size(); ++i) {
        const Row& r = L.rows[i];
        const Member& m = members[i];
        const bool bad = low(m, threshold);
        if (bad) panel::roundRect(out, r.box, 7 * s, kRed, 0.10f, kRed, 0.85f, 1.2f * s);  // the row edged red
        panel::circle(out, r.round.x + r.round.w / 2, r.round.y + r.round.h / 2, r.round.w / 2, kTile, 1.0f);
        if (m.leader) {  // the crown after its name
            const float nameW = panel::wordsWidth((bad ? "! " : "") + m.name, 13 * s, true);
            const float icon = 13 * s, x = std::min(r.name.x + nameW + 4 * s, r.name.x + r.name.w - icon);
            uiicons::draw(out, uiicons::Icon::Mvp, x, r.name.y + (r.name.h - icon) / 2, icon, kGold, 1.0f, 2.2f);
        }
        if (!here(m)) continue;  // elsewhere, offline: no bar
        panel::roundRect(out, r.bar, r.bar.h / 2, kBlack, 0.45f);
        if (m.percent > 0) {
            const float part = std::clamp(float(m.percent) / 100.0f, 0.0f, 1.0f);
            panel::roundRect(out, panel::Box{r.bar.x, r.bar.y, std::max(r.bar.h, r.bar.w * part), r.bar.h}, r.bar.h / 2,
                             bad ? kRed : kHp, 1.0f);
        }
    }
}

std::vector<Words> words(const Layout& L, const std::vector<Member>& members, int threshold,
                         const std::string& offline) {
    std::vector<Words> out;
    const float s = L.s;
    for (size_t i = 0; i < L.rows.size() && i < members.size(); ++i) {
        const Row& r = L.rows[i];
        const Member& m = members[i];
        const bool bad = low(m, threshold);
        const bool wide = !m.name.empty() && (static_cast<unsigned char>(m.name[0]) & 0xE0) == 0xC0;  // UTF-8 of 2
        const std::string first = m.name.empty() ? "?" : m.name.substr(0, wide ? 2 : 1);
        out.push_back(Words{r.round, first, kMuted, 10.5f * s, true, Align::Center});
        out.push_back(Words{r.name, bad ? "! " + m.name : m.name, bad ? kRedText : here(m) ? kText : kMuted, 13 * s,
                            true, Align::Left});
        std::string value;
        uint32_t rgb = kMuted;
        if (!m.online) value = offline;
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
