#include "core/class_art.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace uagfx::classart {
namespace {

struct Picture {
    const char* file;
    int resource;  // its RCDATA id (src/win/class_art.rc)
};
constexpr Picture kPictures[] = {
    {"assassin-cross.jpg", 3001}, {"champion.jpg", 3002}, {"clown.jpg", 3003}, {"creator.jpg", 3004},
    {"gunslinger.jpg", 3005}, {"gypsy.jpg", 3006}, {"high-priest.jpg", 3007}, {"high-wizard.jpg", 3008},
    {"lord-knight.jpg", 3009}, {"ninja.jpg", 3010}, {"paladin.jpg", 3011}, {"professor.jpg", 3012},
    {"sniper.jpg", 3013}, {"soul-linker.jpg", 3014}, {"stalker.jpg", 3015}, {"star-gladiator.jpg", 3016},
    {"whitesmith.jpg", 3017}, {"acolyte.jpg", 3018}, {"alchemist.jpg", 3019}, {"archer.jpg", 3020},
    {"assassin.jpg", 3021}, {"bard.jpg", 3022}, {"blacksmith.jpg", 3023}, {"crusader.jpg", 3024},
    {"dancer.jpg", 3025}, {"hunter.jpg", 3026}, {"knight.jpg", 3027}, {"mage.jpg", 3028}, {"merchant.jpg", 3029},
    {"monk.jpg", 3030}, {"novice.jpg", 3031}, {"priest.jpg", 3032}, {"rogue.jpg", 3033}, {"sage.jpg", 3034},
    {"super-novice.jpg", 3035}, {"swordman.jpg", 3036}, {"taekwon.jpg", 3037}, {"thief.jpg", 3038},
    {"wizard.jpg", 3039},
};

struct Job {
    int id;
    const char* file;
};
// 3.0.11-beta.260 (the tester: «у тейквона і ганслінгера немає картінки»): every class its own portrait - the
// novices, the first and the second classes (the high and the baby ones as theirs), the transcendent ones, the
// expanded ones (Taekwon, Star Gladiator, Soul Linker, Gunslinger, Ninja); the mounts as the class
constexpr Job kJobs[] = {
    {0, "novice.jpg"}, {1, "swordman.jpg"}, {2, "mage.jpg"}, {3, "archer.jpg"}, {4, "acolyte.jpg"},
    {5, "merchant.jpg"}, {6, "thief.jpg"}, {7, "knight.jpg"}, {8, "priest.jpg"}, {9, "wizard.jpg"},
    {10, "blacksmith.jpg"}, {11, "hunter.jpg"}, {12, "assassin.jpg"}, {13, "knight.jpg"}, {14, "crusader.jpg"},
    {15, "monk.jpg"}, {16, "sage.jpg"}, {17, "rogue.jpg"}, {18, "alchemist.jpg"}, {19, "bard.jpg"},
    {20, "dancer.jpg"}, {21, "crusader.jpg"}, {23, "super-novice.jpg"}, {24, "gunslinger.jpg"}, {25, "ninja.jpg"},
    {4001, "novice.jpg"}, {4002, "swordman.jpg"}, {4003, "mage.jpg"}, {4004, "archer.jpg"}, {4005, "acolyte.jpg"},
    {4006, "merchant.jpg"}, {4007, "thief.jpg"}, {4008, "lord-knight.jpg"}, {4009, "high-priest.jpg"},
    {4010, "high-wizard.jpg"}, {4011, "whitesmith.jpg"}, {4012, "sniper.jpg"}, {4013, "assassin-cross.jpg"},
    {4014, "lord-knight.jpg"}, {4015, "paladin.jpg"}, {4016, "champion.jpg"}, {4017, "professor.jpg"},
    {4018, "stalker.jpg"}, {4019, "creator.jpg"}, {4020, "clown.jpg"}, {4021, "gypsy.jpg"}, {4022, "paladin.jpg"},
    {4023, "novice.jpg"}, {4024, "swordman.jpg"}, {4025, "mage.jpg"}, {4026, "archer.jpg"}, {4027, "acolyte.jpg"},
    {4028, "merchant.jpg"}, {4029, "thief.jpg"}, {4030, "knight.jpg"}, {4031, "priest.jpg"}, {4032, "wizard.jpg"},
    {4033, "blacksmith.jpg"}, {4034, "hunter.jpg"}, {4035, "assassin.jpg"}, {4036, "knight.jpg"},
    {4037, "crusader.jpg"}, {4038, "monk.jpg"}, {4039, "sage.jpg"}, {4040, "rogue.jpg"}, {4041, "alchemist.jpg"},
    {4042, "bard.jpg"}, {4043, "dancer.jpg"}, {4044, "crusader.jpg"}, {4045, "super-novice.jpg"},
    {4046, "taekwon.jpg"}, {4047, "star-gladiator.jpg"}, {4048, "star-gladiator.jpg"}, {4049, "soul-linker.jpg"},
};

uint32_t at(const std::vector<uint32_t>& argb, int w, int h, int x, int y) {
    return argb[size_t(std::clamp(y, 0, h - 1)) * size_t(w) + size_t(std::clamp(x, 0, w - 1))];
}

}  // namespace

const char* pictureFor(int jobId) {
    for (const Job& j : kJobs)
        if (j.id == jobId) return j.file;
    return nullptr;
}

int resourceOf(const char* picture) {
    if (!picture) return 0;
    for (const Picture& p : kPictures)
        if (std::strcmp(p.file, picture) == 0) return p.resource;
    return 0;
}

void cover(panel::Picture& out, const panel::Box& to, const std::vector<uint32_t>& argb, int width, int height,
           float radius, int corners) {
    if (width <= 0 || height <= 0 || argb.size() < size_t(width) * size_t(height) || to.w <= 0 || to.h <= 0) return;
    const float k = float(width) / to.w;                // source pixels a box pixel (the widths match)
    const float shown = std::min(float(height), to.h * k);  // the source rows shown: its upper part
    const float ky = shown / to.h;
    const int x0 = std::max(0, int(std::floor(to.x))), y0 = std::max(0, int(std::floor(to.y)));
    const int x1 = std::min(out.width, int(std::ceil(to.x + to.w)));
    const int y1 = std::min(out.height, int(std::ceil(to.y + to.h)));
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            const float lx = float(x) + 0.5f - to.x, ly = float(y) + 0.5f - to.y;
            float a = 1.0f;  // the rounded corners' coverage
            const bool left = lx < radius, right = lx > to.w - radius, top = ly < radius, bottom = ly > to.h - radius;
            const int corner = top && left ? 1 : top && right ? 2 : bottom && left ? 4 : bottom && right ? 8 : 0;
            if (corner && (corners & corner)) {
                const float cx = left ? radius : to.w - radius, cy = top ? radius : to.h - radius;
                a = std::clamp(radius + 0.5f - panel::length(lx - cx, ly - cy), 0.0f, 1.0f);
            }
            if (a <= 0) continue;
            const float sx = lx * k - 0.5f, sy = ly * ky - 0.5f;  // bilinear between the source's pixels
            const int ix = int(std::floor(sx)), iy = int(std::floor(sy));
            const float fx = sx - float(ix), fy = sy - float(iy);
            uint32_t c = 0;
            for (int ch = 0; ch < 24; ch += 8) {
                const auto v = [&](int px, int py) { return float((at(argb, width, height, px, py) >> ch) & 0xFF); };
                const float top2 = v(ix, iy) * (1 - fx) + v(ix + 1, iy) * fx;
                const float low2 = v(ix, iy + 1) * (1 - fx) + v(ix + 1, iy + 1) * fx;
                const float value = (top2 * (1 - fy) + low2 * fy) * a;  // premultiplied
                c |= uint32_t(std::clamp(int(panel::roundOf(value)), 0, 255)) << ch;
            }
            out.px[size_t(y) * size_t(out.width) + size_t(x)] = c | uint32_t(panel::roundOf(a * 255.0f)) << 24;
        }
}

void centre(panel::Picture& out, const panel::Box& to, const panel::Picture& pic) {
    const int ox = int(std::lround(to.x + (to.w - float(pic.width)) / 2));
    const int oy = int(std::lround(to.y + (to.h - float(pic.height)) / 2));
    for (int y = 0; y < pic.height; ++y)
        for (int x = 0; x < pic.width; ++x) {
            const int tx = ox + x, ty = oy + y;
            if (tx < 0 || ty < 0 || tx >= out.width || ty >= out.height) continue;
            const uint32_t s = pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
            const uint32_t sa = s >> 24;
            if (!sa) continue;
            uint32_t& d = out.px[size_t(ty) * size_t(out.width) + size_t(tx)];
            uint32_t r = 0;
            for (int ch = 0; ch < 32; ch += 8)  // premultiplied: s + d (1 - sa)
                r |= std::min(255u, ((s >> ch) & 0xFF) + ((d >> ch) & 0xFF) * (255 - sa) / 255) << ch;
            d = r;
        }
}

}  // namespace uagfx::classart
