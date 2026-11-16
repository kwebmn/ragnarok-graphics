#include "core/sharp_art.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>

#include "core/full_colour.hpp"
#include "core/xbr.hpp"

namespace uagfx::sharpart {

Facts factsOf(const uint32_t* argb, int w, int h) {
    Facts f;
    if (!argb || w <= 0 || h <= 0) return f;
    const size_t n = size_t(w) * size_t(h);
    size_t clear = 0, painted = 0, flat = 0, rim = 0, rimDark = 0, pairs = 0, smooth = 0, big = 0;
    f.binary = true;
    const auto at = [&](int x, int y) { return argb[size_t(y) * size_t(w) + size_t(x)]; };
    const auto brightest = [](uint32_t p) { return int(std::max({(p >> 16) & 0xFF, (p >> 8) & 0xFF, p & 0xFF})); };
    // the game's level: 32 a channel (A1R5G5B5 - a step of 8 or 9 of 255), 16 with an alpha of its own (A4R4G4B4,
    // every channel a multiple of 17)
    bool sixteen = true;
    for (size_t i = 0; i < n && sixteen; ++i) {
        const uint32_t p = argb[i];
        if (p >> 24) sixteen = ((p >> 16) & 0xFF) % 17 == 0 && ((p >> 8) & 0xFF) % 17 == 0 && (p & 0xFF) % 17 == 0;
    }
    const int level = sixteen ? kLevel16 : kLevel32;
    const auto step = [](uint32_t a, uint32_t b) {
        int most = 0;
        for (int shift = 0; shift < 24; shift += 8)
            most = std::max(most, std::abs(int((a >> shift) & 0xFF) - int((b >> shift) & 0xFF)));
        return most;
    };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const uint32_t p = at(x, y);
            const uint32_t a = p >> 24;
            if (a == 0) {
                ++clear;
                continue;
            }
            if (a != 255) f.binary = false;
            ++painted;
            // beside a painted texel of its own colour; beside the key
            bool same = false, edge = false;
            const int around[4][2] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
            for (const auto& q : around) {
                if (q[0] < 0 || q[1] < 0 || q[0] >= w || q[1] >= h) continue;
                const uint32_t o = at(q[0], q[1]);
                if (!(o >> 24)) edge = true;
                else if (o == p) same = true;
            }
            if (same) ++flat;
            if (edge) {
                ++rim;
                if (brightest(p) < kRimDarkUnder) ++rimDark;
            }
            // its unlike painted neighbours to the right and below (each pair once)
            const int next[2][2] = {{x + 1, y}, {x, y + 1}};
            for (const auto& q : next) {
                if (q[0] >= w || q[1] >= h) continue;
                const uint32_t o = at(q[0], q[1]);
                if (!(o >> 24) || (o & 0xFFFFFFu) == (p & 0xFFFFFFu)) continue;
                ++pairs;
                const int d = step(p, o);
                if (d <= level) ++smooth;
                else if (d > kBigStep) ++big;
            }
        }
    const auto clamp = [](size_t v) { return int(std::min<size_t>(v, 0x7FFFFFFF)); };
    f.painted = clamp(painted);
    f.texels = clamp(n);
    f.flatTexels = clamp(flat);
    f.pairs = clamp(pairs);
    f.smoothPairs = clamp(smooth);
    f.bigPairs = clamp(big);
    f.rim = clamp(rim);
    f.rimDarkTexels = clamp(rimDark);
    f.clear = int(clear * 100 / n);
    f.flat = painted ? int(flat * 100 / painted) : 0;
    f.smooth = pairs ? int(smooth * 100 / pairs) : 0;
    f.big = pairs ? int(big * 100 / pairs) : 0;
    f.rimDark = rim ? int(rimDark * 100 / rim) : 0;
    return f;
}

bool painted(const uint32_t* argb, int w, int h) {
    if (!argb || std::min(w, h) < kLeastSide || std::max(w, h) > kMostSide) return false;
    const Facts f = factsOf(argb, w, h);
    // by the texels themselves (97.9 % of them keyed is past 97)
    using Count = long long;
    const Count clear = Count(f.texels) - f.painted;
    if (!f.binary || f.painted < kLeastPainted || clear * 100 < Count(kClearFrom) * f.texels ||
        clear * 100 > Count(kClearTo) * f.texels || Count(f.flatTexels) * 100 < Count(kFlatFrom) * f.painted)
        return false;
    if (Count(f.smoothPairs) * 100 > Count(kSmoothMost) * f.pairs) return false;  // shaded softly
    // its edge beside the key its own drawn outline - not light fading into the black the game keyed out
    if (Count(f.rimDarkTexels) * 100 <= Count(kRimDarkMost) * f.rim) return true;
    // a dark edge all the same: an outline drawn in black, when every step in the picture is a hard one (one in two
    // hundred may be a level apart)
    return Count(f.smoothPairs) * 200 <= f.pairs && Count(f.bigPairs) * 100 >= Count(kBigFrom) * f.pairs;
}

sharpframes::Copy makeCopy(const uint32_t* argb, int w, int h) {
    sharpframes::Copy c;
    if (!painted(argb, w, h)) return c;
    c.width = w * kScale;
    c.height = h * kScale;
    c.levels.push_back(xbr::scale4(argb, w, h));
    int lw = c.width, lh = c.height;
    while (lw > w || lh > h) {  // twice, then the picture's own size
        std::vector<uint32_t> next;
        int nw = 0, nh = 0;
        full_colour::halve(c.levels.back(), lw, lh, next, nw, nh);  // weighted by alpha: no dark rims
        if (next.empty() || (nw == lw && nh == lh)) break;
        c.levels.push_back(std::move(next));
        lw = nw;
        lh = nh;
    }
    return c;
}

CopyJob::CopyJob(const uint32_t* argb, int w, int h) {
    if (!painted(argb, w, h)) return;
    source_.assign(argb, argb + size_t(w) * size_t(h));
    w_ = w, h_ = h;
    copy_.width = w * kScale;
    copy_.height = h * kScale;
    copy_.levels.emplace_back(size_t(copy_.width) * size_t(copy_.height));
    stage_ = Stage::Rows;
}

bool CopyJob::step(int texels) {
    constexpr int kMargin = 3;  // rows more on either side of a strip: xBR reads two round a texel
    if (stage_ == Stage::Rows) {
        const int rows = std::clamp(texels / std::max(1, w_), 1, h_ - row_);
        const int from = std::max(0, row_ - kMargin), to = std::min(h_, row_ + rows + kMargin);
        const std::vector<uint32_t> strip = xbr::scale4(source_.data() + size_t(from) * size_t(w_), w_, to - from);
        const size_t wide = size_t(w_) * kScale;
        std::copy(strip.begin() + ptrdiff_t(size_t(row_ - from) * kScale * wide),
                  strip.begin() + ptrdiff_t(size_t(row_ - from + rows) * kScale * wide),
                  copy_.levels[0].begin() + ptrdiff_t(size_t(row_) * kScale * wide));
        row_ += rows;
        if (row_ < h_) return false;
        source_.clear();
        source_.shrink_to_fit();
        levelW_ = copy_.width, levelH_ = copy_.height;
        stage_ = levelW_ > w_ || levelH_ > h_ ? Stage::Halves : Stage::Done;
        return stage_ == Stage::Done;
    }
    if (stage_ == Stage::Halves) {  // as makeCopy: twice, then the picture's own size
        std::vector<uint32_t> next;
        int nw = 0, nh = 0;
        full_colour::halve(copy_.levels.back(), levelW_, levelH_, next, nw, nh);
        if (next.empty() || (nw == levelW_ && nh == levelH_)) {
            stage_ = Stage::Done;
            return true;
        }
        copy_.levels.push_back(std::move(next));
        levelW_ = nw, levelH_ = nh;
        if (levelW_ <= w_ && levelH_ <= h_) stage_ = Stage::Done;
    }
    return stage_ == Stage::Done;
}

sharpframes::Copy CopyJob::take() {
    sharpframes::Copy out;
    if (stage_ != Stage::Done) return out;
    out = std::move(copy_);
    copy_ = sharpframes::Copy{};
    return out;
}

}  // namespace uagfx::sharpart
