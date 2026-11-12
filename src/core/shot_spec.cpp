#include "core/shot_spec.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/png_write.hpp"

namespace uagfx::shotspec {

Spec parse(const std::string& target) {
    Spec s;
    const size_t bar = target.find('|');
    s.path = target.substr(0, bar);
    if (bar == std::string::npos) return s;
    int n = 0;
    if (target.size() > bar + 1 && target[bar + 1] == '/') {  // 3.0.18-beta.189: «|/N»
        if (std::sscanf(target.c_str() + bar + 2, "%d", &n) == 1 && n >= 2) s.shrink = std::min(n, 8);
        return s;
    }
    float v[4] = {};
    if (std::sscanf(target.c_str() + bar + 1, "%f %f %f %f", &v[0], &v[1], &v[2], &v[3]) == 4 && v[2] > 0 &&
        v[3] > 0) {
        s.crop = true;
        s.x = std::clamp(v[0], 0.0f, 1.0f);
        s.y = std::clamp(v[1], 0.0f, 1.0f);
        s.w = std::clamp(v[2], 0.0f, 1.0f - s.x);
        s.h = std::clamp(v[3], 0.0f, 1.0f - s.y);
    }
    return s;
}

std::string withCrop(const std::string& path, float x, float y, float w, float h) {
    char buf[96];
    std::snprintf(buf, sizeof buf, "|%.5f %.5f %.5f %.5f", x, y, w, h);
    return path + buf;
}

std::string withShrink(const std::string& path, int n) { return path + "|/" + std::to_string(n); }

std::vector<uint8_t> shrunk(const uint8_t* bgra, int width, int height, size_t pitch, int n, int& w, int& h) {
    n = std::max(1, n);
    w = std::max(1, width / n);
    h = std::max(1, height / n);
    std::vector<uint8_t> out(size_t(w) * size_t(h) * 4);
    const int across = std::min(n, width), down = std::min(n, height);
    const uint32_t count = uint32_t(across) * uint32_t(down);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            uint32_t sum[4] = {0, 0, 0, 0};
            for (int j = 0; j < down; ++j) {
                const uint8_t* row = bgra + size_t(y * n + j) * pitch + size_t(x * n) * 4;
                for (int i = 0; i < across; ++i)
                    for (int c = 0; c < 4; ++c) sum[c] += row[size_t(i) * 4 + size_t(c)];
            }
            uint8_t* p = out.data() + (size_t(y) * size_t(w) + size_t(x)) * 4;
            for (int c = 0; c < 4; ++c) p[c] = uint8_t((sum[c] + count / 2) / count);
        }
    return out;
}

void pixels(const Spec& s, int width, int height, int& x, int& y, int& w, int& h) {
    if (!s.crop) {
        x = y = 0;
        w = width;
        h = height;
        return;
    }
    x = std::clamp(int(std::lround(s.x * float(width))), 0, std::max(0, width - 1));
    y = std::clamp(int(std::lround(s.y * float(height))), 0, std::max(0, height - 1));
    w = std::clamp(int(std::lround(s.w * float(width))), 1, width - x);
    h = std::clamp(int(std::lround(s.h * float(height))), 1, height - y);
}

bool write(const std::string& target, const uint8_t* bgra, int width, int height, size_t pitch) {
    const Spec s = parse(target);
    int x = 0, y = 0, w = 0, h = 0;
    if (s.shrink > 1) {  // 3.0.18-beta.189: a report's check shot, small
        const std::vector<uint8_t> small = shrunk(bgra, width, height, pitch, s.shrink, w, h);
        return writePng(s.path, small.data(), w, h, size_t(w) * 4);
    }
    pixels(s, width, height, x, y, w, h);
    const uint8_t* from = bgra + size_t(y) * pitch + size_t(x) * 4;
    const bool raw = s.path.size() > 5 && s.path.compare(s.path.size() - 5, 5, ".bgra") == 0;
    if (!raw) return writePng(s.path, from, w, h, pitch);
    FILE* f = std::fopen(s.path.c_str(), "wb");
    if (!f) return false;
    const uint32_t head[3] = {kMagic, uint32_t(w), uint32_t(h)};
    bool ok = std::fwrite(head, sizeof head, 1, f) == 1;
    for (int r = 0; r < h && ok; ++r) ok = std::fwrite(from + size_t(r) * pitch, size_t(w) * 4, 1, f) == 1;
    return std::fclose(f) == 0 && ok;
}

bool readRaw(const std::string& bytes, std::vector<uint32_t>& argb, int& width, int& height) {
    uint32_t head[3] = {};
    if (bytes.size() < sizeof head) return false;
    std::memcpy(head, bytes.data(), sizeof head);
    if (head[0] != kMagic || head[1] == 0 || head[2] == 0 || head[1] > 4096 || head[2] > 4096) return false;
    const size_t n = size_t(head[1]) * size_t(head[2]);
    if (bytes.size() < sizeof head + n * 4) return false;
    argb.resize(n);
    std::memcpy(argb.data(), bytes.data() + sizeof head, n * 4);
    for (uint32_t& p : argb) p |= 0xFF000000u;  // B8G8R8A8 little-endian is 0xAARRGGBB; opaque
    width = int(head[1]);
    height = int(head[2]);
    return true;
}

}  // namespace uagfx::shotspec
