#include "core/guild_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::guildcard {
namespace {

// At scale 1: the card 380 wide, 16 in; the title 28; the tabs 30; a fact 22; a member's row 26 (12 shown at most)
constexpr float kW = 380, kPad = 16, kTitleH = 28, kTabH = 30, kFactH = 22, kRowH = 26, kGap = 6;
constexpr size_t kMostRows = 12;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kOnline = 0x4CC777, kOffline = 0x5C6370;

std::string ofMost(long long a, long long b) { return std::to_string(a) + " / " + std::to_string(b); }

constexpr uint32_t kTabRights[kTabs] = {0, 0x01, 0x02, 0, 0x10, 0x80};  // FUN_0049c4d0's
constexpr uint32_t kDim = 0x5E6575;

}  // namespace

bool gameDrawn(int tab, bool editing) { return editing && (tab == int(Tab::Positions) || tab == int(Tab::Notice)); }

bool allowed(int tab, uint32_t rights) {
    if (tab < 0 || tab >= kTabs) return false;
    return kTabRights[tab] == 0 || (rights & kTabRights[tab]) != 0;
}

Layout layout(int targetW, int targetH, float scale, Tab tab, size_t members) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s, inner = w - 2 * pad;
    L.title = panel::Box{pad, pad, inner - 28 * s, kTitleH * s};
    L.close = panel::Box{w - pad - 24 * s, pad + 2 * s, 24 * s, 24 * s};
    float y = pad + kTitleH * s + 4 * s;
    const float tw = (inner - 2 * kGap * s) / 3;  // 3.0.11-beta.210: two rows of three
    for (int i = 0; i < kTabs; ++i)
        L.tabs[i] = panel::Box{pad + float(i % 3) * (tw + kGap * s), y + float(i / 3) * (kTabH + kGap) * s, tw,
                               kTabH * s};
    y += 2 * kTabH * s + kGap * s + 8 * s;
    L.head = panel::Box{pad, y, inner, 20 * s};
    y += 24 * s;
    if (tab == Tab::Info) {
        for (int i = 0; i < 7; ++i) {
            L.facts[i] = panel::Box{pad, y, inner, kFactH * s};
            y += kFactH * s;
        }
        L.list = panel::Box{pad, L.facts[0].y, inner, y - L.facts[0].y};
    } else if (tab == Tab::Notice) {  // 3.0.11-beta.220: its subject and body
        L.notice = panel::Box{pad, y, inner, 7 * kRowH * s};
        L.list = L.notice;
        y += L.notice.h;
    } else {
        const size_t shown = std::min(members, kMostRows);
        L.list = panel::Box{pad, y, inner, kRowH * s * float(std::max<size_t>(shown, 3))};
        for (size_t i = 0; i < shown; ++i) {
            L.rows.push_back(panel::Box{pad, y + float(i) * kRowH * s, inner, kRowH * s - 2 * s});
            if (tab == Tab::Skills)
                L.plus.push_back(panel::Box{pad + inner - 26 * s, y + float(i) * kRowH * s + 2 * s, 22 * s,
                                            kRowH * s - 6 * s});
        }
        y += L.list.h;
    }
    if (tab == Tab::Positions || tab == Tab::Notice) {  // 3.0.11-beta.220: «Змінити» - the game's own window
        y += 8 * s;
        L.edit = panel::Box{pad + inner - 140 * s, y, 140 * s, 28 * s};
        y += 28 * s;
    }
    y += pad;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::clamp(int(float(targetW) * 0.64f), 0, std::max(0, targetW - L.w));
    L.y = std::clamp(int(float(targetH) * 0.16f), 0, std::max(0, targetH - L.h));
    return L;
}

int mostScroll(const Layout& L, size_t members) { return std::max(0, int(members) - int(L.rows.size())); }

Hit hitTest(const Layout& L, float x, float y) {
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    for (int i = 0; i < kTabs; ++i)
        if (L.tabs[i].contains(x, y)) return Hit{Part::Tab, i};
    for (size_t i = 0; i < L.plus.size(); ++i)
        if (L.plus[i].contains(x, y)) return Hit{Part::Plus, int(i)};
    if (L.edit.w > 0 && L.edit.contains(x, y)) return Hit{Part::Edit, -1};
    if (L.glass.contains(x, y)) return Hit{Part::Body, -1};
    return Hit{};
}

void paint(panel::Picture& out, const Layout& L, const std::vector<Member>& members, const Show& sh) {
    paint(out, L, members, More{}, sh);
}

void paint(panel::Picture& out, const Layout& L, const std::vector<Member>& members, const More& more,
           const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, std::max(0.94f, uistyle::window()), kWhite, 0.12f, 1.0f * s);
    panel::roundRect(out, L.close, 6 * s, kWhite, sh.hoverClose ? 0.14f : 0.06f);
    for (int i = 0; i < kTabs; ++i) {
        const bool on = int(sh.tab) == i, hot = sh.hoverTab == i && allowed(i, sh.rights);
        panel::roundRect(out, L.tabs[i], 8 * s, on ? kGold : kWhite, on ? 0.24f : hot ? 0.10f : 0.04f,
                         on ? kGold : kWhite, on ? 0.8f : 0.12f, 1.0f * s);
    }
    panel::roundRect(out, L.list, 8 * s, 0x000000, 0.16f);
    if (L.edit.w > 0 && sh.canEdit)
        panel::roundRect(out, L.edit, 8 * s, kGold, sh.hoverEdit ? 0.34f : 0.22f, kGold, 0.8f, 1.0f * s);
    if (sh.tab != Tab::Members) {  // 3.0.11-beta.220: tabs 2-5 - a skill's «+» where it can rise
        for (size_t i = 0; i < L.plus.size(); ++i) {
            const size_t at = i + size_t(std::max(0, sh.scroll));
            if (at < more.lines.size() && more.lines[at].plus)
                panel::roundRect(out, L.plus[i], 6 * s, kGold, sh.hoverPlus == int(i) ? 0.45f : 0.28f, kGold, 0.85f,
                                 1.0f * s);
        }
        return;
    }
    for (size_t i = 0; i < L.rows.size(); ++i) {
        const size_t at = i + size_t(std::max(0, sh.scroll));
        if (at >= members.size()) break;
        const float d = 8 * s;
        panel::roundRect(out, panel::Box{L.rows[i].x + 8 * s, L.rows[i].y + (L.rows[i].h - d) / 2, d, d}, d / 2,
                         members[at].online ? kOnline : kOffline, 0.95f);
    }
}

std::vector<Words> words(const Layout& L, const Info& g, const std::vector<Member>& members, const Show& sh,
                         const Texts& t) {
    return words(L, g, members, More{}, sh, t);
}

std::vector<Words> words(const Layout& L, const Info& g, const std::vector<Member>& members, const More& more,
                         const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, g.name.empty() ? t.title : g.name, kGold2, 15 * s, true, Align::Left});
    out.push_back(Words{L.close, "\xC3\x97", kText, 14 * s, false, Align::Center});  // ×
    for (int i = 0; i < kTabs; ++i)
        out.push_back(Words{L.tabs[i], t.tabs[i], allowed(i, sh.rights) ? kText : kDim, 12.5f * s, int(sh.tab) == i,
                            Align::Center});
    if (sh.tab == Tab::Info) {
        out.push_back(Words{L.head, t.title, kMuted, 12 * s, false, Align::Left});
        const std::string values[7] = {g.master,
                                       std::to_string(g.level),
                                       ofMost(g.members, g.most),
                                       std::to_string(g.average),
                                       ofMost(g.exp, g.nextExp),
                                       std::to_string(g.tax),
                                       g.territory.empty() ? t.none : g.territory};
        for (int i = 0; i < 7; ++i) {
            const panel::Box b{L.facts[i].x + 10 * s, L.facts[i].y, L.facts[i].w - 20 * s, L.facts[i].h};
            out.push_back(Words{b, t.facts[i], kMuted, 12 * s, false, Align::Left});
            out.push_back(Words{b, values[i], kText, 12.5f * s, i == 0, Align::Right});
        }
        return out;
    }
    if (L.edit.w > 0 && sh.canEdit) out.push_back(Words{L.edit, t.edit, kText, 12.5f * s, true, Align::Center});
    if (sh.tab == Tab::Notice) {  // 3.0.11-beta.220
        const panel::Box& n = L.notice;
        out.push_back(Words{L.head, t.tabs[int(Tab::Notice)], kMuted, 12 * s, false, Align::Left});
        out.push_back(Words{panel::Box{n.x + 10 * s, n.y + 8 * s, n.w - 20 * s, 22 * s},
                            more.subject.empty() && more.body.empty() ? t.none : more.subject, kGold2, 13.5f * s, true,
                            Align::Left});
        out.push_back(Words{panel::Box{n.x + 10 * s, n.y + 36 * s, n.w - 20 * s, n.h - 44 * s}, more.body, kText,
                            12.5f * s, false, Align::Left});
        return out;
    }
    if (sh.tab != Tab::Members) {  // 3.0.11-beta.220: the positions, the skills, the expelled
        out.push_back(Words{L.head, more.head, kMuted, 12 * s, false, Align::Left});
        if (more.lines.empty() && !L.list.h) return out;
        if (more.lines.empty())
            out.push_back(Words{panel::Box{L.list.x + 10 * s, L.list.y, L.list.w, kRowH * s}, t.none, kMuted, 12 * s,
                                false, Align::Left});
        for (size_t i = 0; i < L.rows.size(); ++i) {
            const size_t at = i + size_t(std::max(0, sh.scroll));
            if (at >= more.lines.size()) break;
            const Line& n = more.lines[at];
            const panel::Box& r = L.rows[i];
            const float right = i < L.plus.size() ? 32 * s : 10 * s;
            out.push_back(Words{panel::Box{r.x + 10 * s, r.y, r.w * 0.40f, r.h}, n.left, kText, 12.5f * s, false,
                                Align::Left});
            out.push_back(Words{panel::Box{r.x + r.w * 0.42f, r.y, r.w * 0.38f, r.h}, n.middle, kMuted, 11.5f * s,
                                false, Align::Left});
            out.push_back(Words{panel::Box{r.x + r.w * 0.78f, r.y, r.w * 0.22f - right, r.h}, n.right, kGold2,
                                11.5f * s, false, Align::Right});
            if (n.plus && i < L.plus.size())
                out.push_back(Words{L.plus[i], "+", kText, 13 * s, true, Align::Center});
        }
        return out;
    }
    const int online = int(std::count_if(members.begin(), members.end(), [](const Member& m) { return m.online; }));
    std::string head = t.online;
    if (const size_t p = head.find("{1}"); p != std::string::npos) head.replace(p, 3, std::to_string(online));
    if (const size_t p = head.find("{2}"); p != std::string::npos) head.replace(p, 3, std::to_string(members.size()));
    out.push_back(Words{L.head, head, kMuted, 12 * s, false, Align::Left});
    for (size_t i = 0; i < L.rows.size(); ++i) {
        const size_t at = i + size_t(std::max(0, sh.scroll));
        if (at >= members.size()) break;
        const Member& m = members[at];
        const panel::Box& r = L.rows[i];
        out.push_back(Words{panel::Box{r.x + 22 * s, r.y, r.w * 0.36f, r.h}, m.name, m.online ? kText : kMuted,
                            12.5f * s, false, Align::Left});
        out.push_back(Words{panel::Box{r.x + r.w * 0.40f, r.y, r.w * 0.24f, r.h}, m.title, kGold2, 11.5f * s, false,
                            Align::Left});
        out.push_back(Words{panel::Box{r.x + r.w * 0.64f, r.y, r.w * 0.34f, r.h},
                            m.job + " " + std::to_string(m.level), kMuted, 11.5f * s, false, Align::Right});
    }
    return out;
}

}  // namespace uagfx::guildcard
