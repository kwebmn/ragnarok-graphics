#include "core/gnd.hpp"

#include <cstring>

namespace uagfx::gnd {
namespace {

class Reader {
public:
    Reader(const uint8_t* d, size_t n) : d_(d), n_(n) {}
    template <typename T>
    T get() {
        T v{};
        if (n_ - p_ < sizeof v) {
            ok_ = false;
            return v;
        }
        std::memcpy(&v, d_ + p_, sizeof v);
        p_ += sizeof v;
        return v;
    }
    bool bytes(void* out, size_t k) {
        if (n_ - p_ < k) return ok_ = false;
        std::memcpy(out, d_ + p_, k);
        p_ += k;
        return true;
    }
    bool ok() const { return ok_; }

private:
    const uint8_t* d_;
    size_t n_, p_ = 0;
    bool ok_ = true;
};

}  // namespace

bool parse(const uint8_t* data, size_t size, Ground& out, std::string& error) {
    out = Ground{};
    if (!data || size < 6 || std::memcmp(data, "GRGN", 4) != 0) {
        error = "not a ground file (GRGN)";
        return false;
    }
    // 3.0.4-beta.13: 1.8 and 1.9 are 1.7 with the map's water after the cells (left unread)
    if (data[4] != 1 || data[5] < 7 || data[5] > 9) {
        error = "a ground file of version " + std::to_string(data[4]) + "." + std::to_string(data[5]) + ", not 1.7-1.9";
        return false;
    }
    Reader r(data + 6, size - 6);
    out.width = r.get<int32_t>();
    out.height = r.get<int32_t>();
    out.zoom = r.get<float>();
    const int32_t textures = r.get<int32_t>(), nameLength = r.get<int32_t>();
    if (!r.ok() || out.width <= 0 || out.height <= 0 || out.width > 2048 || out.height > 2048 || textures < 0 ||
        textures > 4096 || nameLength <= 0 || nameLength > 256) {
        error = "a damaged ground file (its size or textures)";
        return false;
    }
    std::vector<char> name(size_t(nameLength) + 1, 0);
    for (int32_t i = 0; i < textures && r.ok(); ++i) {
        r.bytes(name.data(), size_t(nameLength));
        out.textures.emplace_back(name.data(), strnlen(name.data(), size_t(nameLength)));
    }
    const int32_t lightmaps = r.get<int32_t>(), lw = r.get<int32_t>(), lh = r.get<int32_t>(), lc = r.get<int32_t>();
    if (!r.ok() || lightmaps < 0 || lightmaps > 4000000 || lw != 8 || lh != 8 || lc != 1) {
        error = "a damaged ground file (its lightmaps)";
        return false;
    }
    out.lightmaps.resize(size_t(lightmaps));
    for (Lightmap& l : out.lightmaps) {
        r.bytes(l.brightness, sizeof l.brightness);
        r.bytes(l.rgb, sizeof l.rgb);
    }
    const int32_t surfaces = r.get<int32_t>();
    if (!r.ok() || surfaces < 0 || surfaces > 8000000) {
        error = "a damaged ground file (its surfaces)";
        return false;
    }
    out.surfaces.resize(size_t(surfaces));
    for (Surface& s : out.surfaces) {
        for (float& f : s.u) f = r.get<float>();
        for (float& f : s.v) f = r.get<float>();
        s.texture = r.get<int16_t>();
        s.lightmap = r.get<uint16_t>();
        uint8_t bgra[4] = {};
        r.bytes(bgra, 4);
        s.color = uint32_t(bgra[3]) << 24 | uint32_t(bgra[2]) << 16 | uint32_t(bgra[1]) << 8 | bgra[0];
    }
    out.cells.resize(size_t(out.width) * size_t(out.height));
    for (Cell& c : out.cells) {
        for (float& f : c.h) f = r.get<float>();
        c.top = r.get<int32_t>();
        c.front = r.get<int32_t>();
        c.right = r.get<int32_t>();
    }
    if (!r.ok()) {
        error = "a ground file that ends short";
        out = Ground{};
        return false;
    }
    return true;
}

}  // namespace uagfx::gnd
