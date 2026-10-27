#include "core/answer_card.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::card {
namespace {

using panel::Box;

// The card at scale 1, in pixels
constexpr float kPad = 14, kTop = 12, kRow = 22, kGap = 8, kLine = 18, kButtonsGap = 10, kButton = 28, kBottom = 14;
constexpr float kRadius = 10, kChipH = 20, kChipPad = 8, kButtonPad = 12, kMargin = 8, kLeast = 260;
constexpr uint32_t kGround = 0x0E121A, kWhite = 0xFFFFFF, kBlue = 0x2F6FD1, kBlueHover = 0x3D7FE0;

// The bytes of the longest start of `word` that `fits` takes, at a character's start; one character at least
size_t longestPrefix(const std::string& word, const std::function<bool(const std::string&)>& fits) {
    size_t best = 0;
    for (size_t at = 1; at <= word.size(); ++at) {
        if (at < word.size() && (static_cast<unsigned char>(word[at]) & 0xC0) == 0x80) continue;
        if (!fits(word.substr(0, at))) break;
        best = at;
    }
    if (best == 0) {  // not even one: one anyway, the line goes on
        best = 1;
        while (best < word.size() && (static_cast<unsigned char>(word[best]) & 0xC0) == 0x80) ++best;
    }
    return best;
}

void dropLast(std::string& s) {  // one UTF-8 character
    while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80) s.pop_back();
    if (!s.empty()) s.pop_back();
}

}  // namespace

Geometry geometry(float s, int roomWidth, int lines, const Widths& w) {
    Geometry g;
    g.scale = s;
    g.lines = std::clamp(lines, 1, 2);
    const float room = float(roomWidth) - 2 * kMargin * s;
    g.width = int(std::lround(std::max(std::min(kWidth * s, room), std::min(kLeast * s, float(roomWidth)))));
    const float W = float(g.width);
    g.radius = kRadius * s;
    const float left = kPad * s + 2 * s, rowY = kTop * s;
    g.dot = Box{left, rowY + (kRow - 8) / 2 * s, 8 * s, 8 * s};
    const float chipW = w.chip + 2 * kChipPad * s;
    g.chip = Box{W - kPad * s - chipW, rowY + (kRow - kChipH) / 2 * s, chipW, kChipH * s};
    g.chipText = Box{g.chip.x + kChipPad * s, g.chip.y, w.chip, g.chip.h};
    const float titleX = g.dot.x + g.dot.w + 8 * s;
    g.title = Box{titleX, rowY, std::max(0.0f, g.chip.x - 8 * s - titleX), kRow * s};
    const float quoteY = rowY + (kRow + kGap) * s;
    g.quote = Box{left, quoteY, std::max(0.0f, W - kPad * s - left), kLine * s * float(g.lines)};
    const float by = quoteY + g.quote.h + kButtonsGap * s;
    g.open = Box{left, by, w.open + 2 * kButtonPad * s, kButton * s};
    g.openText = Box{g.open.x + kButtonPad * s, by, w.open, kButton * s};
    g.later = Box{g.open.x + g.open.w + 8 * s, by, w.later + 2 * kButtonPad * s, kButton * s};
    g.laterText = Box{g.later.x + kButtonPad * s, by, w.later, kButton * s};
    const float timeX = g.later.x + g.later.w + 8 * s;
    g.time = Box{timeX, by, std::max(0.0f, W - kPad * s - timeX), kButton * s};
    g.height = int(std::lround(by + (kButton + kBottom) * s));
    g.accent = Box{5 * s, 10 * s, 3 * s, std::max(0.0f, float(g.height) - 20 * s)};
    return g;
}

panel::Point position(const Geometry& g, int roomWidth) {
    return panel::Point{(roomWidth - g.width) / 2, int(std::lround(kMargin * g.scale))};
}

Part hitTest(const Geometry& g, float x, float y) {
    if (x < 0 || y < 0 || x >= float(g.width) || y >= float(g.height)) return Part::None;
    if (g.open.contains(x, y)) return Part::Open;
    if (g.later.contains(x, y)) return Part::Later;
    return Part::Body;
}

uint32_t statusColour(reports::Status s) {
    if (s == reports::Status::NeedInfo) return 0xF0A050;
    if (s == reports::Status::Fixed) return 0x4CAF6E;
    return 0x5B8DEF;
}

void paint(panel::Picture& out, const Geometry& g, reports::Status st, Part hover) {
    out.width = g.width;
    out.height = g.height;
    out.px.assign(size_t(std::max(out.width, 0)) * size_t(std::max(out.height, 0)), 0u);
    const float s = g.scale, edge = std::max(1.0f, s);
    const uint32_t c = statusColour(st);
    panel::roundRect(out, Box{0, 0, float(g.width), float(g.height)}, g.radius, kGround, 0.90f, kWhite, 0.12f, edge);
    panel::roundRect(out, g.accent, 1.5f * s, c, 1.0f);
    panel::circle(out, g.dot.x + g.dot.w / 2, g.dot.y + g.dot.h / 2, g.dot.w / 2, c, 1.0f);
    panel::roundRect(out, g.chip, 6 * s, c, 0.20f);
    panel::roundRect(out, g.open, 7 * s, hover == Part::Open ? kBlueHover : kBlue, 1.0f);
    panel::roundRect(out, g.later, 7 * s, kWhite, hover == Part::Later ? 0.10f : 0.0f, kWhite, 0.24f, edge);
}

std::vector<std::string> fitLines(const std::string& text, size_t most,
                                  const std::function<bool(const std::string&)>& fits) {
    std::vector<std::string> words;
    std::string word;
    for (char ch : text) {
        if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') {
            if (!word.empty()) words.push_back(word);
            word.clear();
        } else {
            word += ch;
        }
    }
    if (!word.empty()) words.push_back(word);
    std::vector<std::string> lines;
    if (most == 0 || words.empty()) return lines;
    std::string line;
    bool more = false;
    for (size_t i = 0; i < words.size();) {
        const std::string candidate = line.empty() ? words[i] : line + " " + words[i];
        if (fits(candidate)) {
            line = candidate;
            ++i;
            continue;
        }
        if (line.empty()) {  // one word wider than a line: as much of it as fits, the rest on the next
            const size_t cut = longestPrefix(words[i], fits);
            line = words[i].substr(0, cut);
            words[i] = words[i].substr(cut);
            if (words[i].empty()) ++i;
        }
        if (lines.size() + 1 == most) {  // the last line, and words left
            more = true;
            break;
        }
        lines.push_back(line);
        line.clear();
    }
    if (more) {
        while (!line.empty() && !fits(line + "…")) dropLast(line);
        while (!line.empty() && line.back() == ' ') line.pop_back();
        line += "…";
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

}  // namespace uagfx::card
