#include "core/ai/texture_cache.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace uagfx::ai {
namespace {

void put32(std::vector<uint8_t>& b, uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (8 * i)));
}
uint32_t get32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

}  // namespace

TextureKey textureKey(const uint32_t* argb, int width, int height) {
    uint64_t h = 1469598103934665603ull;
    const auto mix = [&h](uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            h ^= (v >> (8 * i)) & 0xFF;
            h *= 1099511628211ull;
        }
    };
    mix(uint32_t(width));
    mix(uint32_t(height));
    for (size_t i = 0, n = size_t(width) * size_t(height); i < n; ++i) mix(argb[i]);
    return TextureKey{h, width, height};
}

std::string keyName(const TextureKey& k) {
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(k.hash));
    return std::to_string(k.width) + "x" + std::to_string(k.height) + "\\" + hex;
}

std::string entryName(const std::string& folder, const TextureKey& k) { return entryName(folder, keyName(k)); }

std::string entryName(const std::string& folder, const std::string& key) {
    return entryFolder(folder) + key + ".tex";
}

std::string entryFolder(const std::string& folder) { return "ai\\" + folder + "\\"; }

std::string netFolder(const std::string& model, int denoise, int scale) {
    const std::string size = scale == 2 ? "-x2" : "";
    if (model == "anime") return "realesr-animevideov3-r2" + size;
    return "realesr-general-x4v3-dn" + std::to_string(std::clamp(denoise, 0, 100)) + "-r2" + size;
}

std::string kindTag(Kind kind, const std::string& key) {
    return std::string("ai\\@kind\\") + kindName(kind) + "\\" + key;
}

std::string mapTag(const std::string& map, const std::string& key) { return "ai\\@map\\" + map + "\\" + key; }

namespace {

// "256x256\0123456789abcdef": a size and 16 hex digits.
bool isKey(const std::string& s) {
    const size_t slash = s.find('\\');
    if (slash == std::string::npos || s.size() != slash + 17) return false;
    const size_t x = s.find('x');
    if (x == 0 || x == std::string::npos || x + 1 >= slash) return false;
    for (size_t i = 0; i < slash; ++i)
        if (i != x && !std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    for (size_t i = slash + 1; i < s.size(); ++i)
        if (!std::isxdigit(static_cast<unsigned char>(s[i])) || std::isupper(static_cast<unsigned char>(s[i])))
            return false;
    return true;
}

// The first part of `rest` up to a backslash, and rest after it; false when there is none.
bool split(std::string& rest, std::string& first) {
    const size_t slash = rest.find('\\');
    if (slash == std::string::npos || slash == 0) return false;
    first = rest.substr(0, slash);
    rest = rest.substr(slash + 1);
    return true;
}

}  // namespace

ParsedEntry parseEntry(const std::string& name) {
    ParsedEntry e;
    if (name.compare(0, 3, "ai\\") != 0) return e;
    std::string rest = name.substr(3), first;
    if (!split(rest, first)) return e;
    if (first == "@kind") {
        std::string kind;
        if (split(rest, kind) && parseKind(kind, e.kind) && isKey(rest)) {
            e.type = ParsedEntry::KindTag;
            e.key = rest;
        }
        return e;
    }
    if (first == "@map") {
        std::string map;
        if (split(rest, map) && isKey(rest)) {
            e.type = ParsedEntry::MapTag;
            e.map = map;
            e.key = rest;
        }
        return e;
    }
    if (rest.size() > 4 && rest.compare(rest.size() - 4, 4, ".tex") == 0 && isKey(rest.substr(0, rest.size() - 4))) {
        e.type = ParsedEntry::Picture;
        e.folder = first;
        e.key = rest.substr(0, rest.size() - 4);
    }
    return e;
}

std::string mapName(const char* raw, size_t size) {
    std::string out;
    for (size_t i = 0; i < size && raw[i]; ++i) {
        const char c = char(std::tolower(static_cast<unsigned char>(raw[i])));
        if (c == '.') break;
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '@' && c != '-') return "";
        out += c;
    }
    return out.size() <= 40 ? out : "";
}

std::vector<uint8_t> encodeTexture(const uint32_t* argb, int width, int height) {
    std::vector<uint8_t> b;
    b.reserve(16 + size_t(width) * size_t(height) * 4);
    for (const char c : {'U', 'A', 'T', 'X'}) b.push_back(uint8_t(c));  // (insert of a list: a false GCC warning)
    put32(b, 1);
    put32(b, uint32_t(width));
    put32(b, uint32_t(height));
    for (size_t i = 0, n = size_t(width) * size_t(height); i < n; ++i) put32(b, argb[i]);
    return b;
}

bool decodeTexture(const std::vector<uint8_t>& blob, std::vector<uint32_t>& argb, int& width, int& height) {
    if (blob.size() < 16 || std::memcmp(blob.data(), "UATX", 4) != 0 || get32(blob.data() + 4) != 1) return false;
    const uint32_t w = get32(blob.data() + 8), h = get32(blob.data() + 12);
    if (w == 0 || h == 0 || w > 16384 || h > 16384 || blob.size() != 16 + size_t(w) * h * 4) return false;
    width = int(w);
    height = int(h);
    argb.resize(size_t(w) * h);
    for (size_t i = 0; i < argb.size(); ++i) argb[i] = get32(blob.data() + 16 + i * 4);
    return true;
}

bool hasTransparency(const uint32_t* argb, int width, int height) {
    for (size_t i = 0, n = size_t(width) * size_t(height); i < n; ++i)
        if ((argb[i] >> 24) < 128) return true;
    return false;
}

void prepareInput(const uint32_t* argb, int width, int height, std::vector<float>& planes) {
    const size_t n = size_t(width) * size_t(height);
    planes.assign(n * 4, 0.0f);
    std::vector<uint8_t> known(n);
    std::vector<float> rgb(n * 3);
    for (size_t i = 0; i < n; ++i) {
        const uint32_t p = argb[i];
        known[i] = (p >> 24) >= 128;
        rgb[i * 3] = float((p >> 16) & 0xFF) / 255.0f;
        rgb[i * 3 + 1] = float((p >> 8) & 0xFF) / 255.0f;
        rgb[i * 3 + 2] = float(p & 0xFF) / 255.0f;
        planes[n * 3 + i] = known[i] ? 1.0f : 0.0f;
    }
    // Hidden texels take the average of their known neighbours, ring by ring inwards: each ring is the hidden texels
    // next to a known one, all of it from the texels known before it (a walk outwards from them, linear in the
    // texels - sprite cache cells are mostly hidden).
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    std::vector<uint8_t> queued(known);
    std::vector<size_t> ring, next;
    const auto addNeighbours = [&](size_t i, std::vector<size_t>& to) {
        const int x = int(i % size_t(width)), y = int(i / size_t(width));
        for (int d = 0; d < 4; ++d) {
            const int xx = x + dx[d], yy = y + dy[d];
            if (xx < 0 || yy < 0 || xx >= width || yy >= height) continue;
            const size_t j = size_t(yy) * size_t(width) + size_t(xx);
            if (queued[j]) continue;
            queued[j] = 1;
            to.push_back(j);
        }
    };
    for (size_t i = 0; i < n; ++i)
        if (known[i]) addNeighbours(i, ring);
    std::vector<float> value;
    while (!ring.empty()) {
        value.assign(ring.size() * 3, 0.0f);
        for (size_t r = 0; r < ring.size(); ++r) {
            const int x = int(ring[r] % size_t(width)), y = int(ring[r] / size_t(width));
            float s[3] = {0, 0, 0};
            int k = 0;
            for (int d = 0; d < 4; ++d) {
                const int xx = x + dx[d], yy = y + dy[d];
                if (xx < 0 || yy < 0 || xx >= width || yy >= height) continue;
                const size_t j = size_t(yy) * size_t(width) + size_t(xx);
                if (!known[j]) continue;
                for (int c = 0; c < 3; ++c) s[c] += rgb[j * 3 + c];
                ++k;
            }
            for (int c = 0; c < 3; ++c) value[r * 3 + c] = s[c] / float(k);  // k >= 1: it came from a known one
        }
        for (size_t r = 0; r < ring.size(); ++r) {
            for (int c = 0; c < 3; ++c) rgb[ring[r] * 3 + c] = value[r * 3 + c];
            known[ring[r]] = 1;
        }
        next.clear();
        for (size_t i : ring) addNeighbours(i, next);
        ring.swap(next);
    }
    for (size_t i = 0; i < n; ++i)
        for (int c = 0; c < 3; ++c) planes[size_t(c) * n + i] = rgb[i * 3 + c];
}

void enlargeAlpha(const float* alpha, int width, int height, int scale, std::vector<uint8_t>& out) {
    const int ow = width * scale, oh = height * scale;
    out.assign(size_t(ow) * size_t(oh), 0);
    for (int y = 0; y < oh; ++y)
        for (int x = 0; x < ow; ++x) {
            // The output pixel's centre in input texels, between the input's texel centres, clamped at the edges.
            const float fx = std::clamp((float(x) + 0.5f) / float(scale) - 0.5f, 0.0f, float(width - 1));
            const float fy = std::clamp((float(y) + 0.5f) / float(scale) - 0.5f, 0.0f, float(height - 1));
            const int x0 = int(fx), y0 = int(fy);
            const int x1 = std::min(x0 + 1, width - 1), y1 = std::min(y0 + 1, height - 1);
            const float tx = fx - float(x0), ty = fy - float(y0);
            const auto at = [&](int xx, int yy) { return alpha[size_t(yy) * size_t(width) + size_t(xx)]; };
            const float v = (at(x0, y0) * (1 - tx) + at(x1, y0) * tx) * (1 - ty) +
                            (at(x0, y1) * (1 - tx) + at(x1, y1) * tx) * ty;
            out[size_t(y) * size_t(ow) + size_t(x)] = v >= 0.5f ? 255 : 0;
        }
}

void enlargeCell(const uint32_t* argb, int width, int height, int scale, std::vector<uint32_t>& out) {
    std::vector<float> planes;
    prepareInput(argb, width, height, planes);
    std::vector<uint8_t> alpha;
    const size_t n = size_t(width) * size_t(height);
    enlargeAlpha(planes.data() + 3 * n, width, height, scale, alpha);
    const int ow = width * scale, oh = height * scale;
    out.assign(size_t(ow) * size_t(oh), 0);
    for (int y = 0; y < oh; ++y) {
        const float fy = std::clamp((float(y) + 0.5f) / float(scale) - 0.5f, 0.0f, float(height - 1));
        const int y0 = int(fy), y1 = std::min(y0 + 1, height - 1);
        const float ty = fy - float(y0);
        for (int x = 0; x < ow; ++x) {
            const float fx = std::clamp((float(x) + 0.5f) / float(scale) - 0.5f, 0.0f, float(width - 1));
            const int x0 = int(fx), x1 = std::min(x0 + 1, width - 1);
            const float tx = fx - float(x0);
            uint32_t p = uint32_t(alpha[size_t(y) * size_t(ow) + size_t(x)]) << 24;
            for (int c = 0; c < 3; ++c) {
                const float* plane = planes.data() + size_t(c) * n;
                const auto at = [&](int xx, int yy) { return plane[size_t(yy) * size_t(width) + size_t(xx)]; };
                const float v = (at(x0, y0) * (1 - tx) + at(x1, y0) * tx) * (1 - ty) +
                                (at(x0, y1) * (1 - tx) + at(x1, y1) * tx) * ty;
                p |= uint32_t(std::clamp(int(v * 255.0f + 0.5f), 0, 255)) << (16 - 8 * c);
            }
            out[size_t(y) * size_t(ow) + size_t(x)] = p;
        }
    }
}

void halve(const std::vector<uint32_t>& argb, int width, int height, std::vector<uint32_t>& out, int& outWidth,
           int& outHeight) {
    outWidth = std::max(1, width / 2);
    outHeight = std::max(1, height / 2);
    out.assign(size_t(outWidth) * size_t(outHeight), 0);
    for (int y = 0; y < outHeight; ++y)
        for (int x = 0; x < outWidth; ++x) {
            uint32_t s[4] = {0, 0, 0, 0}, n = 0;
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    const int sx = x * 2 + dx, sy = y * 2 + dy;
                    if (sx >= width || sy >= height) continue;
                    const uint32_t p = argb[size_t(sy) * size_t(width) + size_t(sx)];
                    for (int c = 0; c < 4; ++c) s[c] += (p >> (8 * c)) & 255;  // b, g, r, a
                    ++n;
                }
            uint32_t m[4];
            for (int c = 0; c < 4; ++c) m[c] = (s[c] + n / 2) / std::max(n, 1u);
            const uint32_t a = m[3] >= 128 ? 255u : 0u;
            out[size_t(y) * size_t(outWidth) + size_t(x)] = a << 24 | m[2] << 16 | m[1] << 8 | m[0];
        }
}

}  // namespace uagfx::ai
