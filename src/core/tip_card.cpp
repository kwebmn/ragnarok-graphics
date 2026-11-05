#include "core/tip_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::tipcard {
namespace {

// At scale 1: the card 440 wide, 18 in; its title 30 high, the words 19 a row (3 to 12 rows), the box's row 22,
// buttons 36
constexpr float kW = 440, kPad = 18, kTitleH = 30, kRowH = 19, kTickH = 22, kButtonH = 36, kGap = 10;
constexpr int kLeastRows = 3, kMostRows = 12;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kText = 0xEEF2F8, kMuted = 0x9AA3B5;
constexpr uint32_t kBlack = 0x000000;

std::string trimmed(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

}  // namespace

std::vector<std::string> paragraphs(const std::vector<std::string>& lines) {
    std::vector<std::string> out;
    for (const std::string& line : lines) {
        std::string t = trimmed(line);
        if (out.empty() && !t.empty() && t[0] == '*') t = trimmed(t.substr(1));
        if (!t.empty()) out.push_back(t);
    }
    return out;
}

std::string counter(int number, int count) {
    if (count <= 0 || number <= 0) return "";
    return std::to_string(number) + " / " + std::to_string(count);
}

float textWidth(float scale) { return (kW - 2 * kPad) * (scale > 0 ? scale : 1.0f); }

int rows(const std::string& text, float width, const std::function<float(const std::string&)>& measure) {
    int out = 0;
    size_t at = 0;
    while (true) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(at, end - at);
        float x = 0;
        int lineRows = 1;
        for (size_t from = 0; from < line.size();) {
            size_t sp = line.find(' ', from);
            sp = sp == std::string::npos ? line.size() : sp + 1;
            const float w = measure(line.substr(from, sp - from));
            if (x > 0 && x + w > width) {
                ++lineRows;
                x = 0;
            }
            x += w;
            from = sp;
        }
        out += lineRows;
        if (end == text.size()) break;
        at = end + 1;
    }
    return std::max(1, out);
}

Layout layout(const Card& c, int targetW, int targetH, float scale, const Texts& t) {
    (void)t;
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s;
    L.title = panel::Box{pad, pad, w - 2 * pad - 80 * s, kTitleH * s};
    L.counter = panel::Box{w - pad - 80 * s, pad, 80 * s, kTitleH * s};
    float y = pad + kTitleH * s + 4 * s;
    const int rows = std::clamp(c.rows, kLeastRows, kMostRows);
    L.text = panel::Box{pad, y, w - 2 * pad, kRowH * float(rows) * s};
    y += L.text.h + kGap * s;
    L.tick = panel::Box{pad, y + 2 * s, 18 * s, 18 * s};
    L.tickLabel = panel::Box{pad + 26 * s, y, w - 2 * pad - 26 * s, kTickH * s};
    y += kTickH * s + kGap * s;
    const float bw = (w - 2 * pad - 2 * kGap * s) / 3;
    L.back = panel::Box{pad, y, bw, kButtonH * s};
    L.next = panel::Box{pad + bw + kGap * s, y, bw, kButtonH * s};
    L.close = panel::Box{pad + 2 * (bw + kGap * s), y, bw, kButtonH * s};
    y += kButtonH * s + pad;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::clamp(int(float(targetH) * 0.30f) - L.h / 2, 0, std::max(0, targetH - L.h));
    return L;
}

Part partAt(const Layout& L, float x, float y) {
    if (L.back.contains(x, y)) return Part::Back;
    if (L.next.contains(x, y)) return Part::Next;
    if (L.close.contains(x, y)) return Part::Close;
    if (L.tick.contains(x, y)) return Part::Startup;
    if (L.tickLabel.contains(x, y)) return Part::Startup;
    return Part::None;
}

int gameButton(Part p) {
    switch (p) {
        case Part::Back: return 0xB4;
        case Part::Next: return 0x98;
        case Part::Close: return 0x87;
        case Part::Startup: return 0x93;
        case Part::None: break;
    }
    return 0;
}

void paint(panel::Picture& out, const Layout& L, const Card& c, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, uistyle::window(), kWhite, 0.12f, 1.0f * s);
    const bool tickHot = sh.hover == Part::Startup;
    panel::roundRect(out, L.tick, 4 * s, c.startup ? kGold : kWhite, c.startup ? 0.85f : (tickHot ? 0.14f : 0.06f),
                     c.startup ? kGold : kWhite, c.startup ? 1.0f : 0.35f, 1.0f * s);
    const auto button = [&](const panel::Box& b, Part p, bool primary) {
        const bool hot = sh.hover == p;
        panel::roundRect(out, b, 8 * s, primary ? kGold : kWhite,
                         primary ? (hot ? 0.34f : 0.22f) : (hot ? 0.12f : 0.06f), primary ? kGold : kWhite,
                         primary ? 0.8f : 0.16f, 1.0f * s);
    };
    button(L.back, Part::Back, false);
    button(L.next, Part::Next, true);
    button(L.close, Part::Close, false);
}

std::vector<Words> words(const Layout& L, const Card& c, const Show& sh, const Texts& t) {
    (void)sh;
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, t.title, kText, 15 * s, true, Align::Left});
    const std::string n = counter(c.number, c.count);
    if (!n.empty()) out.push_back(Words{L.counter, n, kMuted, 12 * s, false, Align::Right});
    std::string text;
    for (const std::string& p : c.paragraphs) text += (text.empty() ? "" : "\n") + p;
    out.push_back(Words{L.text, text, kText, 13 * s, false, Align::Left, true});
    if (c.startup) out.push_back(Words{L.tick, "\xE2\x9C\x93", kBlack, 13 * s, true, Align::Center});  // ✓
    out.push_back(Words{L.tickLabel, t.startup, kMuted, 12.5f * s, false, Align::Left});
    const auto button = [&](const panel::Box& b, const std::string& label, const std::string& key, bool primary) {
        out.push_back(Words{b, label, kText, 13 * s, primary, Align::Center});
        if (!key.empty())
            out.push_back(Words{panel::Box{b.x + b.w - 40 * s, b.y, 34 * s, b.h}, key, kMuted, 11 * s, false,
                                Align::Right});
    };
    button(L.back, t.back, t.backKey, false);
    button(L.next, t.next, t.nextKey, true);
    button(L.close, t.close, t.closeKey, false);
    return out;
}

}  // namespace uagfx::tipcard
