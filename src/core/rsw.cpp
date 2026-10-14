#include "core/rsw.hpp"

#include <cstring>

namespace uagfx::rsw {
namespace {

class Reader {
public:
    Reader(const uint8_t* d, size_t n) : d_(d), n_(n) {}
    bool skip(size_t k) {
        if (n_ - p_ < k) return ok_ = false;
        p_ += k;
        return true;
    }
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
    bool ok() const { return ok_; }

private:
    const uint8_t* d_;
    size_t n_;
    size_t p_ = 0;
    bool ok_ = true;
};

}  // namespace

bool parse(const uint8_t* data, size_t size, World& out, std::string& error) {
    out = World{};
    if (!data || size < 6 || std::memcmp(data, "GRSW", 4) != 0) {
        error = "not a map file (GRSW)";
        return false;
    }
    Reader r(data, size);
    r.skip(4);
    const int major = r.get<uint8_t>(), minor = r.get<uint8_t>();
    const int v = major * 256 + minor;
    out.version = v;
    r.skip(40 * 4);  // ini, gnd, gat, src
    if (v >= 0x103) {  // water: level, type, wave height, speed, pitch (and from 1.8 its animation)
        r.skip(4);
        out.waterType = r.get<int32_t>();
        r.skip(12 + (v >= 0x108 ? 4 : 0));
    }
    r.skip(8 + 24 + (v >= 0x107 ? 4 : 0));               // the light: longitude, latitude, colours, opacity
    if (v >= 0x106) r.skip(16);                          // the ground's box
    const int32_t count = r.get<int32_t>();
    if (!r.ok() || count < 0 || count > 1000000) {
        error = "a damaged map file";
        return false;
    }
    for (int32_t i = 0; i < count && r.ok(); ++i) {
        const int32_t type = r.get<int32_t>();
        if (type == 1) {  // a model: name, animation, speed, block, file, node, place, turn, scale
            r.skip(40 + 12 + 80 + 80 + 36);
        } else if (type == 2) {
            Light l;
            r.skip(80);
            for (float& f : l.pos) f = r.get<float>();
            for (float& f : l.color) f = r.get<float>();
            l.range = r.get<float>();
            if (r.ok()) out.lights.push_back(l);
        } else if (type == 3) {  // a sound: name, file, place, volume, size, range, cycle (2.0 on)
            r.skip(160 + 12 + 4 + 8 + 4 + (v >= 0x200 ? 4 : 0));
        } else if (type == 4) {
            Effect e;
            r.skip(80);
            for (float& f : e.pos) f = r.get<float>();
            e.id = r.get<int32_t>();
            e.delay = r.get<float>();
            for (float& f : e.param) f = r.get<float>();
            if (r.ok()) out.effects.push_back(e);
        } else {
            break;  // its size is unknown: what came before it stays
        }
    }
    return true;
}

}  // namespace uagfx::rsw
