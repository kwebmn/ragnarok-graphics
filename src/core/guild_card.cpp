#include "core/guild_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/client_libs.hpp"
#include "core/pack_source.hpp"
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

Layout layout(int targetW, int targetH, float scale, Tab tab, size_t members, bool editing) {
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
    if (tab == Tab::Info) {  // 3.0.19-beta.1: its own head - the emblem, the name beside it, level and members under
        L.emblem = panel::Box{pad, y, 48 * s, 48 * s};
        L.head = panel::Box{pad + 60 * s, y + 4 * s, inner - 60 * s, 22 * s};
        L.sub = panel::Box{pad + 60 * s, y + 26 * s, inner - 60 * s, 18 * s};
        y += 56 * s;
    } else {
        L.head = panel::Box{pad, y, inner, 20 * s};
        y += 24 * s;
    }
    if (tab == Tab::Info) {
        for (int i = 0; i < 7; ++i) {
            L.facts[i] = panel::Box{pad, y, inner, kFactH * s};
            y += kFactH * s;
        }
        L.list = panel::Box{pad, L.facts[0].y, inner, y - L.facts[0].y};
    } else if (tab == Tab::Notice) {  // 3.0.11-beta.220: its subject and body
        L.notice = panel::Box{pad, y, inner, 7 * kRowH * s};
        L.list = L.notice;
        if (editing) {  // .271: «Тема» over its field, «Текст» over its taller one
            L.fields[0] = panel::Box{pad + 8 * s, y + 24 * s, inner - 16 * s, 30 * s};
            L.fields[1] = panel::Box{pad + 8 * s, y + 24 * s + 30 * s + 28 * s, inner - 16 * s,
                                     L.notice.h - (24 + 30 + 28 + 8) * s};
        }
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
        if (editing && tab == Tab::Notice) L.cancel = panel::Box{L.edit.x - 10 * s - 120 * s, y, 120 * s, 28 * s};
        y += 28 * s;
    }
    y += pad;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.wide = L.w;
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
    if (L.cancel.w > 0 && L.cancel.contains(x, y)) return Hit{Part::Cancel, -1};  // .271
    for (int i = 0; i < 2; ++i)
        if (L.fields[i].w > 0 && L.fields[i].contains(x, y)) return Hit{Part::Field, i};
    if (L.glass.contains(x, y)) return Hit{Part::Body, -1};
    return Hit{};
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
    if (sh.tab == Tab::Info) {  // 3.0.19-beta.1: the guild's name in gold, its level and members under it
        out.push_back(Words{L.head, g.name.empty() ? t.title : g.name, kGold2, 15 * s, true, Align::Left});
        out.push_back(Words{L.sub,
                            t.facts[1] + ' ' + std::to_string(g.level) + " \xC2\xB7 " + t.facts[2] + ' ' +
                                ofMost(g.members, g.most),
                            kMuted, 12 * s, false, Align::Left});
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
    if (L.edit.w > 0 && sh.canEdit)
        out.push_back(Words{L.edit, sh.editing && sh.tab == Tab::Notice ? t.save : t.edit, kText, 12.5f * s, true,
                            Align::Center});
    if (sh.tab == Tab::Notice && sh.editing) {  // .271: the notice's fields with their caret
        out.push_back(Words{L.head, t.editHint, kMuted, 11.5f * s, false, Align::Left});
        const std::string caret = sh.caret ? "|" : "";
        for (int i = 0; i < 2; ++i) {
            const panel::Box& f = L.fields[i];
            out.push_back(Words{panel::Box{f.x, f.y - 22 * s, f.w, 20 * s}, i ? t.fieldBody : t.fieldTitle, kMuted,
                                12 * s, false, Align::Left});
            out.push_back(Words{panel::Box{f.x + 8 * s, f.y + 4 * s, f.w - 16 * s, f.h - 8 * s},
                                (i ? sh.body : sh.subject) + (sh.field == i ? caret : ""), kText, 13 * s, i == 0,
                                Align::Left});
        }
        if (L.cancel.w > 0) out.push_back(Words{L.cancel, t.cancel, kText, 12.5f * s, false, Align::Center});
        return out;
    }
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

std::vector<uint32_t> emblemPixels(const std::string& file, int side) {
    if (side <= 0 || file.size() < 2) return {};
    std::string bmp;
    if (file[0] == 'B' && file[1] == 'M') {
        bmp = file;
    } else {  // packed: an emblem's BMP is a few KB (24 x 24 at 24 bits 1782 bytes), at most 64 x 64 at 32
        std::vector<unsigned char> out(64 * 1024);
        unsigned long len = static_cast<unsigned long>(out.size());
        if (client_libs::zlibUncompress(out.data(), &len, reinterpret_cast<const unsigned char*>(file.data()),
                                        static_cast<unsigned long>(file.size())) != 0)
            return {};
        bmp.assign(reinterpret_cast<const char*>(out.data()), len);
    }
    std::vector<uint32_t> argb;
    int w = 0, h = 0;
    if (!pack::decodeBmp(reinterpret_cast<const uint8_t*>(bmp.data()), bmp.size(), argb, w, h) || w <= 0 || h <= 0 ||
        w > 256 || h > 256)
        return {};
    std::vector<uint32_t> px(size_t(side) * size_t(side), 0u);
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x) {
            const uint32_t c = argb[size_t(y * h / side) * size_t(w) + size_t(x * w / side)];
            const uint32_t r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
            px[size_t(y) * size_t(side) + size_t(x)] = r >= 0xF8 && g <= 0x08 && b >= 0xF8 ? 0u : (c | 0xFF000000u);
        }
    return px;
}

}  // namespace uagfx::guildcard
