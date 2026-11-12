#include "core/launcher_picker.hpp"

#include <algorithm>

namespace uagfx::launcherpick {
namespace {

// At scale 1
constexpr float kCardW = 520, kPad = 18, kTitleH = 24, kAutoH = 52, kHeadH = 22, kGap = 10, kRowGap = 12;
constexpr float kTextOptionH = 40, kPictureName = 26, kFootH = 34, kCloseW = 28, kMargin = 8;

}  // namespace

Picker layoutFor(const launcherview::Layout& L) {
    namespace presets = settings::presets;
    Picker p;
    const float s = L.s > 0 ? L.s : 1.0f;
    p.s = s;
    const float W = float(std::max(L.w, 1)), H = float(std::max(L.h, 1));
    const float w = std::min(kCardW * s, W - 2 * kMargin * s);
    const float inner = w - 2 * kPad * s;
    const float pictureW = (inner - 3 * kGap * s) / 4;  // «Картинка»: four with pictures (16:9) and their names
    const float pictureH = pictureW * 9.0f / 16.0f + kPictureName * s;
    const float h = kPad * s + kTitleH * s + kGap * s + kAutoH * s + 3 * (kRowGap * s + kHeadH * s) + pictureH +
                    2 * kTextOptionH * s + 16 * s + kFootH * s + 16 * s;
    // the right edge on the play column's, the bottom over the look line; in the window under the top bar
    const float right = L.playMore.x + L.playMore.w;
    const float x = std::max(kMargin * s, right - w);
    float y = L.look.y - kGap * s - h;
    y = std::min(y, H - kMargin * s - h);
    y = std::max(y, L.bar.y + L.bar.h + kMargin * s);
    p.card = Box{x, y, w, h};
    const float left = x + kPad * s;
    float at = y + kPad * s;
    p.title = Box{left, at, inner - kCloseW * s - kGap * s, kTitleH * s};
    p.close = Box{x + w - kPad * s - kCloseW * s, at - 2 * s, kCloseW * s, kCloseW * s};
    at += kTitleH * s + kGap * s;
    p.autoRow = Box{left, at, inner, kAutoH * s};
    at += kAutoH * s;
    for (int c = 0; c < presets::kChoices; ++c) {
        const auto choice = presets::Choice(c);
        at += kRowGap * s;
        p.heads[c] = Box{left, at, inner, kHeadH * s};
        at += kHeadH * s;
        const int n = presets::variants(choice);
        const float ow = (inner - float(n - 1) * kGap * s) / float(n);
        const float oh = choice == presets::Choice::Picture ? pictureH : kTextOptionH * s;
        for (int v = 0; v < n; ++v)
            p.options.push_back(Option{choice, v, Box{left + float(v) * (ow + kGap * s), at, ow, oh}});
        at += oh;
    }
    at += 16 * s;
    const float third = (inner - 2 * kGap * s) / 3;
    p.test = Box{left, at, third, kFootH * s};
    p.undo = Box{left + third + kGap * s, at, third, kFootH * s};
    p.all = Box{left + 2 * (third + kGap * s), at, third, kFootH * s};
    return p;
}

int optionAt(const Picker& p, float x, float y) {
    for (size_t i = 0; i < p.options.size(); ++i)
        if (p.options[i].box.contains(x, y)) return int(i);
    return -1;
}

PickHit pickAt(const Picker& p, float x, float y) {
    if (!p.card.contains(x, y)) return PickHit{};
    if (p.close.contains(x, y)) return PickHit{PickPart::Close};
    const int option = optionAt(p, x, y);
    if (option >= 0) return PickHit{PickPart::Option, option};
    if (p.autoRow.contains(x, y)) return PickHit{PickPart::Auto};
    if (p.test.contains(x, y)) return PickHit{PickPart::Test};
    if (p.undo.contains(x, y)) return PickHit{PickPart::Undo};
    if (p.all.contains(x, y)) return PickHit{PickPart::All};
    return PickHit{PickPart::Card};
}

}  // namespace uagfx::launcherpick
