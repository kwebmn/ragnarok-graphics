#include "core/options_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::optionscard {
namespace {

// At scale 1: the card 380 wide, 18 in; its title 32; a volume's row 30 (its words over its bar), a switch's row 30
constexpr float kW = 380, kPad = 18, kTitleH = 32, kRowH = 30, kSwitchW = 40, kSwitchH = 22, kBarH = 8;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kText = 0xEEF2F8, kMuted = 0x9AA3B5;

}  // namespace

int volumeAt(const panel::Box& slider, float x) {
    if (slider.w <= 0) return 0;
    const float t = std::clamp((x - slider.x) / slider.w, 0.0f, 1.0f);
    return int(std::lround(t * float(kVolumeMost)));
}

Layout layout(int targetW, int targetH, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s, inner = w - 2 * pad;
    L.title = panel::Box{pad, pad, inner - 28 * s, kTitleH * s};
    L.close = panel::Box{w - pad - 24 * s, pad + 4 * s, 24 * s, 24 * s};
    float y = pad + kTitleH * s + 6 * s;
    for (int i = 0; i < 6; ++i) {
        L.labels[i] = panel::Box{pad, y, inner - kSwitchW * s - 10 * s, kSwitchH * s};
        L.toggles[i] = panel::Box{w - pad - kSwitchW * s, y, kSwitchW * s, kSwitchH * s};
        y += kSwitchH * s + 4 * s;
        if (i < 2) {  // the music's and the sounds' volume under their switches
            L.sliders[i] = panel::Box{pad, y + 4 * s, inner - 52 * s, kBarH * s};
            L.shares[i] = panel::Box{w - pad - 46 * s, y - 2 * s, 46 * s, 18 * s};
            y += kRowH * s - 8 * s;
        }
        if (i == 1) {
            y += 6 * s;
            L.hint = panel::Box{pad, y, inner, 16 * s};
            y += 20 * s;
        }
        y += 4 * s;
    }
    y += pad - 4 * s;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::max(0, targetH / 2 - L.h / 2);
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    for (int i = 0; i < 6; ++i)
        if (L.toggles[i].contains(x, y)) return Hit{Part::Toggle, i};
    for (int i = 0; i < 2; ++i) {
        const panel::Box& b = L.sliders[i];
        const panel::Box grab{b.x - 6 * L.s, b.y - 8 * L.s, b.w + 12 * L.s, b.h + 16 * L.s};  // easier to catch
        if (grab.contains(x, y)) return Hit{Part::Slider, i};
    }
    return Hit{};
}

std::vector<Words> words(const Layout& L, const Values& v, const Show& sh, const Texts& t) {
    (void)sh;
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, t.title, kText, 15 * s, true, Align::Left});
    out.push_back(Words{L.close, "\xC3\x97", kText, 14 * s, false, Align::Center});  // ×
    for (int i = 0; i < 6; ++i) out.push_back(Words{L.labels[i], t.toggles[i], kText, 13 * s, i < 2, Align::Left});
    for (int i = 0; i < 2; ++i) {
        const int share = int(std::lround(100.0 * double(std::clamp(v.volume[i], 0, kVolumeMost)) / kVolumeMost));
        out.push_back(Words{L.shares[i], std::to_string(share) + "%", kMuted, 11.5f * s, false, Align::Right});
    }
    out.push_back(Words{L.hint, t.snapHint, kMuted, 11 * s, false, Align::Left});
    return out;
}

}  // namespace uagfx::optionscard
