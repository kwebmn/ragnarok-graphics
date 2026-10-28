#include "core/reference_pack.hpp"

#include "core/json.hpp"
#include "core/sha256.hpp"

namespace uagfx::refpack {

namespace {

bool hex64(const std::string& s) {
    return s.size() == 64 && s.find_first_not_of("0123456789abcdef") == std::string::npos;
}

}  // namespace

bool parse(const std::string& text, Manifest& out) {
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error) || v.type() != json::Value::Object) return false;
    Manifest m;
    m.format = int(v["v"].asInt(0));
    m.data = v["data"].asInt(0);
    m.commit = v["commit"].asString();
    m.date = v["date"].asString();
    const double size = v["size"].asNumber(0);
    m.sha256 = v["sha256"].asString();
    if (m.format != kFormat || m.data <= 0 || size < 1 || size > double(kMostSize) || !hex64(m.sha256)) return false;
    m.size = uint64_t(size);
    out = m;
    return true;
}

std::string write(const Manifest& m) {
    json::Value v = json::Value::object();
    v.set("v", json::Value(m.format));
    v.set("data", json::Value(int64_t(m.data)));
    v.set("commit", m.commit);
    v.set("date", m.date);
    v.set("size", json::Value(int64_t(m.size)));
    v.set("sha256", m.sha256);
    return json::dump(v);
}

std::string fileName(const Manifest& m) {
    return "pack-" + std::to_string(m.data) + ".json.gz";
}

std::string sitePath(const Manifest& m) {
    return "/api/public/game/pack-" + std::to_string(m.data) + ".gz";
}

bool needs(const Manifest& site, const std::optional<Manifest>& there, long long haveBytes) {
    if (!there || there->data != site.data || there->sha256 != site.sha256) return true;
    return haveBytes < 0 || uint64_t(haveBytes) != there->size;
}

bool verify(const std::string& bytes, const Manifest& m) {
    return bytes.size() == m.size && sha256::hexOf(bytes) == m.sha256;
}

std::string dateOf(const Manifest& m) {
    return m.date.size() >= 10 ? m.date.substr(0, 10) : "";
}

}  // namespace uagfx::refpack
