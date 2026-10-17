#include "core/shader_cache.hpp"

namespace uagfx::shader_cache {
namespace {

constexpr uint32_t kMagic = 0x53474155;  // "UAGS"
constexpr uint32_t kFormat = 1;
constexpr size_t kHead = 20;  // magic, format, key, count
constexpr size_t kSum = 8;    // the whole file's hash at its end

uint64_t fnv(const void* data, size_t size, uint64_t h = 0xcbf29ce484222325ull) {  // FNV-1a, 64 bits
    const auto* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) h = (h ^ p[i]) * 0x100000001b3ull;
    return h;
}

void put(std::vector<uint8_t>& out, uint64_t v, int bytes) {  // little-endian
    for (int i = 0; i < bytes; ++i) out.push_back(uint8_t(v >> (8 * i)));
}

// Reads up to `end` and no further; once anything is missing, `ok` stays false.
struct Reader {
    const std::vector<uint8_t>& in;
    size_t end;
    size_t at = 0;
    bool ok = true;
    uint64_t get(int bytes) {
        if (!ok || end - at < size_t(bytes)) {
            ok = false;
            return 0;
        }
        uint64_t v = 0;
        for (int i = 0; i < bytes; ++i) v |= uint64_t(in[at + size_t(i)]) << (8 * i);
        at += size_t(bytes);
        return v;
    }
    const uint8_t* take(size_t n) {
        if (!ok || end - at < n) {
            ok = false;
            return nullptr;
        }
        const uint8_t* p = in.data() + at;
        at += n;
        return p;
    }
};

}  // namespace

std::string profileOf(const std::string& profile, bool safe) { return safe ? profile + "+safe" : profile; }

bool matches(const Shader& s, const std::string& entry, const std::string& profile, bool& safe) {
    if (s.entry != entry) return false;
    safe = s.profile == profile + "+safe";
    return safe || s.profile == profile;
}

uint64_t keyOf(const std::string& source) {
    // With what else decides the code: the optimisation levels compile() tries in turn (0.39.2).
    static const char kSettings[] = "uaro-graphics shaders: optimisation 3, then 1, then none";
    return fnv(source.data(), source.size(), fnv(kSettings, sizeof kSettings - 1));
}

std::vector<uint8_t> pack(uint64_t key, const std::vector<Shader>& shaders) {
    std::vector<uint8_t> out;
    put(out, kMagic, 4);
    put(out, kFormat, 4);
    put(out, key, 8);
    put(out, shaders.size(), 4);
    for (const Shader& s : shaders) {
        put(out, s.entry.size(), 2);
        out.insert(out.end(), s.entry.begin(), s.entry.end());
        put(out, s.profile.size(), 2);
        out.insert(out.end(), s.profile.begin(), s.profile.end());
        put(out, s.code.size(), 4);
        out.insert(out.end(), s.code.begin(), s.code.end());
    }
    put(out, fnv(out.data(), out.size()), int(kSum));  // a byte changed anywhere: nothing of it is used
    return out;
}

std::vector<Shader> unpack(uint64_t key, const std::vector<uint8_t>& file) {
    if (file.size() < kHead + kSum) return {};
    const size_t body = file.size() - kSum;
    Reader sum{file, file.size(), body};
    if (sum.get(int(kSum)) != fnv(file.data(), body)) return {};
    Reader r{file, body};
    if (r.get(4) != kMagic || r.get(4) != kFormat || r.get(8) != key) return {};
    const uint64_t count = r.get(4);
    if (count > body / 8) return {};  // each takes 8 bytes at least
    std::vector<Shader> shaders(static_cast<size_t>(count));
    for (Shader& s : shaders) {
        const size_t entry = size_t(r.get(2));
        const uint8_t* e = r.take(entry);
        const size_t profile = size_t(r.get(2));
        const uint8_t* p = r.take(profile);
        const size_t code = size_t(r.get(4));
        const uint8_t* c = r.take(code);
        if (!r.ok) return {};
        s.entry.assign(reinterpret_cast<const char*>(e), entry);
        s.profile.assign(reinterpret_cast<const char*>(p), profile);
        s.code.assign(c, c + code);
    }
    if (!r.ok || r.at != body) return {};  // something after the shaders: not this file
    return shaders;
}

}  // namespace uagfx::shader_cache
