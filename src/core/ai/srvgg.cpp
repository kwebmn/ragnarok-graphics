#include "core/ai/srvgg.hpp"

#include <algorithm>
#include <cstring>

namespace uagfx::ai {
namespace {

uint32_t u32(const std::vector<uint8_t>& b, size_t at) {
    return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 | uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24;
}

}  // namespace

bool parseModel(const std::vector<uint8_t>& file, Model& m, std::string& error) {
    m = Model{};
    if (file.size() < 24 || std::memcmp(file.data(), "SRVG", 4) != 0) {
        error = "not an SRVG model file";
        return false;
    }
    m.inChannels = int(u32(file, 4));
    m.features = int(u32(file, 8));
    m.middle = int(u32(file, 12));
    m.scale = int(u32(file, 16));
    if (m.inChannels != 3 || m.features < 1 || m.features > 256 || m.middle < 0 || m.middle > 256 || m.scale < 1 ||
        m.scale > 8 || u32(file, 20) != 1) {
        error = "unexpected SRVG header";
        return false;
    }
    // Order as written: per layer bias then weight for convolutions, the slopes for PReLU.
    size_t n = 0;
    const auto conv = [&](int in, int out) {
        Model::Conv c;
        c.in = in;
        c.out = out;
        c.bias = n;
        n += size_t(out);
        c.weight = n;
        n += size_t(out) * size_t(in) * 9;
        m.convs.push_back(c);
    };
    const auto act = [&]() {
        m.prelu.push_back(n);
        n += size_t(m.features);
    };
    conv(m.inChannels, m.features);
    act();
    for (int i = 0; i < m.middle; ++i) {
        conv(m.features, m.features);
        act();
    }
    conv(m.features, 3 * m.scale * m.scale);
    if (file.size() != 24 + n * 4) {
        error = "SRVG model of the wrong size";
        return false;
    }
    m.data.resize(n);
    std::memcpy(m.data.data(), file.data() + 24, n * 4);  // little-endian floats, as on x86
    return true;
}

bool mixModels(const Model& strong, const Model& weak, int denoise, Model& out, std::string& error) {
    if (strong.features != weak.features || strong.middle != weak.middle || strong.scale != weak.scale ||
        strong.inChannels != weak.inChannels || strong.data.size() != weak.data.size()) {
        error = "the two models are not the same network";
        return false;
    }
    const float d = float(std::clamp(denoise, 0, 100)) / 100.0f;
    out = strong;
    if (d == 1.0f) return true;  // exactly the strong one, and the weak one at 0 below
    for (size_t i = 0; i < out.data.size(); ++i)
        out.data[i] = d == 0.0f ? weak.data[i] : d * strong.data[i] + (1.0f - d) * weak.data[i];
    return true;
}

void convolve(const Model& m, const Model::Conv& c, const float* in, int width, int height, float* out, int first,
              int last, Border border) {
    const size_t plane = size_t(width) * size_t(height);
    const float* bias = m.data.data() + c.bias;
    const float* weight = m.data.data() + c.weight;
    for (int o = first; o < last; ++o) {
        float* dst = out + size_t(o) * plane;
        std::fill(dst, dst + plane, bias[o]);
        for (int i = 0; i < c.in; ++i) {
            const float* src = in + size_t(i) * plane;
            const float* k = weight + (size_t(o) * size_t(c.in) + size_t(i)) * 9;
            for (int y = 0; y < height; ++y) {
                float* row = dst + size_t(y) * size_t(width);
                for (int ky = 0; ky < 3; ++ky) {
                    int sy = y + ky - 1;
                    if (sy < 0 || sy >= height) {
                        if (border == Border::Zero) continue;  // nothing past the edge
                        sy = (sy + height) % height;           // the other side
                    }
                    const float* s = src + size_t(sy) * size_t(width);
                    const float k0 = k[ky * 3], k1 = k[ky * 3 + 1], k2 = k[ky * 3 + 2];
                    // x = 0 and x = width - 1 have one neighbour outside; the inside runs without checks.
                    const float before = border == Border::Wrap ? s[width - 1] : 0.0f;  // what x = -1 reads
                    const float after = border == Border::Wrap ? s[0] : 0.0f;           // and x = width
                    if (width == 1) {
                        row[0] += k0 * before + k1 * s[0] + k2 * after;
                        continue;
                    }
                    row[0] += k0 * before + k1 * s[0] + k2 * s[1];
                    for (int x = 1; x + 1 < width; ++x) row[x] += k0 * s[x - 1] + k1 * s[x] + k2 * s[x + 1];
                    row[width - 1] += k0 * s[width - 2] + k1 * s[width - 1] + k2 * after;
                }
            }
        }
    }
}

void prelu(const float* slopes, float* planes, int channels, int width, int height) {
    const size_t plane = size_t(width) * size_t(height);
    for (int c = 0; c < channels; ++c) {
        float* p = planes + size_t(c) * plane;
        const float a = slopes[c];
        for (size_t i = 0; i < plane; ++i)
            if (p[i] < 0.0f) p[i] *= a;
    }
}

void shuffleAdd(const float* planes, const float* rgb, int width, int height, int scale, float* out) {
    const int ow = width * scale, oh = height * scale;
    const size_t plane = size_t(width) * size_t(height), oplane = size_t(ow) * size_t(oh);
    for (int c = 0; c < 3; ++c)
        for (int y = 0; y < oh; ++y)
            for (int x = 0; x < ow; ++x) {
                const int sy = y / scale, sx = x / scale, i = y % scale, j = x % scale;
                const size_t from = size_t(c * scale * scale + i * scale + j) * plane + size_t(sy) * size_t(width) +
                                    size_t(sx);
                const float v = planes[from] + rgb[size_t(c) * plane + size_t(sy) * size_t(width) + size_t(sx)];
                out[size_t(c) * oplane + size_t(y) * size_t(ow) + size_t(x)] = std::min(1.0f, std::max(0.0f, v));
            }
}

void upscale(const Model& m, const float* rgb, int width, int height, float* out, Border border) {
    const size_t plane = size_t(width) * size_t(height);
    const int widest = std::max(m.features, 3 * m.scale * m.scale);
    std::vector<float> a(plane * size_t(widest)), b(plane * size_t(widest));
    convolve(m, m.convs[0], rgb, width, height, a.data(), 0, m.convs[0].out, border);
    prelu(m.data.data() + m.prelu[0], a.data(), m.features, width, height);
    for (int l = 1; l <= m.middle; ++l) {
        convolve(m, m.convs[size_t(l)], a.data(), width, height, b.data(), 0, m.convs[size_t(l)].out, border);
        prelu(m.data.data() + m.prelu[size_t(l)], b.data(), m.features, width, height);
        a.swap(b);
    }
    const Model::Conv& last = m.convs.back();
    convolve(m, last, a.data(), width, height, b.data(), 0, last.out, border);
    shuffleAdd(b.data(), rgb, width, height, m.scale, out);
}

}  // namespace uagfx::ai
