#include "core/key_hints.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace uagfx::keyhint {
namespace {

using settings::Text;

constexpr float kH = 38, kPad = 14, kCapH = 22, kCapPad = 6, kCapGap = 3, kCapText = 12, kAfterCaps = 6;
constexpr float kPartGap = 20, kText = 14;
constexpr uint32_t kInk = 0xECEFF5;

std::string say(settings::Lang l, Text t) { return settings::tr(l, t); }

}  // namespace

const char* keyOf(Moment m) {
    static const char* const kKeys[kMoments] = {"view3d_window", "view3d_skill", "free_camera", "view3d",
                                                "item_picks"};
    return size_t(m) < size_t(kMoments) ? kKeys[size_t(m)] : "";
}

std::array<bool, kMoments> momentsOf(const Now& n) {
    std::array<bool, kMoments> a{};
    a[size_t(Moment::View3dWindow)] = n.view3d && !n.freeCamera && n.looking && n.windowOpen;
    a[size_t(Moment::View3dSkill)] = n.view3d && !n.freeCamera && n.looking && n.skillWait != 0;
    a[size_t(Moment::FreeCamera)] = n.view3d && n.freeCamera;
    a[size_t(Moment::View3d)] = n.view3d && !n.freeCamera;
    a[size_t(Moment::ItemPicks)] = n.itemsShared;
    return a;
}

std::vector<Part> partsOf(Moment m, const Now& n, settings::Lang l) {
    switch (m) {
        case Moment::View3dWindow: return {{"Alt", say(l, Text::KeyHintHoldAlt)}};
        case Moment::View3dSkill:
            return {{say(l, Text::KeyClickAlone),
                     say(l, n.skillWait == 4 ? Text::KeyHintSkillFriend : Text::KeyHintSkillEnemy)}};
        case Moment::FreeCamera:
            return {{"W A S D", say(l, Text::KeyHintFly)},
                    {say(l, Text::KeySpace), say(l, Text::KeyHintUp)},
                    {"C", say(l, Text::KeyHintDown)},
                    {"Shift", say(l, Text::KeyHintFaster)},
                    {"Ctrl+Shift+C", say(l, Text::KeyHintBack)}};
        case Moment::View3d: {
            std::vector<Part> out = {{"Alt", say(l, Text::KeyHintCursor)}};
            if (n.behind) {
                out.push_back({say(l, Text::KeyWheel), say(l, Text::KeyHintDistance)});
                out.push_back({"Shift", say(l, Text::KeyHintRound)});
            }
            out.push_back({"Tab", say(l, Text::KeyHintTarget)});
            out.push_back({"Ctrl+Shift+V", say(l, n.behind ? Text::KeyHintToEyes : Text::KeyHintToUsual)});
            return out;
        }
        case Moment::ItemPicks: {
            const std::string click = say(l, Text::KeyClick);
            return {{"Ctrl+" + click, say(l, Text::KeyHintPick)},
                    {"Shift+" + click, say(l, Text::KeyHintPickRun)},
                    {"Ctrl+A", say(l, Text::KeyHintPickAll)}};
        }
        default: return {};
    }
}

Seen read(const std::string& text) {
    Seen s;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        const size_t space = line.find(' ');
        if (space == std::string::npos) continue;
        const std::string key = line.substr(0, space), value = line.substr(space + 1);
        for (int m = 0; m < kMoments; ++m) {
            if (key != keyOf(Moment(m))) continue;
            if (value == "learned") s.learned[size_t(m)] = true;
            else s.times[size_t(m)] = std::clamp(std::atoi(value.c_str()), 0, kTimes);
        }
    }
    return s;
}

std::string write(const Seen& s) {
    std::string out;
    for (int m = 0; m < kMoments; ++m) {
        if (s.times[size_t(m)] > 0)
            out += std::string(keyOf(Moment(m))) + ' ' + std::to_string(s.times[size_t(m)]) + '\n';
        if (s.learned[size_t(m)]) out += std::string(keyOf(Moment(m))) + " learned\n";
    }
    return out;
}

uint32_t capOf(Moment m) {
    switch (m) {
        case Moment::View3dWindow:
        case Moment::View3dSkill: return 0;
        case Moment::FreeCamera: return 10000;
        default: return 8000;
    }
}

Moment Shower::frame(uint32_t nowMs, const std::array<bool, kMoments>& active, const std::array<bool, kMoments>& done,
                     Seen& seen, bool& changed) {
    const uint32_t step = started_ ? std::min<uint32_t>(nowMs - last_, 250) : 0;  // a pause is no showing
    started_ = true;
    last_ = nowMs;
    for (int m = 0; m < kMoments; ++m)
        if (done[size_t(m)] && !seen.learned[size_t(m)]) seen.learned[size_t(m)] = true, changed = true;
    for (int m = 0; m < kMoments; ++m) {
        Episode& e = episodes_[size_t(m)];
        if (!active[size_t(m)]) e = Episode{};
        else if (!e.on) e = Episode{true, false, false, 0};
    }
    for (int m = 0; m < kMoments; ++m) {
        Episode& e = episodes_[size_t(m)];
        if (!active[size_t(m)] || seen.learned[size_t(m)] || e.over || (seen.times[size_t(m)] >= kTimes && !e.counted))
            continue;
        e.shownMs += step;
        if (!e.counted && e.shownMs >= kCountMs) {
            e.counted = true;
            seen.times[size_t(m)] = std::min(seen.times[size_t(m)] + 1, kTimes);
            changed = true;
        }
        const uint32_t cap = capOf(Moment(m));
        if (cap && e.shownMs >= cap) {
            e.over = true;
            return Moment::Count;
        }
        return Moment(m);
    }
    return Moment::Count;
}

std::vector<std::string> capsOf(const std::string& keys) {
    std::vector<std::string> out;
    std::string cur;
    for (const char c : keys) {
        if (c == '+' || c == ' ') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

Layout layout(const std::vector<Part>& parts, int centreX, int bottom, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    float x = kPad * s;
    const float cy = (kH - kCapH) / 2 * s;
    for (const Part& p : parts) {
        Layout::Placed placed;
        for (const std::string& key : capsOf(p.keys)) {
            const float w = std::max(kCapH * s, panel::wordsWidth(key, kCapText * s, true) + 2 * kCapPad * s);
            placed.caps.push_back(panel::Box{x, cy, w, kCapH * s});
            x += w + kCapGap * s;
        }
        if (!placed.caps.empty()) x += (kAfterCaps - kCapGap) * s;
        if (!p.words.empty()) {
            const float w = panel::wordsWidth(p.words, kText * s, false) + 2 * s;
            placed.words = panel::Box{x, 0, w, kH * s};
            x += w;
        }
        L.parts.push_back(placed);
        x += kPartGap * s;
    }
    x += (kPad - kPartGap) * s;
    L.glass = panel::Box{0, 0, std::max(x, 60 * s), kH * s};
    L.w = int(std::ceil(L.glass.w));
    L.h = int(std::ceil(L.glass.h));
    L.x = centreX - L.w / 2;
    L.y = bottom - L.h;
    return L;
}

std::vector<Words> words(const Layout& L, const std::vector<Part>& parts) {
    std::vector<Words> out;
    const float s = L.s;
    for (size_t i = 0; i < L.parts.size() && i < parts.size(); ++i) {
        const std::vector<std::string> caps = capsOf(parts[i].keys);
        for (size_t c = 0; c < caps.size() && c < L.parts[i].caps.size(); ++c)
            out.push_back(Words{L.parts[i].caps[c], caps[c], kInk, kCapText * s, true, Align::Center});
        if (!parts[i].words.empty())
            out.push_back(Words{L.parts[i].words, parts[i].words, kInk, kText * s, false, Align::Left});
    }
    return out;
}

}  // namespace uagfx::keyhint
