#include "core/ai/rrdb.hpp"

#include <algorithm>
#include <cstring>

namespace uagfx::ai {
namespace {

uint32_t u32(const std::vector<uint8_t>& b, size_t at) {
    return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 | uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24;
}

// The convolutions in the file's order: the first, B x 3 x 5 of the blocks, the body's, up1, up2, hr, last.
int convFirst() { return 0; }
int convDense(int block, int rdb, int k) { return 1 + (block * 3 + rdb) * 5 + k; }
int convBody(const RrdbModel& m) { return 1 + m.blocks * 15; }

}  // namespace

int bufChannels(const RrdbModel& m, Buf b) {
    switch (b) {
        case Buf::Img:
        case Buf::Out: return 3;
        case Buf::Grow: return 4 * m.growth;
        case Buf::Count: return 0;
        default: return m.features;
    }
}

int bufScale(Buf b) {
    switch (b) {
        case Buf::Up1: return 2;
        case Buf::Up2:
        case Buf::Up3:
        case Buf::Out: return 4;
        default: return 1;
    }
}

bool parseRrdb(const std::vector<uint8_t>& file, RrdbModel& m, std::string& error) {
    m = RrdbModel{};
    if (file.size() < 24 || std::memcmp(file.data(), "RRDB", 4) != 0) {
        error = "not an RRDB model file";
        return false;
    }
    m.features = int(u32(file, 4));
    m.growth = int(u32(file, 8));
    m.blocks = int(u32(file, 12));
    const int scale = int(u32(file, 16));
    if (m.features < 1 || m.features > 256 || m.growth < 1 || m.growth > 256 || m.blocks < 1 || m.blocks > 64 ||
        scale != 4 || u32(file, 20) != 1) {
        error = "unexpected RRDB header";
        return false;
    }
    Model& n = m.net;
    n.inChannels = 3;
    n.features = m.features;
    n.middle = 0;
    n.scale = 4;
    size_t at = 0;
    const auto conv = [&](int in, int out) {
        Model::Conv c;
        c.in = in;
        c.out = out;
        c.bias = at;
        at += size_t(out);
        c.weight = at;
        at += size_t(out) * size_t(in) * 9;
        n.convs.push_back(c);
    };
    const int f = m.features, g = m.growth;
    conv(3, f);
    for (int b = 0; b < m.blocks; ++b)
        for (int r = 0; r < 3; ++r)
            for (int k = 0; k < 5; ++k) conv(f + k * g, k < 4 ? g : f);
    for (int i = 0; i < 4; ++i) conv(f, f);  // body, up1, up2, hr
    conv(f, 3);
    if (file.size() != 24 + at * 4) {
        error = "RRDB model of the wrong size";
        return false;
    }
    n.data.resize(at + size_t(f));
    std::memcpy(n.data.data(), file.data() + 24, at * 4);  // little-endian floats, as on x86
    // The scales folded in: a dense block's fifth convolution times 0.2; the third block's of each group times 0.04
    // (0.2 of its block's end and 0.2 of the group's, rrdbProgram adds the rest).
    for (int b = 0; b < m.blocks; ++b)
        for (int r = 0; r < 3; ++r) {
            const Model::Conv& c = n.convs[size_t(convDense(b, r, 4))];
            const float s = r < 2 ? 0.2f : 0.04f;
            const size_t count = size_t(c.out) * (1 + size_t(c.in) * 9);  // bias and weights follow each other
            for (size_t i = 0; i < count; ++i) n.data[c.bias + i] *= s;
        }
    n.prelu.push_back(at);  // leaky ReLU as PReLU with every slope 0.2
    std::fill(n.data.begin() + long(at), n.data.end(), 0.2f);
    return true;
}

std::vector<Op> rrdbProgram(const RrdbModel& m) {
    std::vector<Op> ops;
    const int f = m.features, g = m.growth;
    Op first;
    first.conv = convFirst();
    first.inA = Buf::Img;
    first.split = 3;
    first.out = Buf::First;
    ops.push_back(first);
    // One dense block: x (f channels of `in`) -> `out`, its fifth convolution with `res` added (and `res2`).
    const auto dense = [&](int block, int r, Buf in, Buf out, bool fifth, Buf res2) {
        for (int k = 0; k < 5; ++k) {
            Op o;
            o.conv = convDense(block, r, k);
            o.inA = in;
            o.split = f;
            o.inB = k > 0 ? Buf::Grow : Buf::Count;
            if (k < 4) {
                o.out = Buf::Grow;
                o.outOffset = k * g;
                o.lrelu = true;
            } else {
                o.out = out;
                o.res1 = in;
                o.res1Fifth = fifth;
                o.res2 = res2;
            }
            ops.push_back(o);
        }
    };
    Buf in = Buf::First;
    for (int b = 0; b < m.blocks; ++b) {
        Buf free[2];
        int n = 0;
        for (Buf c : {Buf::P, Buf::Q, Buf::R})
            if (c != in && n < 2) free[n++] = c;
        dense(b, 0, in, free[0], false, Buf::Count);
        dense(b, 1, free[0], free[1], false, Buf::Count);
        dense(b, 2, free[1], free[0], true, in);  // 0.04 conv + 0.2 x + the group's input
        in = free[0];
    }
    Buf body = Buf::P;
    for (Buf c : {Buf::P, Buf::Q, Buf::R})
        if (c != in) {
            body = c;
            break;
        }
    Op o;
    o.conv = convBody(m);
    o.inA = in;
    o.split = f;
    o.out = body;
    o.res1 = Buf::First;
    ops.push_back(o);
    const Buf chain[4][2] = {{body, Buf::Up1}, {Buf::Up1, Buf::Up2}, {Buf::Up2, Buf::Up3}, {Buf::Up3, Buf::Out}};
    for (int i = 0; i < 4; ++i) {
        Op u;
        u.conv = convBody(m) + 1 + i;
        u.inA = chain[i][0];
        u.split = f;
        u.out = chain[i][1];
        u.lrelu = i < 3;
        u.upsample = i < 2;
        u.scale = bufScale(u.out);
        ops.push_back(u);
    }
    return ops;
}

void runRrdb(const RrdbModel& m, const float* rgb, int width, int height, float* out, Border border) {
    const Model& n = m.net;
    std::vector<float> bufs[size_t(Buf::Count)];
    const auto planeOf = [&](Buf b) { return size_t(width * bufScale(b)) * size_t(height * bufScale(b)); };
    for (int b = 0; b < int(Buf::Count); ++b)
        bufs[b].assign(size_t(bufChannels(m, Buf(b))) * planeOf(Buf(b)), 0.0f);
    std::memcpy(bufs[int(Buf::Img)].data(), rgb, size_t(3) * planeOf(Buf::Img) * 4);
    std::vector<float> input, result;
    for (const Op& o : rrdbProgram(m)) {
        const Model::Conv& c = n.convs[size_t(o.conv)];
        const int w = width * o.scale, h = height * o.scale;
        const size_t plane = size_t(w) * size_t(h);
        const int iw = o.upsample ? w / 2 : w, ih = o.upsample ? h / 2 : h;
        const size_t inPlane = size_t(iw) * size_t(ih);
        // The input gathered: inA's first `split` channels, then inB's, enlarged when the step asks.
        input.assign(size_t(c.in) * plane, 0.0f);
        for (int ch = 0; ch < c.in; ++ch) {
            const float* src = ch < o.split ? bufs[int(o.inA)].data() + size_t(o.inOffset + ch) * inPlane
                                            : bufs[int(o.inB)].data() + size_t(ch - o.split) * inPlane;
            float* dst = input.data() + size_t(ch) * plane;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    const size_t from = o.upsample ? size_t(y / 2) * size_t(iw) + size_t(x / 2)
                                                   : size_t(y) * size_t(w) + size_t(x);
                    dst[size_t(y) * size_t(w) + size_t(x)] = src[from];
                }
        }
        result.assign(size_t(c.out) * plane, 0.0f);
        convolve(n, c, input.data(), w, h, result.data(), 0, c.out, border);
        if (o.lrelu) prelu(n.data.data() + n.prelu[0], result.data(), c.out, w, h);
        for (int ch = 0; ch < c.out; ++ch)
            for (size_t i = 0; i < plane; ++i) {
                float v = result[size_t(ch) * plane + i];
                if (o.res1 != Buf::Count) v += bufs[int(o.res1)][size_t(ch) * plane + i] * (o.res1Fifth ? 0.2f : 1.0f);
                if (o.res2 != Buf::Count) v += bufs[int(o.res2)][size_t(ch) * plane + i];
                bufs[int(o.out)][size_t(o.outOffset + ch) * plane + i] = v;
            }
    }
    const std::vector<float>& last = bufs[int(Buf::Out)];
    for (size_t i = 0; i < last.size(); ++i) out[i] = std::clamp(last[i], 0.0f, 1.0f);
}

std::vector<Tile> rrdbTiles(int width, int height) {
    std::vector<Tile> tiles;
    if (width <= kRrdbTile + 2 * kRrdbMargin && height <= kRrdbTile + 2 * kRrdbMargin) {
        Tile t;
        t.width = t.inWidth = width;
        t.height = t.inHeight = height;
        tiles.push_back(t);
        return tiles;
    }
    for (int y = 0; y < height; y += kRrdbTile)
        for (int x = 0; x < width; x += kRrdbTile) {
            Tile t;
            t.x = x;
            t.y = y;
            t.width = std::min(kRrdbTile, width - x);
            t.height = std::min(kRrdbTile, height - y);
            t.left = t.top = kRrdbMargin;
            t.inWidth = t.width + 2 * kRrdbMargin;
            t.inHeight = t.height + 2 * kRrdbMargin;
            tiles.push_back(t);
        }
    return tiles;
}

void tileInput(const float* planes, int channels, int width, int height, const Tile& t, Border border,
               std::vector<float>& out) {
    out.assign(size_t(channels) * size_t(t.inWidth) * size_t(t.inHeight), 0.0f);
    for (int c = 0; c < channels; ++c)
        for (int y = 0; y < t.inHeight; ++y)
            for (int x = 0; x < t.inWidth; ++x) {
                int sx = t.x - t.left + x, sy = t.y - t.top + y;
                const bool outside = sx < 0 || sy < 0 || sx >= width || sy >= height;
                if (outside && border == Border::Zero) continue;
                if (border == Border::Wrap) {
                    sx = (sx % width + width) % width;
                    sy = (sy % height + height) % height;
                } else {
                    sx = std::clamp(sx, 0, width - 1);
                    sy = std::clamp(sy, 0, height - 1);
                }
                out[(size_t(c) * size_t(t.inHeight) + size_t(y)) * size_t(t.inWidth) + size_t(x)] =
                    planes[(size_t(c) * size_t(height) + size_t(sy)) * size_t(width) + size_t(sx)];
            }
}

RrdbLayout rrdbLayout(const RrdbModel& m, int width, int height) {
    RrdbLayout l;
    for (int b = 0; b < int(Buf::Count); ++b) {
        const Buf buf = Buf(b);
        if (buf == Buf::Out) continue;
        l.at[b] = uint32_t(l.words);
        const uint64_t s = uint64_t(bufScale(buf));
        l.words += uint64_t(bufChannels(m, buf)) * uint64_t(width) * s * uint64_t(height) * s;
    }
    l.at[int(Buf::Out)] = l.at[int(Buf::Up2)];
    return l;
}

void convConstants(const RrdbModel& m, const Op& o, const Tile& t, const RrdbLayout& l, uint32_t border, int row,
                   int end, uint32_t k[16]) {
    const Model::Conv& c = m.net.convs[size_t(o.conv)];
    const uint32_t w = uint32_t(t.inWidth * o.scale), h = uint32_t(t.inHeight * o.scale);
    k[0] = w;
    k[1] = h;
    k[2] = uint32_t(c.in);
    k[3] = uint32_t(c.out);
    k[4] = uint32_t(c.bias);
    k[5] = uint32_t(c.weight);
    k[6] = o.lrelu ? uint32_t(m.net.prelu[0]) : 0xFFFFFFFFu;
    k[7] = uint32_t(row);
    k[8] = uint32_t(end);
    k[9] = border | (o.upsample ? 4u : 0u) | (o.res1Fifth ? 8u : 0u);
    k[10] = l.at[int(o.inA)];
    k[11] = o.inB != Buf::Count ? l.at[int(o.inB)] : 0u;
    k[12] = uint32_t(o.split);
    k[13] = l.at[int(o.out)] + uint32_t(o.outOffset) * w * h;
    k[14] = o.res1 != Buf::Count ? l.at[int(o.res1)] : 0xFFFFFFFFu;
    k[15] = o.res2 != Buf::Count ? l.at[int(o.res2)] : 0xFFFFFFFFu;
}

void placeConstants(const Tile& t, const RrdbLayout& l, uint32_t pitch, uint32_t at, int phase, uint32_t k[16]) {
    std::memset(k, 0, 16 * 4);
    k[0] = uint32_t(t.inWidth * 4);
    k[1] = uint32_t(t.inHeight * 4);
    k[2] = uint32_t(t.left * 4);
    k[3] = uint32_t(t.top * 4);
    k[4] = uint32_t(t.width * 4);
    k[5] = uint32_t(t.height * 4);
    k[6] = uint32_t(t.x * 4);
    k[7] = uint32_t(t.y * 4);
    k[8] = pitch;
    k[9] = at;
    k[10] = l.at[int(Buf::Out)];
    k[11] = uint32_t(phase);
}

void convReference(const uint32_t* k, const float* weights, std::vector<float>& u) {
    const uint32_t width = k[0], height = k[1], in = k[2], outC = k[3];
    const uint32_t border = k[9] & 3;
    const bool halfIn = (k[9] & 4) != 0;
    const uint32_t inWidth = halfIn ? width / 2 : width;
    const uint32_t plane = width * height, inPlane = halfIn ? inWidth * (height / 2) : plane;
    std::vector<float> out(size_t(outC) * plane);
    for (uint32_t y = k[7]; y < k[8] && y < height; ++y)
        for (uint32_t x = 0; x < width; ++x)
            for (uint32_t o = 0; o < outC; ++o) {
                float a = weights[k[4] + o];
                for (uint32_t i = 0; i < in; ++i) {
                    const uint32_t inAt = i < k[12] ? k[10] + i * inPlane : k[11] + (i - k[12]) * inPlane;
                    for (int q = 0; q < 9; ++q) {
                        const int lx = int(x) + q % 3 - 1, ly = int(y) + q / 3 - 1;
                        const bool outside = lx < 0 || ly < 0 || lx >= int(width) || ly >= int(height);
                        uint32_t sx = uint32_t(lx + int(width)) % width, sy = uint32_t(ly + int(height)) % height;
                        if (border == 2) {
                            sx = uint32_t(std::min(std::max(lx, 0), int(width) - 1));
                            sy = uint32_t(std::min(std::max(ly, 0), int(height) - 1));
                        }
                        const uint32_t at = halfIn ? (sy / 2) * inWidth + sx / 2 : sy * width + sx;
                        const float v = border == 1 && outside ? 0.0f : u[inAt + at];
                        a += weights[k[5] + (o * in + i) * 9 + uint32_t(q)] * v;
                    }
                }
                if (k[6] != 0xFFFFFFFFu && a < 0.0f) a *= weights[k[6] + o];
                const uint32_t p = o * plane + y * width + x;
                if (k[14] != 0xFFFFFFFFu) a += u[k[14] + p] * ((k[9] & 8) != 0 ? 0.2f : 1.0f);
                if (k[15] != 0xFFFFFFFFu) a += u[k[15] + p];
                out[p] = a;
            }
    // Written after all of it is read: the GPU's writes go where no read of the same step looks.
    for (uint32_t y = k[7]; y < k[8] && y < height; ++y)
        for (uint32_t o = 0; o < outC; ++o)
            for (uint32_t x = 0; x < width; ++x) u[k[13] + o * plane + y * width + x] = out[o * plane + y * width + x];
}

void placeReference(const uint32_t* k, const std::vector<float>& u, std::vector<uint32_t>& image) {
    const uint32_t plane = k[0] * k[1];
    for (uint32_t y = 0; y < k[5]; ++y)
        for (uint32_t x = 0; x < k[4]; ++x) {
            const uint32_t src = (y + k[3]) * k[0] + x + k[2];
            float v[3];
            for (uint32_t c = 0; c < 3; ++c) v[c] = std::min(1.0f, std::max(0.0f, u[k[10] + c * plane + src]));
            uint32_t& d = image[k[9] + (k[7] + y) * k[8] + k[6] + x];
            if (k[11] == 0) {
                d = 0xFF000000u | uint32_t(v[0] * 255.0f + 0.5f) << 16 | uint32_t(v[1] * 255.0f + 0.5f) << 8 |
                    uint32_t(v[2] * 255.0f + 0.5f);
            } else {
                d = (v[0] + v[1] + v[2] >= 1.5f ? 0xFF000000u : 0u) | (d & 0x00FFFFFFu);
            }
        }
}

}  // namespace uagfx::ai
