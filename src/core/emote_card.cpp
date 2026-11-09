#include "core/emote_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::emotecard {
namespace {

// At scale 1: the card 18 in; its title 30 high, the hint 18; a cell 56 x 58 (the picture's 40 over the command's
// 14), 4 apart
constexpr float kPad = 18, kTitleH = 30, kHintH = 18, kCellW = 56, kCellH = 58, kGap = 4, kLabelH = 14;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5;

void put(panel::Picture& out, const panel::Picture& pic, const panel::Box& box) {
    const int ox = int(std::lround(box.x + (box.w - float(pic.width)) / 2));
    const int oy = int(std::lround(box.y + (box.h - float(pic.height)) / 2));
    if (out.scene) {  // 3.0.18-beta.12: recorded for uaUI
        panel::recordImage(out, pic, float(ox), float(oy), 1.0f);
        return;
    }
    for (int y = 0; y < pic.height; ++y)
        for (int x = 0; x < pic.width; ++x) {
            const int tx = ox + x, ty = oy + y;
            if (tx < 0 || ty < 0 || tx >= out.width || ty >= out.height) continue;
            const uint32_t c = pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
            if (!(c >> 24)) continue;
            uint32_t& d = out.px[size_t(ty) * size_t(out.width) + size_t(tx)];
            const uint32_t keep = 255 - (c >> 24);
            const auto ch = [&](int sh) {
                return std::min<uint32_t>(255, ((c >> sh) & 0xFF) + ((d >> sh) & 0xFF) * keep / 255);
            };
            d = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
        }
}

}  // namespace

Layout layout(int targetW, int targetH, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float pad = kPad * s, cw = kCellW * s, ch = kCellH * s, gap = kGap * s;
    const float w = 2 * pad + float(kColumns) * cw + float(kColumns - 1) * gap;
    L.title = panel::Box{pad, pad, w - 2 * pad - 30 * s, kTitleH * s};
    L.close = panel::Box{w - pad - 26 * s, pad + 2 * s, 26 * s, 26 * s};
    L.hint = panel::Box{pad, pad + kTitleH * s, w - 2 * pad, kHintH * s};
    const float top = pad + (kTitleH + kHintH + 8) * s;
    const int rows = (kEmotions + kColumns - 1) / kColumns;
    for (int i = 0; i < kEmotions; ++i) {
        const float cx = pad + float(i % kColumns) * (cw + gap), cy = top + float(i / kColumns) * (ch + gap);
        L.cells.push_back(panel::Box{cx, cy, cw, ch});
        L.icons.push_back(panel::Box{cx + (cw - kIconSide * s) / 2, cy + 2 * s, kIconSide * s, kIconSide * s});
        L.labels.push_back(panel::Box{cx, cy + ch - kLabelH * s - 1 * s, cw, kLabelH * s});
    }
    const float h = top + float(rows) * ch + float(rows - 1) * gap + pad;
    L.glass = panel::Box{0, 0, w, h};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(h));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::clamp(int(float(targetH) * 0.30f) - L.h / 2, 0, std::max(0, targetH - L.h));
    return L;
}

int cellAt(const Layout& L, float x, float y) {
    for (size_t i = 0; i < L.cells.size(); ++i)
        if (L.cells[i].contains(x, y)) return int(i);
    return -1;
}

bool closeAt(const Layout& L, float x, float y) { return L.close.contains(x, y); }

void paint(panel::Picture& out, const Layout& L, const Show& sh, const std::vector<panel::Picture>& icons) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, uistyle::window(), kWhite, 0.12f, 1.0f * s);
    panel::roundRect(out, L.close, 6 * s, kWhite, sh.hoverClose ? 0.14f : 0.0f, kWhite, sh.hoverClose ? 0.3f : 0.0f,
                     1.0f * s);
    for (size_t i = 0; i < L.cells.size(); ++i) {
        const bool hot = int(i) == sh.hover;
        panel::roundRect(out, L.cells[i], 8 * s, hot ? kGold : kWhite, hot ? 0.20f : 0.04f, hot ? kGold : kWhite,
                         hot ? 0.8f : 0.08f, 1.0f * s);
        if (i < icons.size() && icons[i].width > 0 && icons[i].height > 0) put(out, icons[i], L.icons[i]);
    }
}

std::vector<Words> words(const Layout& L, const std::vector<std::string>& commands, const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, t.title, kText, 15 * s, true, Align::Left});
    out.push_back(Words{L.close, "\xE2\x9C\x95", sh.hoverClose ? kText : kMuted, 13 * s, false, Align::Center});  // ✕
    out.push_back(Words{L.hint, t.hint, kMuted, 11.5f * s, false, Align::Left});
    for (size_t i = 0; i < L.labels.size() && i < commands.size(); ++i) {
        const bool hot = int(i) == sh.hover;
        out.push_back(Words{L.labels[i], commands[i], hot ? kGold2 : kText, 11 * s, hot, Align::Center});
    }
    return out;
}

}  // namespace uagfx::emotecard
