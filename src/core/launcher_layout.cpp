#include "core/launcher_layout.hpp"

#include <algorithm>

namespace uagfx::launcherview {
namespace {

// At scale 1 (the design 1280 x 720)
constexpr float kBar = 64, kLeft = 22, kRight = 26, kBottom = 26, kFeedTop = 80, kFeedW = 380, kLabel = 28;
constexpr float kUpdateH = 150, kNewsH = 84, kGap = 10, kIcon = 64, kIconGap = 12, kDockBottom = 22;
constexpr float kPlayW = 336, kLookH = 52, kButtonH = 64, kMoreW = 56, kVersionH = 18, kPlateH = 44, kPlateW = 560;

}  // namespace

Layout layoutFor(int width, int height, float scale, int newsCount, bool plate) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    const float W = float(std::max(width, 1)), H = float(std::max(height, 1));
    L.s = s;
    L.w = width;
    L.h = height;
    L.bar = Box{0, 0, W, kBar * s};
    L.logo = Box{kLeft * s, 12 * s, 260 * s, 40 * s};
    L.language = Box{W - kRight * s - 56 * s, 18 * s, 56 * s, 28 * s};
    L.server = Box{L.language.x - 12 * s - 280 * s, 16 * s, 280 * s, 32 * s};
    // the play column: the version at the bottom, the button over it, the look line over that
    const float playW = std::min(kPlayW * s, W * 0.4f);
    const float px = W - kRight * s - playW;
    L.version = Box{px, H - kBottom * s - kVersionH * s, playW, kVersionH * s};
    const float by = L.version.y - 8 * s - kButtonH * s;
    L.play = Box{px, by, playW - kMoreW * s, kButtonH * s};
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

}  // namespace uagfx::launcherview
