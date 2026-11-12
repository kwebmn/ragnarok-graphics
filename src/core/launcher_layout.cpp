#include "core/launcher_layout.hpp"

#include <algorithm>

namespace uagfx::launcherview {
namespace {

// At scale 1 (the design 1280 x 720)
constexpr float kBar = 64, kLeft = 22, kRight = 26, kBottom = 26, kFeedTop = 92, kFeedW = 380, kLabel = 28;
// 3.0.18-beta.203: the logo (logo.webp, 491 x 206, cut out of the client's login picture) - taller than the bar
constexpr float kLogoX = 20, kLogoY = 6, kLogoH = 76, kLogoW = kLogoH * 491 / 206;
constexpr float kUpdateH = 150, kNewsH = 84, kGap = 10, kIcon = 64, kIconGap = 12, kDockBottom = 22;
constexpr float kPlayW = 336, kLookH = 52, kButtonH = 64, kMoreW = 56, kPlateH = 44, kPlateW = 560;
constexpr float kNoUpdateH = 32, kRowH = 22, kChanW = 60, kCheckW = 64;

}  // namespace

Layout layoutFor(int width, int height, float scale, int newsCount, bool plate, bool offered) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    const float W = float(std::max(width, 1)), H = float(std::max(height, 1));
    L.s = s;
    L.w = width;
    L.h = height;
    L.bar = Box{0, 0, W, kBar * s};
    L.logo = Box{kLogoX * s, kLogoY * s, kLogoW * s, kLogoH * s};
    L.language = Box{W - kRight * s - 56 * s, 18 * s, 56 * s, 28 * s};
    L.server = Box{L.language.x - 12 * s - 280 * s, 16 * s, 280 * s, 32 * s};
    // the play column: the version at the bottom, the button over it, the look line over that
    const float playW = std::min(kPlayW * s, W * 0.4f);
    const float px = W - kRight * s - playW;
    // 3.0.18-beta.202: the bottom row - «Реліз | Бета», the version at its right
    const float rowY = H - kBottom * s - kRowH * s;
    L.channel[0] = Box{px, rowY, kChanW * s, kRowH * s};
    L.channel[1] = Box{px + (kChanW + 4) * s, rowY, kChanW * s, kRowH * s};
    const float vx = px + (2 * kChanW + 12) * s;
    L.version = Box{vx, rowY, std::max(0.0f, px + playW - vx), kRowH * s};
    float above = rowY;  // what the button stands on: the row, or «Грати без оновлення» over it
    if (offered) {
        L.noUpdate = Box{px, rowY - 8 * s - kNoUpdateH * s, playW, kNoUpdateH * s};
        above = L.noUpdate.y;
    }
    const float by = above - 8 * s - kButtonH * s;
    L.check = Box{px, by, kCheckW * s, kButtonH * s};  // «Перевірити»: the big button's left part
    const float bx = px + kCheckW * s;  // 3.0.18-beta.203: one button - ⟳ | ГРАТИ | ▾
    L.play = Box{bx, by, std::max(0.0f, px + playW - kMoreW * s - bx), kButtonH * s};
    L.playMore = Box{px + playW - kMoreW * s, by, kMoreW * s, kButtonH * s};
    L.look = Box{px, by - kGap * s - kLookH * s, playW, kLookH * s};
    // the icons at the bottom left, a plate over them when asked
    const float iconY = H - kDockBottom * s - kIcon * s;
    for (int i = 0; i < kDockIcons; ++i)
        L.dock[i] = Box{kLeft * s + float(i) * (kIcon + kIconGap) * s, iconY, kIcon * s, kIcon * s};
    float bottom = iconY - 16 * s;  // what the feed may reach
    if (plate) {
        const float pw = std::min(kPlateW * s, px - kLeft * s - 24 * s);
        L.plate = Box{kLeft * s, iconY - 12 * s - kPlateH * s, pw, kPlateH * s};
        bottom = L.plate.y - 12 * s;
    }
    // «Що нового»: its label, the update card, as many news cards as fit
    const float feedW = std::min(kFeedW * s, px - kLeft * s - 24 * s);
    L.feed = Box{kLeft * s, kFeedTop * s, feedW, std::max(0.0f, bottom - kFeedTop * s)};
    L.update = Box{L.feed.x, L.feed.y + kLabel * s, feedW, kUpdateH * s};
    float y = L.update.y + L.update.h + kGap * s;
    for (int i = 0; i < std::min(newsCount, kMostNews); ++i) {
        if (y + kNewsH * s > bottom) break;
        L.news[i] = Box{L.feed.x, y, feedW, kNewsH * s};
        y += (kNewsH + kGap) * s;
        L.newsShown = i + 1;
    }
    return L;
}

Hit hitAt(const Layout& L, float x, float y) {
    if (L.playMore.contains(x, y)) return Hit{Part::PlayMore};
    if (L.play.contains(x, y)) return Hit{Part::Play};
    if (L.look.contains(x, y)) return Hit{Part::Look};
    if (L.noUpdate.w > 0 && L.noUpdate.contains(x, y)) return Hit{Part::NoUpdate};
    if (L.check.contains(x, y)) return Hit{Part::Check};  // 3.0.18-beta.202
    for (int i = 0; i < 2; ++i)
        if (L.channel[i].contains(x, y)) return Hit{Part::Channel, i};
    if (L.version.contains(x, y)) return Hit{Part::Version};
    if (L.update.contains(x, y)) return Hit{Part::Changelog};
    for (int i = 0; i < L.newsShown; ++i)
        if (L.news[i].contains(x, y)) return Hit{Part::News, i};
    static const Part kDock[kDockIcons] = {Part::Settings, Part::Reports, Part::Site, Part::Forum};
    for (int i = 0; i < kDockIcons; ++i)
        if (L.dock[i].contains(x, y)) return Hit{kDock[i]};
    if (L.plate.w > 0 && L.plate.contains(x, y)) return Hit{Part::Plate};
    if (L.language.contains(x, y)) return Hit{Part::Language};
    return Hit{};
}

PlayState playState(const PlayInputs& in) {
    if (in.gameRunning) return PlayState::Running;
    if (in.downloading) return PlayState::Updating;
    return in.offered ? PlayState::Update : PlayState::Play;
}

// The cards 240 wide at most, 16 apart, in the window's middle a little under its centre; the title and its note over
// them, «Пропустити» under them
FirstRun firstRunFor(const Layout& L) {
    FirstRun f;
    const float s = L.s > 0 ? L.s : 1.0f;
    const float W = float(std::max(L.w, 1)), H = float(std::max(L.h, 1));
    const float gap = 16 * s, margin = 40 * s;
    const float w = std::min(240 * s, (W - 2 * margin - 3 * gap) / 4);
    const float h = w * 9.0f / 16.0f + 86 * s;
    const float row = 4 * w + 3 * gap;
    const float x0 = (W - row) / 2;
    const float titleH = 36 * s, noteH = 22 * s, foundH = 22 * s, skipH = 34 * s;
    const float block = titleH + 8 * s + noteH + 6 * s + foundH + 18 * s + h + 22 * s + skipH;
    float top = (H - block) / 2 + 12 * s;
    top = std::max(top, std::max(L.bar.h, L.logo.y + L.logo.h) + 8 * s);  // .203: under the logo
    f.title = Box{x0, top, row, titleH};
    f.note = Box{x0, top + titleH + 8 * s, row, noteH};
    f.found = Box{x0, f.note.y + noteH + 6 * s, row, foundH};
    const float cardsY = f.found.y + foundH + 18 * s;
    for (int i = 0; i < kKindCards; ++i) f.cards[i] = Box{x0 + float(i) * (w + gap), cardsY, w, h};
    const float skipW = std::min(row, 380 * s);
    f.skip = Box{(W - skipW) / 2, cardsY + h + 22 * s, skipW, skipH};
    return f;
}

int firstRunAt(const FirstRun& f, float x, float y) {
    for (int i = 0; i < kKindCards; ++i)
        if (f.cards[i].contains(x, y)) return i;
    return f.skip.contains(x, y) ? kKindCards : -1;
}

}  // namespace uagfx::launcherview
