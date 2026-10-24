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
    std::string name(size_t width) {  // 2.0: a name of `width` bytes, its zeros off
        if (n_ - p_ < width) {
            ok_ = false;
            return {};
        }
        const char* s = reinterpret_cast<const char*>(d_ + p_);
        p_ += width;
        return std::string(s, strnlen(s, width));
    }

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
    out.lightLongitude = r.get<int32_t>();  // 2.1.3: the light: longitude, latitude, then colours, opacity
    out.lightLatitude = r.get<int32_t>();
    out.lightKnown = r.ok();
    r.skip(24 + (v >= 0x107 ? 4 : 0));
    if (v >= 0x106) r.skip(16);                          // the ground's box
    const int32_t count = r.get<int32_t>();
    if (!r.ok() || count < 0 || count > 1000000) {
        error = "a damaged map file";
        return false;
    }
    for (int32_t i = 0; i < count && r.ok(); ++i) {
        const int32_t type = r.get<int32_t>();
        if (type == 1) {  // a model: name, animation, speed, block, file, node, place, turn, scale
            Model m;
            r.skip(40 + 12);
            m.file = r.name(80);
            r.skip(80);
            for (float& f : m.pos) f = r.get<float>();
            for (float& f : m.rot) f = r.get<float>();
            for (float& f : m.scale) f = r.get<float>();
            if (r.ok() && out.models.size() < 200000) out.models.push_back(std::move(m));
        } else if (type == 2) {
            Light l;
            r.skip(80);
            for (float& f : l.pos) f = r.get<float>();
            for (float& f : l.color) f = r.get<float>();
            l.range = r.get<float>();
            if (r.ok()) out.lights.push_back(l);
        } else if (type == 3) {  // a sound: name, file, place, volume, size, range, cycle (2.0 on)
            Sound s;
            r.skip(80);
            s.file = r.name(80);
            for (float& f : s.pos) f = r.get<float>();
            s.volume = r.get<float>();
            r.skip(8);  // its width and height
            s.range = r.get<float>();
            if (v >= 0x200) s.cycle = r.get<float>();
            if (r.ok() && out.sounds.size() < 100000) out.sounds.push_back(std::move(s));
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
