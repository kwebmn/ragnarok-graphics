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
    {"assassin-cross.jpg", 3001}, {"champion.jpg", 3002},    {"clown.jpg", 3003},       {"creator.jpg", 3004},
    {"gunslinger.jpg", 3005},     {"gypsy.jpg", 3006},       {"high-priest.jpg", 3007}, {"high-wizard.jpg", 3008},
    {"lord-knight.jpg", 3009},    {"ninja.jpg", 3010},       {"paladin.jpg", 3011},     {"professor.jpg", 3012},
    {"sniper.jpg", 3013},         {"soul-linker.jpg", 3014}, {"stalker.jpg", 3015},     {"star-gladiator.jpg", 3016},
    {"whitesmith.jpg", 3017},
};

struct Job {
    int id;
    const char* file;
};
// The second class, the transcendent one, the baby (and the mounts): the transcendent's portrait
constexpr Job kJobs[] = {
    {7, "lord-knight.jpg"},     {13, "lord-knight.jpg"},    {4008, "lord-knight.jpg"},    {4014, "lord-knight.jpg"},
    {4030, "lord-knight.jpg"},  {4036, "lord-knight.jpg"},  {8, "high-priest.jpg"},       {4009, "high-priest.jpg"},
    {4031, "high-priest.jpg"},  {9, "high-wizard.jpg"},     {4010, "high-wizard.jpg"},    {4032, "high-wizard.jpg"},
    {10, "whitesmith.jpg"},     {4011, "whitesmith.jpg"},   {4033, "whitesmith.jpg"},     {11, "sniper.jpg"},
    {4012, "sniper.jpg"},       {4034, "sniper.jpg"},       {12, "assassin-cross.jpg"},   {4013, "assassin-cross.jpg"},
    {4035, "assassin-cross.jpg"}, {14, "paladin.jpg"},      {21, "paladin.jpg"},          {4015, "paladin.jpg"},
    {4022, "paladin.jpg"},      {4037, "paladin.jpg"},      {4044, "paladin.jpg"},        {15, "champion.jpg"},
    {4016, "champion.jpg"},     {4038, "champion.jpg"},     {16, "professor.jpg"},        {4017, "professor.jpg"},
    {4039, "professor.jpg"},    {17, "stalker.jpg"},        {4018, "stalker.jpg"},        {4040, "stalker.jpg"},
    {18, "creator.jpg"},        {4019, "creator.jpg"},      {4041, "creator.jpg"},        {19, "clown.jpg"},
    {4020, "clown.jpg"},        {4042, "clown.jpg"},        {20, "gypsy.jpg"},            {4021, "gypsy.jpg"},
    {4043, "gypsy.jpg"},        {24, "gunslinger.jpg"},     {25, "ninja.jpg"},            {4047, "star-gladiator.jpg"},
    {4048, "star-gladiator.jpg"}, {4049, "soul-linker.jpg"},
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
