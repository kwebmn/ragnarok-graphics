#include "win/pack_frames.hpp"

#include <algorithm>

#include "core/pack_source.hpp"
#include "win/pack_pictures.hpp"

namespace uagfx::win::packframes {

Frames of(const std::string& spr, const spritefile::Sprite& sp, const std::vector<int>& used, int want) {
    Frames out;
    if (packpictures::iconsStamp() <= 0 || used.empty()) return out;
    std::vector<pack::Picture> cells;
    if (!pack::spriteFrames(reinterpret_cast<const uint8_t*>(spr.data()), spr.size(), cells, nullptr, &used))
        return out;
    int factor = want;
    for (const pack::Picture& c : cells) {
        std::vector<uint32_t> argb;
        int w = 0, h = 0;
        if (c.width <= 0 || c.frame < 0 || size_t(c.frame) >= sp.palette.size() ||
            !packpictures::ofSprite(c.key, argb, w, h) || w % c.width || w / c.width < 2 ||
            h != c.height * (w / c.width))
            continue;
        const int f = w / c.width;
        if (factor && f != factor) continue;  // one size for them all
        factor = f;
        const spritefile::Image& own = sp.palette[size_t(c.frame)];
        spritefile::Image& b = out.big[c.frame];  // the frame in its cell's top left, f times
        b.w = own.w * f;
        b.h = own.h * f;
        b.argb.resize(size_t(b.w) * size_t(b.h));
        for (int y = 0; y < b.h; ++y)
            std::copy_n(argb.begin() + std::ptrdiff_t(size_t(y) * size_t(w)), b.w,
                        b.argb.begin() + std::ptrdiff_t(size_t(y) * size_t(b.w)));
    }
    out.factor = out.big.empty() ? 0 : factor;
    return out;
}

std::vector<int> usedBy(const spritefile::Sprite& sp, const spritefile::Act& ac, size_t action, size_t frame) {
    std::vector<int> used;
    if (action >= ac.actions.size() || frame >= ac.actions[action].size()) return used;
    for (const spritefile::Layer& ly : ac.actions[action][frame].layers)
        if (ly.type == 0 && ly.index >= 0 && size_t(ly.index) < sp.palette.size() &&
            std::find(used.begin(), used.end(), ly.index) == used.end())
            used.push_back(ly.index);
    return used;
}

}  // namespace uagfx::win::packframes
