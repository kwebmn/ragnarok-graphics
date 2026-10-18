#include "core/ai/chain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "core/ai/picture_metrics.hpp"
#include "core/ai/texture_cache.hpp"
#include "core/sharpen.hpp"

namespace uagfx::ai {
namespace {

constexpr int kTilePad = 4;  // texels of a tile wrapped about it for the network (16 in its picture)

int ch(uint32_t p, int c) { return int(p >> (8 * c) & 0xFF); }  // 0 blue, 1 green, 2 red, 3 alpha
bool shows(uint32_t p) { return (p >> 24) >= 128; }
float lum(uint32_t p) { return 0.299f * float(ch(p, 2)) + 0.587f * float(ch(p, 1)) + 0.114f * float(ch(p, 0)); }
uint32_t withRgb(uint32_t p, float r, float g, float b) {
    const auto byte = [](float v) { return uint32_t(std::clamp(int(std::lround(v)), 0, 255)); };
    return (p & 0xFF000000u) | byte(r) << 16 | byte(g) << 8 | byte(b);
}

// A picture texel's original texel (any ratio).
size_t sourceOf(int x, int y, int W, int H, int w, int h) {
    const int ox = std::min(w - 1, int(int64_t(x) * w / W)), oy = std::min(h - 1, int(int64_t(y) * h / H));
    return size_t(oy) * size_t(w) + size_t(ox);
}

float cubic(float a, float b, float c, float d, float t) {  // Catmull-Rom
    return b + 0.5f * t * (c - a + t * (2.0f * a - 5.0f * b + 4.0f * c - d + t * (3.0f * (b - c) + d - a)));
}

// A plane (w x h) at W x H, bicubic between texel centres, its edges held.
std::vector<float> bicubic(const std::vector<float>& v, int w, int h, int W, int H) {
    std::vector<float> out(size_t(W) * size_t(H));
    const auto at = [&](int x, int y) {
        return v[size_t(std::clamp(y, 0, h - 1)) * size_t(w) + size_t(std::clamp(x, 0, w - 1))];
    };
    for (int y = 0; y < H; ++y) {
        const float fy = (float(y) + 0.5f) * float(h) / float(H) - 0.5f;
        const int y0 = int(std::floor(fy));
        const float ty = fy - float(y0);
        for (int x = 0; x < W; ++x) {
            const float fx = (float(x) + 0.5f) * float(w) / float(W) - 0.5f;
            const int x0 = int(std::floor(fx));
            const float tx = fx - float(x0);
            float rows[4];
            for (int k = 0; k < 4; ++k) {
                const int yy = y0 - 1 + k;
                rows[k] = cubic(at(x0 - 1, yy), at(x0, yy), at(x0 + 1, yy), at(x0 + 2, yy), tx);
            }
            out[size_t(y) * size_t(W) + size_t(x)] = cubic(rows[0], rows[1], rows[2], rows[3], ty);
        }
    }
    return out;
}

std::vector<float> planeOf(const uint32_t* p, int w, int h, int c) {
    std::vector<float> out(size_t(w) * size_t(h));
    for (size_t i = 0; i < out.size(); ++i) out[i] = float(ch(p[i], c));
    return out;
}

}  // namespace

std::string chainText(const Chain& c) {
    std::string t = c.model + (c.scale == 2 ? " x2" : " x4");
    const auto number = [&t](const char* name, float v) {
        char s[48];
        std::snprintf(s, sizeof s, " %s=%.2f", name, double(v));
        t += s;
    };
    if (c.deblock) t += " deblock";
    if (c.repairKey) t += " key";
    if (c.tiles) t += " tiles";
    if (c.matchTone) t += " tone";
    if (c.grain > 0) number("grain", c.grain);
    if (c.sharpen > 0) number("sharpen", c.sharpen);
    if (c.cleanAlpha) t += " alpha";
    if (!c.blendModel.empty()) t += " blend=" + c.blendModel;
    if (c.original > 0) number("original", c.original);
    if (c.denoise >= 0) t += " denoise=" + std::to_string(c.denoise);
    return t;
}

bool parseChain(const std::string& text, Chain& out) {
    std::vector<std::string> words;
    for (size_t at = 0; at < text.size();) {
        const size_t end = std::min(text.find(' ', at), text.size());
        if (end > at) words.push_back(text.substr(at, end - at));
        at = end + 1;
    }
    if (words.empty() || words[0].find('=') != std::string::npos) return false;
    Chain c;
    c.model = words[0];
    size_t i = 1;
    if (i < words.size() && words[i].size() == 2 && words[i][0] == 'x') {
        if (words[i] != "x4" && words[i] != "x2") return false;
        c.scale = words[i][1] - '0';
        ++i;
    }
    for (; i < words.size(); ++i) {
        const std::string& w = words[i];
        const size_t eq = w.find('=');
        const std::string name = w.substr(0, eq), value = eq == std::string::npos ? "" : w.substr(eq + 1);
        const float number = std::strtof(value.c_str(), nullptr);
        if (w == "deblock") c.deblock = true;
        else if (w == "key") c.repairKey = true;
        else if (w == "tiles") c.tiles = true;
        else if (w == "tone") c.matchTone = true;
        else if (w == "alpha") c.cleanAlpha = true;
        else if (name == "grain" && !value.empty()) c.grain = std::clamp(number, 0.0f, 1.0f);
        else if (name == "sharpen" && !value.empty()) c.sharpen = std::clamp(number, 0.0f, 1.0f);
        else if (name == "original" && !value.empty()) c.original = std::clamp(number, 0.0f, 1.0f);
        else if (name == "blend" && !value.empty()) c.blendModel = value;
        else if (name == "denoise" && !value.empty()) c.denoise = std::clamp(std::atoi(value.c_str()), 0, 100);
        else return false;
    }
    out = c;
    return true;
}

void deblock(std::vector<uint32_t>& argb, int w, int h) {
    if (argb.size() < size_t(w) * size_t(h)) return;
    for (int pass = 0; pass < 2; ++pass) {  // the blocks' left edges, then their top ones
        const int lines = pass ? w : h, len = pass ? h : w;
        for (int a = 0; a < lines; ++a)
            for (int k = 8; k + 1 < len; k += 8) {
                const auto at = [&](int b) -> uint32_t& {
                    return pass ? argb[size_t(b) * size_t(w) + size_t(a)] : argb[size_t(a) * size_t(w) + size_t(b)];
                };
                uint32_t &p0 = at(k - 2), &p1 = at(k - 1), &p2 = at(k), &p3 = at(k + 1);
                if (!shows(p0) || !shows(p1) || !shows(p2) || !shows(p3)) continue;
                const float step = std::fabs(lum(p2) - lum(p1));
                if (std::fabs(lum(p0) - lum(p1)) > 3 || std::fabs(lum(p2) - lum(p3)) > 3 || step == 0 || step > 12)
                    continue;
                float b[3], c[3];
                for (int i = 0; i < 3; ++i) {
                    const float d = float(ch(p2, i) - ch(p1, i)) / 3.0f;
                    b[i] = float(ch(p1, i)) + d;
                    c[i] = float(ch(p2, i)) - d;
                }
                p1 = withRgb(p1, b[2], b[1], b[0]);
                p2 = withRgb(p2, c[2], c[1], c[0]);
            }
    }
}

void repairKey(std::vector<uint32_t>& argb, int w, int h) {
    const size_t n = std::min(argb.size(), size_t(w) * size_t(h));
    for (size_t i = 0; i < n; ++i)
        if (ch(argb[i], 2) >= 220 && ch(argb[i], 0) >= 220 && ch(argb[i], 1) <= 50) argb[i] = 0x00FF00FFu;
}

void matchTone(std::vector<uint32_t>& pic, int W, int H, const uint32_t* orig, int w, int h) {
    const auto stats = [](const uint32_t* p, size_t n, double mean[3], double spread[3]) {
        double sum[3] = {}, sq[3] = {};
        size_t k = 0;
        for (size_t i = 0; i < n; ++i) {
            if (!shows(p[i])) continue;
            float lab[3];
            oklabOf(p[i], lab);
            for (int c = 0; c < 3; ++c) sum[c] += lab[c], sq[c] += double(lab[c]) * lab[c];
            ++k;
        }
        for (int c = 0; c < 3; ++c) {
            mean[c] = k ? sum[c] / double(k) : 0;
            spread[c] = k ? std::sqrt(std::max(0.0, sq[c] / double(k) - mean[c] * mean[c])) : 0;
        }
        return k;
    };
    const size_t n = std::min(pic.size(), size_t(W) * size_t(H));
    double pm[3], ps[3], om[3], os[3];
    if (!stats(pic.data(), n, pm, ps) || !stats(orig, size_t(w) * size_t(h), om, os)) return;
    for (size_t i = 0; i < n; ++i) {
        if (!shows(pic[i])) continue;
        float lab[3];
        oklabOf(pic[i], lab);
        for (int c = 0; c < 3; ++c) {
            const double k = ps[c] > 1e-4 ? std::clamp(os[c] / ps[c], 0.5, 2.0) : 1.0;
            lab[c] = float((lab[c] - pm[c]) * k + om[c]);
        }
        pic[i] = argbOfOklab(lab, pic[i] >> 24);
    }
}

void restoreGrain(std::vector<uint32_t>& pic, int W, int H, const uint32_t* orig, int w, int h, float strength) {
    if (pic.size() < size_t(W) * size_t(H) || w < 1 || h < 1) return;
    for (int c = 0; c < 3; ++c) {
        const std::vector<float> o = planeOf(orig, w, h, c);
        std::vector<float> high(o.size(), 0.0f);  // the original less its 3x3 mean: its grain
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                if (!shows(orig[size_t(y) * size_t(w) + size_t(x)])) continue;
                float sum = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int yy = std::clamp(y + dy, 0, h - 1), xx = std::clamp(x + dx, 0, w - 1);
                        sum += o[size_t(yy) * size_t(w) + size_t(xx)];
                    }
                high[size_t(y) * size_t(w) + size_t(x)] = o[size_t(y) * size_t(w) + size_t(x)] - sum / 9.0f;
            }
        const std::vector<float> up = bicubic(high, w, h, W, H);
        for (size_t i = 0; i < size_t(W) * size_t(H); ++i) {
            if (!shows(pic[i])) continue;
            const float v = float(ch(pic[i], c)) + strength * up[i];
            pic[i] = (pic[i] & ~(0xFFu << (8 * c))) | uint32_t(std::clamp(int(std::lround(v)), 0, 255)) << (8 * c);
        }
    }
}

void sharpenAdaptive(std::vector<uint32_t>& pic, int W, int H, float amount) {
    if (pic.size() < size_t(W) * size_t(H)) return;
    const std::vector<uint32_t> was = pic;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const size_t i = size_t(y) * size_t(W) + size_t(x);
            if (!shows(was[i])) continue;
            const auto near = [&](int xx, int yy) {  // a neighbour, or the texel itself past the edge or the key
                if (xx < 0 || yy < 0 || xx >= W || yy >= H) return was[i];
                const uint32_t v = was[size_t(yy) * size_t(W) + size_t(xx)];
                return shows(v) ? v : was[i];
            };
            const uint32_t n = near(x, y - 1), s = near(x, y + 1), wv = near(x - 1, y), e = near(x + 1, y);
            float out[3];
            for (int c = 0; c < 3; ++c)
                out[c] = 255.0f * float(casSharpen(ch(was[i], c) / 255.0, ch(n, c) / 255.0, ch(s, c) / 255.0,
                                                   ch(wv, c) / 255.0, ch(e, c) / 255.0, amount));
            pic[i] = withRgb(was[i], out[2], out[1], out[0]);
        }
}

void cleanAlpha(std::vector<uint32_t>& pic, int W, int H, const uint32_t* orig, int w, int h) {
    if (pic.size() < size_t(W) * size_t(H) || w < 1 || h < 1) return;
    std::vector<uint8_t> alpha(size_t(W) * size_t(H));
    if (W % w == 0 && H % h == 0 && W / w == H / h) {  // as the driver enlarges the key (bilinear, cut at a half)
        std::vector<float> a(size_t(w) * size_t(h));
        for (size_t i = 0; i < a.size(); ++i) a[i] = shows(orig[i]) ? 1.0f : 0.0f;
        enlargeAlpha(a.data(), w, h, W / w, alpha);
    } else {
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                alpha[size_t(y) * size_t(W) + size_t(x)] = shows(orig[sourceOf(x, y, W, H, w, h)]) ? 255 : 0;
    }
    for (size_t i = 0; i < alpha.size(); ++i) pic[i] = (pic[i] & 0x00FFFFFFu) | uint32_t(alpha[i]) << 24;
    const std::vector<uint32_t> was = pic;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const size_t i = size_t(y) * size_t(W) + size_t(x);
            if (!alpha[i]) continue;
            const auto key = [&](int xx, int yy) {
                return xx >= 0 && yy >= 0 && xx < W && yy < H && !alpha[size_t(yy) * size_t(W) + size_t(xx)];
            };
            if (!key(x - 1, y) && !key(x + 1, y) && !key(x, y - 1) && !key(x, y + 1)) continue;
            const uint32_t p = was[i], o = orig[sourceOf(x, y, W, H, w, h)];
            const int lean = (ch(p, 2) - ch(p, 1)) + (ch(p, 0) - ch(p, 1));
            const int was0 = (ch(o, 2) - ch(o, 1)) + (ch(o, 0) - ch(o, 1));
            if (shows(o) && ch(p, 2) > ch(p, 1) && ch(p, 0) > ch(p, 1) && lean - was0 > 60)
                pic[i] = (p & 0xFF000000u) | (o & 0x00FFFFFFu);  // the edge without the fringe
        }
}

void blendByEdges(std::vector<uint32_t>& pic, const std::vector<uint32_t>& other, int W, int H, const uint32_t* orig,
                  int w, int h) {
    if (pic.size() < size_t(W) * size_t(H) || other.size() < pic.size() || w < 1 || h < 1) return;
    std::vector<float> edge(size_t(w) * size_t(h));
    const auto l = [&](int x, int y) {
        return lum(orig[size_t(std::clamp(y, 0, h - 1)) * size_t(w) + size_t(std::clamp(x, 0, w - 1))]);
    };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float gx = l(x + 1, y - 1) + 2 * l(x + 1, y) + l(x + 1, y + 1) - l(x - 1, y - 1) - 2 * l(x - 1, y) -
                             l(x - 1, y + 1);
            const float gy = l(x - 1, y + 1) + 2 * l(x, y + 1) + l(x + 1, y + 1) - l(x - 1, y - 1) - 2 * l(x, y - 1) -
                             l(x + 1, y - 1);
            edge[size_t(y) * size_t(w) + size_t(x)] = std::clamp(std::sqrt(gx * gx + gy * gy) / 128.0f, 0.0f, 1.0f);
        }
    const std::vector<float> e = bicubic(edge, w, h, W, H);
    for (size_t i = 0; i < size_t(W) * size_t(H); ++i) {
        const float k = std::clamp(e[i], 0.0f, 1.0f);
        float rgb[3];
        for (int c = 0; c < 3; ++c) rgb[c] = float(ch(pic[i], c)) * (1 - k) + float(ch(other[i], c)) * k;
        pic[i] = withRgb(pic[i], rgb[2], rgb[1], rgb[0]);
    }
}

void mixOriginal(std::vector<uint32_t>& pic, int W, int H, const uint32_t* orig, int w, int h, float amount) {
    if (pic.size() < size_t(W) * size_t(H) || w < 1 || h < 1) return;
    std::vector<float> up[3];
    for (int c = 0; c < 3; ++c) up[c] = bicubic(planeOf(orig, w, h, c), w, h, W, H);
    for (size_t i = 0; i < size_t(W) * size_t(H); ++i) {
        float rgb[3];
        for (int c = 0; c < 3; ++c) rgb[c] = float(ch(pic[i], c)) * (1 - amount) + up[c][i] * amount;
        pic[i] = withRgb(pic[i], rgb[2], rgb[1], rgb[0]);
    }
}

bool runChain(const Chain& c, const uint32_t* argb, int w, int h, atlas_tiles::Grid grid, const Enlarge& enlarge,
              std::vector<uint32_t>& out, int& W, int& H, std::string& error) {
    if (!argb || w < 1 || h < 1) {
        error = "no picture";
        return false;
    }
    std::vector<uint32_t> src(argb, argb + size_t(w) * size_t(h));
    if (c.deblock) deblock(src, w, h);
    if (c.repairKey) repairKey(src, w, h);
    const bool tiles = c.tiles && (grid.cols > 1 || grid.rows > 1);
    const auto net = [&](const std::string& model, std::vector<uint32_t>& to) {
        if (!tiles) {
            if (!enlarge(model, c.denoise, src, w, h, to, error)) return false;
            if (to.size() == size_t(w) * 4 * size_t(h) * 4) return true;
            error = "the network's picture is not 4 times the texture";
            return false;
        }
        to.assign(size_t(w) * 4 * size_t(h) * 4, 0);
        std::vector<uint32_t> big;
        for (int row = 0; row < grid.rows; ++row)
            for (int col = 0; col < grid.cols; ++col) {
                int tw = 0, th = 0;
                const std::vector<uint32_t> tile =
                    atlas_tiles::cutTile(src.data(), w, h, grid, col, row, kTilePad, tw, th);
                if (!enlarge(model, c.denoise, tile, tw, th, big, error)) return false;
                if (big.size() != size_t(tw) * 4 * size_t(th) * 4) {
                    error = "the network's picture is not 4 times the tile";
                    return false;
                }
                atlas_tiles::placeTile(to, w * 4, h * 4, grid, col, row, big, tw * 4, th * 4, kTilePad * 4);
            }
        return true;
    };
    if (!net(c.model, out)) return false;
    W = w * 4;
    H = h * 4;
    if (!c.blendModel.empty()) {
        std::vector<uint32_t> other;
        if (!net(c.blendModel, other)) return false;
        blendByEdges(out, other, W, H, src.data(), w, h);
    }
    if (c.matchTone) matchTone(out, W, H, src.data(), w, h);
    if (c.grain > 0) restoreGrain(out, W, H, src.data(), w, h, c.grain);
    if (c.sharpen > 0) sharpenAdaptive(out, W, H, c.sharpen);
    if (c.cleanAlpha) cleanAlpha(out, W, H, src.data(), w, h);
    if (c.original > 0) mixOriginal(out, W, H, src.data(), w, h, c.original);
    if (c.scale == 2) halve(out, W, H);
    return true;
}

}  // namespace uagfx::ai
