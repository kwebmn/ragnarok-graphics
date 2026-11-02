#include "core/login_chrome.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace uagfx::loginchrome {
namespace {

// At scale 1: the strip 80 high; the pill 32 high, 20 over the edge, 16 in, its dot 9 and 10 to the words
constexpr float kStripH = 80, kPillH = 32, kEdge = 20, kPad = 16, kDot = 9, kDotGap = 10, kPx = 14;
constexpr uint32_t kPill = 0x0A0C12, kWhite = 0xFFFFFF, kWords = 0xC9CFDB, kGold = 0xC9A55B, kMuted = 0x9AA3B5;
// 3.0.11-beta.174: the switches' segments: the language's 52 wide, the views' 96; 24 from the screen's sides
constexpr float kLangW = 52, kViewW = 96, kSide = 24, kSegPx = 13.5f, kLabelGap = 10;
constexpr uint32_t kChosenWords = 0xF5E6BE;
// 3.0.11-beta.174: the news pill - 30 high, its foot 70 over the screen's edge (the strip 110 high then), 960 at most
constexpr float kStripNewsH = 110, kNewsH = 30, kNewsAbove = 70, kNewsMost = 960, kNewsGap = 10, kLinkPx = 13.5f;
constexpr uint32_t kNewsWords = 0xE6CB8A, kLink = 0x8CB8FF, kLinkHot = 0xB5D0FF;

// `text` cut at whole characters to `room` with «…» after it
std::string fit(const std::string& text, float px, float room) {
    if (panel::wordsWidth(text, px, false) <= room) return text;
    std::string t = text;
    while (!t.empty()) {
        size_t cut = t.size() - 1;
        while (cut > 0 && (uint8_t(t[cut]) & 0xC0) == 0x80) --cut;
        t.resize(cut);
        while (!t.empty() && t.back() == ' ') t.pop_back();
        if (panel::wordsWidth(t + "…", px, false) <= room) return t + "…";
    }
    return "…";
}

}  // namespace

Layout layout(int targetW, int targetH, float scale, const std::string& statusText) {
    Content c;
    c.status = statusText;
    return layout(targetW, targetH, scale, c);
}

Layout layout(int targetW, int targetH, float scale, const Content& c) {
    const std::string& statusText = c.status;
    const std::string& viewLabel = c.viewLabel;
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    L.w = targetW;
    L.h = int(std::ceil((c.news.empty() ? kStripH : kStripNewsH) * s));
    L.x = 0;
    L.y = targetH - L.h;
    if (!statusText.empty()) {
        const float text = panel::wordsWidth(statusText, kPx * s, false);
        const float w = kPad * s + kDot * s + kDotGap * s + text + kPad * s;
        const float top = float(L.h) - kEdge * s - kPillH * s;
        L.status = panel::Box{std::round((float(targetW) - w) / 2), top, std::ceil(w), kPillH * s};
        L.statusDot = panel::Box{L.status.x + kPad * s, top + (kPillH - kDot) * s / 2, kDot * s, kDot * s};
        L.statusText = panel::Box{L.statusDot.x + (kDot + kDotGap) * s, top, text + 2 * s, kPillH * s};
    }
    const float top = float(L.h) - kEdge * s - kPillH * s;
    for (int i = 0; i < 3; ++i)
        L.langSeg[i] = panel::Box{float(targetW) - kSide * s - (3 - i) * kLangW * s, top, kLangW * s, kPillH * s};
    float x = kSide * s;
    if (!viewLabel.empty()) {
        const float w = panel::wordsWidth(viewLabel, kSegPx * s, false);
        L.viewLabel = panel::Box{x, top, w + 2 * s, kPillH * s};
        x += w + kLabelGap * s;
    }
    for (int i = 0; i < 3; ++i) {
        const float w = std::max(kViewW * s, panel::wordsWidth(c.viewNames[i], kSegPx * s, true) + 26 * s);
        L.viewSeg[i] = panel::Box{x, top, w, kPillH * s};
        x += w;
    }
    if (!c.news.empty()) {  // 3.0.11-beta.174: the title as long as the screen lets it, the link whole after it
        const float link = c.allNews.empty() ? 0.0f : panel::wordsWidth(c.allNews, kLinkPx * s, false);
        const float most = std::min(kNewsMost * s, float(targetW) - 2 * kSide * s);
        const float room = most - 2 * kPad * s - (link > 0 ? link + kNewsGap * s : 0.0f);
        const float title = std::max(0.0f, std::min(panel::wordsWidth(c.news, kPx * s, false), room));
        const float w = 2 * kPad * s + title + (link > 0 ? kNewsGap * s + link : 0.0f);
        const float newsTop = float(L.h) - kNewsAbove * s - kNewsH * s;
        L.news = panel::Box{std::round((float(targetW) - w) / 2), newsTop, std::ceil(w), kNewsH * s};
        L.newsText = panel::Box{L.news.x + kPad * s, newsTop, title, kNewsH * s};
        if (link > 0) L.newsLink = panel::Box{L.newsText.x + title + kNewsGap * s, newsTop, link + 1, kNewsH * s};
    }
    return L;
}

Part hitTest(const Layout& L, float x, float y) {
    if (L.newsLink.w > 0 && L.newsLink.contains(x, y)) return Part::AllNews;
    if (L.news.w > 0 && L.news.contains(x, y)) return Part::News;
    for (int i = 0; i < 3; ++i) {
        if (L.langSeg[i].contains(x, y)) return Part(int(Part::Lang0) + i);
        if (L.viewSeg[i].contains(x, y)) return Part(int(Part::View0) + i);
    }
    return Part::None;
}

void paint(panel::Picture& out, const Layout& L, const Show& sh) {
    const float s = L.s;
    if (L.status.w > 0) {
        panel::roundRect(out, L.status, kPillH * s / 2, kPill, 0.72f, kWhite, 0.10f, 1.0f * s);
        panel::roundRect(out, L.statusDot, kDot * s / 2, sh.statusRgb, 1.0f, sh.statusRgb, 1.0f, 0.0f);
    }
    if (L.news.w > 0)  // 3.0.11-beta.174: the site's item now - its edge in gold, brighter under the mouse
        panel::roundRect(out, L.news, kNewsH * s / 2, kPill, 0.62f, kGold, sh.hover == int(Part::News) ? 0.55f : 0.30f,
                         1.0f * s);
    // 3.0.11-beta.174: a segmented pill - its ground, the chosen one in gold
    const auto segments = [&](const panel::Box* seg, int chosen, int firstPart) {
        const panel::Box all{seg[0].x, seg[0].y, seg[2].x + seg[2].w - seg[0].x, seg[0].h};
        panel::roundRect(out, all, kPillH * s / 2, kPill, 0.72f, kWhite, 0.10f, 1.0f * s);
        for (int i = 0; i < 3; ++i) {
            const bool hot = sh.hover == firstPart + i;
            if (i == chosen)
                panel::roundRect(out, seg[i], kPillH * s / 2, kGold, 0.26f, kGold, 0.0f, 0.0f);
            else if (hot)
                panel::roundRect(out, seg[i], kPillH * s / 2, kWhite, 0.08f, kWhite, 0.0f, 0.0f);
        }
    };
    segments(L.langSeg, sh.lang, int(Part::Lang0));
    if (sh.views) segments(L.viewSeg, sh.view, int(Part::View0));
}

std::vector<Words> words(const Layout& L, const Show& sh) {
    std::vector<Words> out;
    if (L.status.w > 0 && !sh.status.empty())
        out.push_back(Words{L.statusText, sh.status, kWords, kPx * L.s, false, Align::Left});
    for (int i = 0; i < 3; ++i)  // 3.0.11-beta.174: the language's segments
        out.push_back(Words{L.langSeg[i], kLangNames[i], i == sh.lang ? kChosenWords : kMuted, kSegPx * L.s,
                            i == sh.lang, Align::Center});
    if (L.news.w > 0 && !sh.news.empty()) {  // 3.0.11-beta.174
        const float px = kPx * L.s;
        out.push_back(Words{L.newsText, fit(sh.news, px, L.newsText.w),
                            sh.hover == int(Part::News) ? kChosenWords : kNewsWords, px, false, Align::Left});
        if (L.newsLink.w > 0)
            out.push_back(Words{L.newsLink, sh.allNews, sh.hover == int(Part::AllNews) ? kLinkHot : kLink,
                                kLinkPx * L.s, false, Align::Left});
    }
    if (sh.views) {
        if (L.viewLabel.w > 0)
            out.push_back(Words{L.viewLabel, sh.viewLabel, kWords, kSegPx * L.s, false, Align::Left});
        for (int i = 0; i < 3; ++i)
            out.push_back(Words{L.viewSeg[i], sh.viewNames[i], i == sh.view ? kChosenWords : kMuted, kSegPx * L.s,
                                i == sh.view, Align::Center});
    }
    return out;
}

}  // namespace uagfx::loginchrome
