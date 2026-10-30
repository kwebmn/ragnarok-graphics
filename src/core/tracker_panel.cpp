#include "core/tracker_panel.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::tracker {
namespace {

// At scale 1: the panel 264 wide, its header 34, an entry's head 24 and each line 19 under it, 6 between entries
constexpr float kW = 264, kHeader = 34, kPad = 8, kHead = 24, kLine = 19, kGap = 6, kInner = 6, kMore = 26;
constexpr float kButton = 24, kClose = 20;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xE6CB8A;
constexpr uint32_t kText = 0xECEFF5, kMuted = 0x9AA3B5, kFaint = 0x7D8597;

float entryHeight(const Entry& e, bool folded, size_t lines) {
    return kInner + kHead + (folded ? 0.0f : float(std::min(lines, e.lines.size())) * kLine) + kInner;
}

std::string counted(const std::string& text, int n) {
    std::string out = text;
    const size_t at = out.find("{1}");
    if (at != std::string::npos) out.replace(at, 3, std::to_string(n));
    return out;
}

}  // namespace

Layout layout(const std::vector<Entry>& entries, bool folded, int right, int top, int bottom, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    L.w = int(std::lround(kW * s));
    const float room = float(bottom - top) / s;  // at scale 1
    float y = kHeader;
    size_t shown = 0;
    std::vector<size_t> lines;  // each shown entry's lines
    for (size_t i = 0; i < entries.size(); ++i) {
        const float need = kGap + entryHeight(entries[i], folded, entries[i].lines.size());
        if (shown > 0 && y + need + kPad > room) break;  // the rest under «ще N» (room made for it below)
        size_t keep = folded ? 0 : entries[i].lines.size();
        if (shown == 0)  // the first one shown whole as it fits (its head at the least)
            while (keep > 0 && y + kGap + entryHeight(entries[i], folded, keep) + kPad > room) --keep;
        lines.push_back(keep);
        y += kGap + entryHeight(entries[i], folded, keep);
        ++shown;
    }
    L.hidden = int(entries.size() - shown);
    if (L.hidden > 0 && y + kGap + kMore + kPad > room && shown > 1) {  // room for «ще N»: one entry fewer
        y -= kGap + entryHeight(entries[shown - 1], folded, lines.back());
        lines.pop_back();
        --shown;
        ++L.hidden;
    }
    const float height = y + (L.hidden > 0 ? kGap + kMore : 0.0f) + kPad;
    L.h = int(std::ceil(height * s));
    L.x = right - L.w;
    L.y = top;
    L.glass = panel::Box{0, 0, float(L.w), float(L.h)};
    L.hide = panel::Box{float(L.w) - (kPad + kButton) * s, (kHeader - kButton) / 2 * s, kButton * s, kButton * s};
    L.fold = panel::Box{L.hide.x - (4 + kButton) * s, L.hide.y, kButton * s, kButton * s};
    L.title = panel::Box{12 * s, 0, L.fold.x - 16 * s, kHeader * s};
    float at = kHeader * s;
    for (size_t i = 0; i < shown; ++i) {
        Placed p;
        at += kGap * s;
        const float h = entryHeight(entries[i], folded, lines[i]) * s;
        p.box = panel::Box{kPad * s, at, float(L.w) - 2 * kPad * s, h};
        p.head = panel::Box{p.box.x + 10 * s, at + kInner * s, p.box.w - 20 * s, kHead * s};
        p.close = panel::Box{p.box.x + p.box.w - (6 + kClose) * s, at + (kInner + (kHead - kClose) / 2) * s,
                             kClose * s, kClose * s};
        for (size_t k = 0; k < lines[i]; ++k)
            p.lines.push_back(panel::Box{p.head.x, p.head.y + p.head.h + float(k) * kLine * s, p.head.w, kLine * s});
        L.entries.push_back(std::move(p));
        at += h;
    }
    if (L.hidden > 0) L.more = panel::Box{kPad * s, at + kGap * s, float(L.w) - 2 * kPad * s, kMore * s};
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (!L.glass.contains(x, y)) return Hit{};
    if (L.fold.contains(x, y)) return Hit{Part::Fold, -1};
    if (L.hide.contains(x, y)) return Hit{Part::Hide, -1};
    for (size_t i = 0; i < L.entries.size(); ++i) {
        const Placed& p = L.entries[i];
        if (p.close.contains(x, y)) return Hit{Part::Close, int(i)};
        if (p.head.contains(x, y)) return Hit{Part::Head, int(i)};
    }
    return Hit{Part::Panel, -1};
}

void paint(panel::Picture& out, const Layout& L, const std::vector<Entry>& entries, const Hit& hover) {
    (void)entries;
    const float s = L.s;
    panel::roundRect(out, L.glass, 10 * s, kCard, uistyle::panel(), kWhite, 0.09f, 1.0f * s);
    panel::roundRect(out, panel::Box{0, kHeader * s - 1.0f * s, float(L.w), 1.0f * s}, 0, kWhite, 0.08f);
    for (const panel::Box* b : {&L.fold, &L.hide})
        if ((b == &L.fold && hover.part == Part::Fold) || (b == &L.hide && hover.part == Part::Hide))
            panel::roundRect(out, *b, 6 * s, kWhite, 0.12f);
    for (size_t i = 0; i < L.entries.size(); ++i) {
        const Placed& p = L.entries[i];
        const bool over = hover.entry == int(i) && (hover.part == Part::Head || hover.part == Part::Close);
        panel::roundRect(out, p.box, 8 * s, kWhite, over ? 0.075f : 0.04f);
        if (hover.part == Part::Close && hover.entry == int(i)) panel::roundRect(out, p.close, 6 * s, kWhite, 0.12f);
    }
    if (L.more.w > 0) panel::roundRect(out, L.more, 7 * s, kWhite, 0.06f);
}

std::vector<Words> words(const Layout& L, const std::vector<Entry>& entries, const Texts& t, bool folded) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, t.title, kText, 13.5f * s, true, Align::Left});
    out.push_back(Words{L.fold, folded ? "\xE2\x96\xB8" : "\xE2\x96\xBE", kMuted, 13 * s, false, Align::Center});
    out.push_back(Words{L.hide, "\xE2\x9C\x95", kMuted, 12 * s, false, Align::Center});
    for (size_t i = 0; i < L.entries.size() && i < entries.size(); ++i) {
        const Placed& p = L.entries[i];
        const Entry& e = entries[i];
        const float tagW = e.tag.empty() ? 0 : std::ceil(panel::wordsWidth(e.tag, 11.5f * s, false));
        const float tagX = p.close.x - 6 * s - tagW;
        out.push_back(Words{panel::Box{p.head.x, p.head.y, std::max(0.0f, tagX - 8 * s - p.head.x), p.head.h}, e.title,
                            kText, 13.5f * s, true, Align::Left});
        if (!e.tag.empty())
            out.push_back(Words{panel::Box{tagX, p.head.y, tagW, p.head.h}, e.tag, kMuted, 11.5f * s, false,
                                Align::Right});
        out.push_back(Words{p.close, "\xE2\x9C\x95", kFaint, 11 * s, false, Align::Center});
        for (size_t k = 0; k < p.lines.size() && k < e.lines.size(); ++k) {
            const Line& ln = e.lines[k];
            const panel::Box& b = p.lines[k];
            const float rightW = ln.right.empty() ? 0 : std::ceil(panel::wordsWidth(ln.right, 12.5f * s, true));
            out.push_back(Words{panel::Box{b.x, b.y, std::max(0.0f, b.w - rightW - (rightW > 0 ? 8 * s : 0)), b.h},
                                ln.text, ln.strong ? kGold : kText, 12.5f * s, ln.strong, Align::Left});
            if (!ln.right.empty())
                out.push_back(Words{panel::Box{b.x + b.w - rightW, b.y, rightW, b.h}, ln.right, kMuted, 12.5f * s,
                                    true, Align::Right});
        }
    }
    if (L.more.w > 0) out.push_back(Words{L.more, counted(t.more, L.hidden), kMuted, 12 * s, true, Align::Center});
    return out;
}

}  // namespace uagfx::tracker
