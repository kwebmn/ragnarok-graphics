#include "core/reference_quests.hpp"

#include <algorithm>

#include "core/json.hpp"
#include "core/sha256.hpp"

namespace uagfx::refquests {
namespace {

constexpr uint64_t kMostSize = 16u << 20;
const char* const kKeys[kLangs] = {"ua", "ru", "en"};

bool hex64(const std::string& s) {
    if (s.size() != 64) return false;
    for (char c : s)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

}  // namespace

bool parse(const std::string& text, Manifest& out) {
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error) || v.type() != json::Value::Object) return false;
    Manifest m;
    m.format = int(v["v"].asInt(0));
    m.sha256 = v["sha256"].asString();
    m.size = uint64_t(std::max<long long>(0, v["size"].asInt(0)));
    m.count = int(v["count"].asInt(0));
    if (m.format != 1 || !hex64(m.sha256) || m.size == 0 || m.size > kMostSize) return false;
    out = m;
    return true;
}

std::string write(const Manifest& m) {
    return "{\"v\":" + std::to_string(m.format) + ",\"sha256\":\"" + m.sha256 + "\",\"size\":" +
           std::to_string(m.size) + ",\"count\":" + std::to_string(m.count) + "}";
}

std::string fileName(const Manifest& m) { return "quests-" + m.sha256.substr(0, 12) + ".json.gz"; }
std::string sitePath(const Manifest& m) { return "/api/public/game/" + fileName(m); }

bool needs(const Manifest& site, const std::optional<Manifest>& there, long long haveBytes) {
    if (!there || there->sha256 != site.sha256) return true;
    return haveBytes < 0 || uint64_t(haveBytes) != there->size;
}

bool verify(const std::string& bytes, const Manifest& m) {
    return bytes.size() == m.size && sha256::hexOf(bytes) == m.sha256;
}

bool read(const std::string& text, std::vector<Quest>& out) {
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error) || v.type() != json::Value::Object || v["v"].asInt(0) != 1 ||
        v["quests"].type() != json::Value::Array)
        return false;
    std::vector<Quest> list;
    for (const json::Value& a : v["quests"].items()) {
        Quest q;
        q.slug = a[size_t(0)].asString();
        q.category = a[size_t(2)].asString();
        q.minLevel = int(a[size_t(3)].asInt(0));
        q.npc = a[size_t(4)].asString();
        q.npcMap = a[size_t(5)].asString();
        for (int l = 0; l < kLangs; ++l) {
            q.title[l] = a[size_t(1)][kKeys[l]].asString();
            q.summary[l] = a[size_t(6)][kKeys[l]].asString();
            for (const json::Value& s : a[size_t(7)][kKeys[l]].items()) q.steps[l].push_back(s.asString());
            for (const json::Value& s : a[size_t(9)][kKeys[l]].items()) q.rewards[l].push_back(s.asString());
        }
        for (const json::Value& it : a[size_t(8)].items())
            if (it[size_t(0)].asInt(0) > 0)
                q.items.emplace_back(int(it[size_t(0)].asInt(0)), int(it[size_t(1)].asInt(1)));
        if (!q.slug.empty() && !(q.title[Uk].empty() && q.title[En].empty())) list.push_back(std::move(q));
    }
    out = std::move(list);
    return true;
}

const std::string& textOf(const std::string (&texts)[kLangs], int lang) {
    if (lang >= 0 && lang < kLangs && !texts[lang].empty()) return texts[lang];
    return texts[En].empty() ? texts[Uk] : texts[En];
}

const std::vector<std::string>& listOf(const std::vector<std::string> (&lists)[kLangs], int lang) {
    if (lang >= 0 && lang < kLangs && !lists[lang].empty()) return lists[lang];
    return lists[En].empty() ? lists[Uk] : lists[En];
}

}  // namespace uagfx::refquests
