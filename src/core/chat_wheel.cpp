#include "core/chat_wheel.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "core/ui_style.hpp"

namespace uagfx::chatwheel {
namespace {

// At scale 1: the ring 190 out, 64 in; its labels 120 x 44 (10 sectors: 96); the chips 30 high; 20 round it
constexpr float kOuter = 190, kInner = 64, kChipH = 30, kPad = 20, kGap = 12, kHintH = 22;
constexpr float kPi = 3.14159265358979f;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5;

std::string trimmed(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

}  // namespace

int sectorAt(float x, float y, int count, float dead) {
    if (count <= 0 || std::sqrt(x * x + y * y) < dead) return -1;
    float a = std::atan2(x, y);  // 0 at the top, clockwise
    if (a < 0) a += 2 * kPi;
    const float step = 2 * kPi / float(count);
    return int(std::floor((a + step / 2) / step)) % count;
}

std::vector<std::string> phraseList(const std::array<std::string, kPhrases>& configured,
                                    const std::array<std::string, kPhrases>& defaults) {
    std::vector<std::string> out;
    for (int i = 0; i < kPhrases; ++i) {
        const std::string mine = trimmed(configured[size_t(i)]);
        out.push_back(mine.empty() ? defaults[size_t(i)] : mine);
    }
    return out;
}

std::vector<std::string> emoteList(const std::string& configured, const std::vector<std::string>& defaults) {
    std::vector<std::string> out;
    std::istringstream in(configured);
    std::string word;
    while (in >> word && int(out.size()) < kEmotes)
        if (word.size() > 1 && word[0] == '/') out.push_back(word);
    return out.empty() ? defaults : out;
}

Layout layout(int count, int targetW, int targetH, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    L.outer = kOuter * s;
    L.inner = kInner * s;
    const float w = 2 * L.outer + 2 * kPad * s;
    L.cx = w / 2;
    float y = kPad * s;
    const float pw = 120 * s;
    L.pages[0] = panel::Box{L.cx - pw - kGap * s / 2, y, pw, kChipH * s};
    L.pages[1] = panel::Box{L.cx + kGap * s / 2, y, pw, kChipH * s};
    y += kChipH * s + kGap * s;
    L.cy = y + L.outer;
    y += 2 * L.outer + kGap * s;
    const float cw = 110 * s;
    for (int i = 0; i < kChannels; ++i)
        L.channels[i] = panel::Box{L.cx - 1.5f * cw - kGap * s + float(i) * (cw + kGap * s), y, cw, kChipH * s};
    y += kChipH * s + kGap * s;
    L.keyboard = panel::Box{kPad * s, y, w / 2 - kPad * s - kGap * s / 2, kChipH * s};
    L.voice = panel::Box{w / 2 + kGap * s / 2, y, w / 2 - kPad * s - kGap * s / 2, kChipH * s};
    y += kChipH * s + 6 * s;
    L.hint = panel::Box{kPad * s, y, w - 2 * kPad * s, kHintH * s};
    y += kHintH * s + kPad * s;
    const float lw = (count > 8 ? 96 : 120) * s, lh = 44 * s, mid = (L.inner + L.outer) / 2;
    for (int i = 0; i < count; ++i) {
        const float a = 2 * kPi * float(i) / float(count);
        const float lx = L.cx + std::sin(a) * mid, ly = L.cy - std::cos(a) * mid;
        L.labels.push_back(panel::Box{lx - lw / 2, ly - lh / 2, lw, lh});
    }
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::clamp(targetW / 2 - L.w / 2, 0, std::max(0, targetW - L.w));
    L.y = std::clamp(targetH / 2 - L.h / 2, 0, std::max(0, targetH - L.h));
    return L;
}

std::vector<Words> words(const Layout& L, const std::vector<std::string>& items, int hover, Page page,
                         Channel channel, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    for (size_t i = 0; i < L.labels.size() && i < items.size(); ++i) {
        const bool hot = int(i) == hover;
        panel::Box b = L.labels[i];
        if (page == Page::Emotes) b = panel::Box{b.x, b.y + b.h * 0.5f, b.w, b.h * 0.5f};  // under its picture
        out.push_back(Words{b, items[i], hot ? kGold2 : kText, (page == Page::Emotes ? 12.5f : 12.5f) * s, hot,
                            Align::Center, page == Page::Phrases});
    }
    const panel::Box middle{L.cx - L.inner, L.cy - 14 * s, 2 * L.inner, 28 * s};
    out.push_back(Words{middle, t.pages[int(page)], kGold2, 15 * s, true, Align::Center});
    for (int i = 0; i < 2; ++i)
        out.push_back(Words{L.pages[i], t.pages[i], int(page) == i ? kGold2 : kMuted, 12.5f * s, int(page) == i,
                            Align::Center});
    for (int i = 0; i < kChannels; ++i) {
        const bool on = int(channel) == i && page == Page::Phrases;
        out.push_back(Words{L.channels[i], t.channels[i], on ? kGold2 : kMuted, 12 * s, on, Align::Center});
    }
    out.push_back(Words{L.keyboard, t.keyboard, kText, 12 * s, false, Align::Center});
    out.push_back(Words{L.voice, t.voice, kText, 12 * s, false, Align::Center});
    out.push_back(Words{L.hint, t.hint, kMuted, 11.5f * s, false, Align::Center});
    return out;
}

}  // namespace uagfx::chatwheel
