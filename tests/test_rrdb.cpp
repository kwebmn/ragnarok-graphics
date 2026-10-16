#include <cmath>
#include <cstring>
#include <random>
#include <vector>

#include "core/ai/rrdb.hpp"
#include "test.hpp"

using namespace uagfx::ai;

namespace {

// A small RRDB file (F 8, G 4, 2 blocks) of random weights, and the weights as written, for the direct reference.
std::vector<uint8_t> smallModel(std::vector<std::vector<float>>& convs, std::vector<int>& ins, std::vector<int>& outs) {
    const int f = 8, g = 4, blocks = 2;
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> d(-0.15f, 0.15f);
    const auto add = [&](int in, int out) {
        std::vector<float> v(size_t(out) * (1 + size_t(in) * 9));
        for (float& x : v) x = d(rng);
        convs.push_back(v);
        ins.push_back(in);
        outs.push_back(out);
    };
    add(3, f);
    for (int b = 0; b < blocks; ++b)
        for (int r = 0; r < 3; ++r)
            for (int k = 0; k < 5; ++k) add(f + k * g, k < 4 ? g : f);
    for (int i = 0; i < 4; ++i) add(f, f);
    add(f, 3);
    std::vector<uint8_t> file(24);
    std::memcpy(file.data(), "RRDB", 4);
    const uint32_t head[5] = {uint32_t(f), uint32_t(g), uint32_t(blocks), 4, 1};
    std::memcpy(file.data() + 4, head, 20);
    for (const auto& v : convs) {
        const size_t at = file.size();
        file.resize(at + v.size() * 4);
        std::memcpy(file.data() + at, v.data(), v.size() * 4);
    }
    return file;
}

using Planes = std::vector<float>;

// The network written out the way Real-ESRGAN's RRDBNet does it, zeros past the edge.
Planes conv(const std::vector<float>& w, int in, int out, const Planes& x, int width, int height) {
    Planes y(size_t(out) * size_t(width) * size_t(height));
    for (int o = 0; o < out; ++o)
        for (int py = 0; py < height; ++py)
            for (int px = 0; px < width; ++px) {
                float s = w[size_t(o)];
                for (int i = 0; i < in; ++i)
                    for (int k = 0; k < 9; ++k) {
                        const int sx = px + k % 3 - 1, sy = py + k / 3 - 1;
                        if (sx < 0 || sy < 0 || sx >= width || sy >= height) continue;
                        s += w[size_t(out) + (size_t(o) * size_t(in) + size_t(i)) * 9 + size_t(k)] *
                             x[(size_t(i) * size_t(height) + size_t(sy)) * size_t(width) + size_t(sx)];
                    }
                y[(size_t(o) * size_t(height) + size_t(py)) * size_t(width) + size_t(px)] = s;
            }
    return y;
}
void lrelu(Planes& p) {
    for (float& v : p) v = v > 0 ? v : 0.2f * v;
}
Planes up2(const Planes& p, int c, int w, int h) {
    Planes r(size_t(c) * 4 * size_t(w) * size_t(h));
    for (int k = 0; k < c; ++k)
        for (int y = 0; y < 2 * h; ++y)
            for (int x = 0; x < 2 * w; ++x)
                r[(size_t(k) * size_t(2 * h) + size_t(y)) * size_t(2 * w) + size_t(x)] =
                    p[(size_t(k) * size_t(h) + size_t(y / 2)) * size_t(w) + size_t(x / 2)];
    return r;
}

}  // namespace

TEST(rrdb_program_matches_the_network_as_written) {
    std::vector<std::vector<float>> w;
    std::vector<int> ins, outs;
    const std::vector<uint8_t> file = smallModel(w, ins, outs);
    RrdbModel m;
    std::string error;
    CHECK(parseRrdb(file, m, error));
    const int width = 7, height = 5;
    Planes img(size_t(3 * width * height));
    for (size_t i = 0; i < img.size(); ++i) img[i] = float((i * 37) % 100) / 100.0f;
    // The reference.
    size_t ci = 0;
    const auto c = [&](const Planes& x, int ww, int hh) {
        Planes y = conv(w[ci], ins[ci], outs[ci], x, ww, hh);
        ++ci;
        return y;
    };
    Planes feat = c(img, width, height);
    Planes x = feat;
    for (int b = 0; b < 2; ++b) {
        Planes start = x;
        for (int r = 0; r < 3; ++r) {
            Planes all = x;
            for (int k = 0; k < 4; ++k) {
                Planes o = c(all, width, height);
                lrelu(o);
                all.insert(all.end(), o.begin(), o.end());
            }
            Planes o5 = c(all, width, height);
            for (size_t i = 0; i < o5.size(); ++i) o5[i] = o5[i] * 0.2f + x[i];
            x = o5;
        }
        for (size_t i = 0; i < x.size(); ++i) x[i] = x[i] * 0.2f + start[i];
    }
    Planes body = c(x, width, height);
    for (size_t i = 0; i < body.size(); ++i) body[i] += feat[i];
    Planes u1 = c(up2(body, 8, width, height), 2 * width, 2 * height);
    lrelu(u1);
    Planes u2 = c(up2(u1, 8, 2 * width, 2 * height), 4 * width, 4 * height);
    lrelu(u2);
    Planes hr = c(u2, 4 * width, 4 * height);
    lrelu(hr);
    Planes last = c(hr, 4 * width, 4 * height);
    // The program.
    Planes out(size_t(3 * 16 * width * height));
    runRrdb(m, img.data(), width, height, out.data(), Border::Zero);
    float worst = 0;
    for (size_t i = 0; i < out.size(); ++i)
        worst = std::max(worst, std::fabs(out[i] - std::min(1.0f, std::max(0.0f, last[i]))));
    CHECK(worst < 1e-4f);
    CHECK_EQ(int(rrdbProgram(m).size()), 1 + 2 * 15 + 1 + 4);
}

TEST(rrdb_tiles_cover_the_picture_with_margins) {
    const std::vector<Tile> one = rrdbTiles(100, 116);
    CHECK_EQ(int(one.size()), 1);
    CHECK(one[0].left == 0 && one[0].inWidth == 100 && one[0].inHeight == 116);
    const std::vector<Tile> many = rrdbTiles(256, 130);
    CHECK_EQ(int(many.size()), 3 * 2);
    int covered = 0;
    for (const Tile& t : many) {
        covered += t.width * t.height;
        CHECK(t.left == kRrdbMargin && t.inWidth == t.width + 2 * kRrdbMargin);
    }
    CHECK_EQ(covered, 256 * 130);
    // A tile's input past the picture: the other side (a ground texture), the edge, or nothing.
    std::vector<float> planes(4 * 3);
    for (size_t i = 0; i < planes.size(); ++i) planes[i] = float(i + 1);
    Tile t;
    t.width = t.height = 2;
    t.left = t.top = 1;
    t.inWidth = t.inHeight = 4;
    std::vector<float> out;
    tileInput(planes.data(), 1, 4, 3, t, Border::Wrap, out);
    CHECK(out[0] == planes[2 * 4 + 3]);  // (-1, -1) is (3, 2)
    tileInput(planes.data(), 1, 4, 3, t, Border::Clamp, out);
    CHECK(out[0] == planes[0]);
    tileInput(planes.data(), 1, 4, 3, t, Border::Zero, out);
    CHECK(out[0] == 0.0f && out[5] == planes[0]);
}
