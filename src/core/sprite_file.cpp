#include "core/sprite_file.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace uagfx::spritefile {
namespace {

struct In {
    const std::string& s;
    size_t at = 0;
    bool ok = true;
    bool has(size_t n) const { return ok && at + n <= s.size(); }
    uint32_t u(int bytes) {
        if (!has(size_t(bytes))) {
            ok = false;
            return 0;
        }
        uint32_t v = 0;
        for (int i = bytes - 1; i >= 0; --i) v = v << 8 | static_cast<unsigned char>(s[at + size_t(i)]);
        at += size_t(bytes);
        return v;
    }
    int32_t i32() { return static_cast<int32_t>(u(4)); }
    float f32() {
        const uint32_t v = u(4);
        float f = 0;
        std::memcpy(&f, &v, 4);
        return std::isfinite(f) ? f : 1.0f;
    }
    void skip(size_t n) {
        if (!has(n)) ok = false;
        else at += n;
    }
};

constexpr int kMostSide = 2048;  // a picture's side at most (a broken file stops here)

}  // namespace

bool readSpr(const std::string& bytes, Sprite& out) {
    In in{bytes};
    if (bytes.size() < 6 || bytes[0] != 'S' || bytes[1] != 'P') return false;
    in.at = 2;
    const uint32_t minor = in.u(1), major = in.u(1);
    const uint32_t version = major << 8 | minor;
    const uint32_t paletteCount = in.u(2);
    const uint32_t trueCount = version >= 0x200 ? in.u(2) : 0;
    Sprite s;
    // the palette: the file's last 1024 bytes (red, green, blue, unused)
    if (bytes.size() < 1024 + 6) return false;
    const unsigned char* pal = reinterpret_cast<const unsigned char*>(bytes.data()) + bytes.size() - 1024;
    auto colour = [&](unsigned idx) {
        return idx == 0 ? 0u : 0xFF000000u | uint32_t(pal[idx * 4]) << 16 | uint32_t(pal[idx * 4 + 1]) << 8 |
                                   uint32_t(pal[idx * 4 + 2]);
    };
    for (uint32_t n = 0; n < paletteCount && in.ok; ++n) {
        Image im;
        im.w = int(in.u(2));
        im.h = int(in.u(2));
        if (im.w < 0 || im.h < 0 || im.w > kMostSide || im.h > kMostSide) return false;
        im.argb.assign(size_t(im.w) * size_t(im.h), 0);
        const size_t want = im.argb.size();
        if (version >= 0x201) {  // RLE: a zero and how many zeros; anything else itself
            const size_t packed = in.u(2);
            if (!in.has(packed)) return false;
            size_t p = 0, i = in.at, end = in.at + packed;
            while (i < end && p < want) {
                const unsigned char c = static_cast<unsigned char>(bytes[i++]);
                if (c == 0) {
                    const size_t run = i < end ? static_cast<unsigned char>(bytes[i++]) : 0;
                    p += std::max<size_t>(run, 1);
                } else {
                    im.argb[p++] = colour(c);
                }
            }
            in.at = end;
        } else {
            if (!in.has(want)) return false;
            for (size_t p = 0; p < want; ++p) im.argb[p] = colour(static_cast<unsigned char>(bytes[in.at + p]));
            in.at += want;
        }
        s.palette.push_back(std::move(im));
    }
    for (uint32_t n = 0; n < trueCount && in.ok; ++n) {  // alpha, blue, green, red; rows bottom up
        Image im;
        im.w = int(in.u(2));
        im.h = int(in.u(2));
        if (im.w < 0 || im.h < 0 || im.w > kMostSide || im.h > kMostSide) return false;
        const size_t want = size_t(im.w) * size_t(im.h);
        if (!in.has(want * 4)) return false;
        im.argb.assign(want, 0);
        for (int y = 0; y < im.h; ++y)
            for (int x = 0; x < im.w; ++x) {
                const size_t src = in.at + (size_t(im.h - 1 - y) * size_t(im.w) + size_t(x)) * 4;
                const auto b = [&](size_t k) { return uint32_t(static_cast<unsigned char>(bytes[src + k])); };
                im.argb[size_t(y) * size_t(im.w) + size_t(x)] = b(0) << 24 | b(3) << 16 | b(2) << 8 | b(1);
            }
        in.at += want * 4;
        s.truecolour.push_back(std::move(im));
    }
    if (!in.ok) return false;
    out = std::move(s);
    return true;
}

bool readAct(const std::string& bytes, Act& out) {
    In in{bytes};
    if (bytes.size() < 16 || bytes[0] != 'A' || bytes[1] != 'C') return false;
    in.at = 2;
    const uint32_t minor = in.u(1), major = in.u(1);
    const uint32_t version = major << 8 | minor;
    const uint32_t actions = in.u(2);
    in.skip(10);
    Act a;
    for (uint32_t n = 0; n < actions && in.ok && n < 4096; ++n) {
        const uint32_t frames = in.u(4);
        if (frames > 4096) return false;
        std::vector<Frame> list;
        for (uint32_t f = 0; f < frames && in.ok; ++f) {
            in.skip(32);  // two rectangles nobody uses
            const uint32_t layers = in.u(4);
            if (layers > 1024) return false;
            Frame fr;
            for (uint32_t l = 0; l < layers && in.ok; ++l) {
                Layer ly;
                ly.x = in.i32();
                ly.y = in.i32();
                ly.index = in.i32();
                ly.mirror = in.u(4) != 0;
                if (version >= 0x200) {
                    const uint32_t r = in.u(1), g = in.u(1), b = in.u(1), al = in.u(1);
                    ly.tint = al << 24 | r << 16 | g << 8 | b;
                    ly.scaleX = in.f32();
                    ly.scaleY = version >= 0x204 ? in.f32() : ly.scaleX;
                    in.u(4);  // the rotation
                    ly.type = int(in.u(4));
                    if (version >= 0x205) in.skip(8);  // its width and height
                }
                fr.layers.push_back(ly);
            }
            if (version >= 0x200) in.u(4);  // the frame's sound
            if (version >= 0x203) {
                const uint32_t anchors = in.u(4);
                if (anchors > 64) return false;
                in.skip(size_t(anchors) * 16);
            }
            list.push_back(std::move(fr));
        }
        a.actions.push_back(std::move(list));
    }
    if (!in.ok) return false;
    out = std::move(a);
    return true;
}

bool frameImage(const Sprite& s, const Act& a, size_t action, size_t frame, Image& out) {
    return frameImageScaled(s, a, action, frame, 1, nullptr, out);
}

bool frameImageScaled(const Sprite& s, const Act& a, size_t action, size_t frame, int factor,
                      const std::function<const Image*(int index)>& bigger, Image& out) {
    if (action >= a.actions.size() || frame >= a.actions[action].size() || factor < 1) return false;
    const Frame& fr = a.actions[action][frame];
    struct Placed {
        const Image* im;
        const Layer* ly;
        float x0, y0, w, h;
    };
    std::vector<Placed> placed;
    float left = 1e9f, top = 1e9f, right = -1e9f, bottom = -1e9f;
    const float f = float(factor);
    for (const Layer& ly : fr.layers) {
        const std::vector<Image>& list = ly.type == 1 ? s.truecolour : s.palette;
        if (ly.index < 0 || size_t(ly.index) >= list.size()) continue;
        const Image& own = list[size_t(ly.index)];
        const Image* big = ly.type == 1 || !bigger ? nullptr : bigger(ly.index);
        const Image& im = big && big->w > 0 && big->h > 0 ? *big : own;  // 3.0.11-beta.76: the pack's when given
        const float w = float(own.w) * f * std::fabs(ly.scaleX), h = float(own.h) * f * std::fabs(ly.scaleY);
        if (w < 1 || h < 1) continue;
        const float x0 = float(ly.x) * f - w / 2, y0 = float(ly.y) * f - h / 2;
        placed.push_back(Placed{&im, &ly, x0, y0, w, h});
        left = std::min(left, x0);
        top = std::min(top, y0);
        right = std::max(right, x0 + w);
        bottom = std::max(bottom, y0 + h);
    }
    if (placed.empty()) return false;
    Image o;
    o.w = std::min(kMostSide, int(std::ceil(right - left)));
    o.h = std::min(kMostSide, int(std::ceil(bottom - top)));
    o.argb.assign(size_t(o.w) * size_t(o.h), 0);
    for (const Placed& p : placed) {
        const Image& im = *p.im;
        const uint32_t tint = p.ly->tint;
        const float ta = float(tint >> 24) / 255, tr = float((tint >> 16) & 0xFF) / 255,
                    tg = float((tint >> 8) & 0xFF) / 255, tb = float(tint & 0xFF) / 255;
        for (int y = 0; y < o.h; ++y) {
            const float sy = (float(y) + 0.5f + top - p.y0) / p.h * float(im.h);
            if (sy < 0 || sy >= float(im.h)) continue;
            for (int x = 0; x < o.w; ++x) {
                float sx = (float(x) + 0.5f + left - p.x0) / p.w * float(im.w);
                if (sx < 0 || sx >= float(im.w)) continue;
                if (p.ly->mirror) sx = float(im.w) - sx;
                const int ix = std::clamp(int(sx), 0, im.w - 1), iy = std::clamp(int(sy), 0, im.h - 1);
                const uint32_t c = im.argb[size_t(iy) * size_t(im.w) + size_t(ix)];
                const float alpha = float(c >> 24) / 255 * ta;
                if (alpha <= 0) continue;
                uint32_t& d = o.argb[size_t(y) * size_t(o.w) + size_t(x)];
                const float da = float(d >> 24) / 255, oa = alpha + da * (1 - alpha);
                auto mix = [&](int shift, float t) {
                    const float src = float((c >> shift) & 0xFF) * t, dst = float((d >> shift) & 0xFF);
                    return uint32_t(std::lround((src * alpha + dst * da * (1 - alpha)) / std::max(oa, 1e-6f)));
                };
                d = uint32_t(std::lround(oa * 255)) << 24 | mix(16, tr) << 16 | mix(8, tg) << 8 | mix(0, tb);
            }
        }
    }
    out = std::move(o);
    return true;
}

Image fitted(const Image& in, int w, int h, int most) {
    Image o;
    o.w = std::max(0, w);
    o.h = std::max(0, h);
    o.argb.assign(size_t(o.w) * size_t(o.h), 0);
    if (in.w <= 0 || in.h <= 0 || o.w == 0 || o.h == 0) return o;
    float s = std::min(float(o.w) / float(in.w), float(o.h) / float(in.h));
    if (s >= 1) s = float(std::min(most, int(s)));  // whole steps: sharp pixels
    const float dw = float(in.w) * s, dh = float(in.h) * s, ox = (float(o.w) - dw) / 2, oy = (float(o.h) - dh) / 2;
    for (int y = 0; y < o.h; ++y) {
        const float sy0 = (float(y) - oy) / s, sy1 = (float(y) + 1 - oy) / s;
        if (sy1 <= 0 || sy0 >= float(in.h)) continue;
        for (int x = 0; x < o.w; ++x) {
            const float sx0 = (float(x) - ox) / s, sx1 = (float(x) + 1 - ox) / s;
            if (sx1 <= 0 || sx0 >= float(in.w)) continue;
            const int ax = std::clamp(int(sx0), 0, in.w - 1), bx = std::clamp(int(std::ceil(sx1)), ax + 1, in.w);
            const int ay = std::clamp(int(sy0), 0, in.h - 1), by = std::clamp(int(std::ceil(sy1)), ay + 1, in.h);
            float a = 0, r = 0, g = 0, b = 0;  // premultiplied average
            int n = 0;
            for (int py = ay; py < by; ++py)
                for (int px = ax; px < bx; ++px) {
                    const uint32_t c = in.argb[size_t(py) * size_t(in.w) + size_t(px)];
                    const float ca = float(c >> 24) / 255;
                    a += ca;
                    r += float((c >> 16) & 0xFF) * ca;
                    g += float((c >> 8) & 0xFF) * ca;
                    b += float(c & 0xFF) * ca;
                    ++n;
                }
            a /= float(n);
            r /= float(n);
            g /= float(n);
            b /= float(n);
            o.argb[size_t(y) * size_t(o.w) + size_t(x)] = uint32_t(std::lround(a * 255)) << 24 |
                                                          uint32_t(std::lround(r)) << 16 |
                                                          uint32_t(std::lround(g)) << 8 | uint32_t(std::lround(b));
        }
    }
    return o;
}

}  // namespace uagfx::spritefile
