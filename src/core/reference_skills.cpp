#include "core/reference_skills.hpp"

#include <cstdlib>

#include "core/json.hpp"

namespace uagfx::refskills {
namespace {

const char* const kKeys[settings::kLangCount] = {"en", "uk", "ru"};  // by settings::Lang

void wordsOf(const json::Value& o, std::map<int, std::string>& out) {
    for (const auto& [key, v] : o.members()) {
        const int id = std::atoi(key.c_str());
        if (id > 0 && !v.asString().empty()) out[id] = v.asString();
    }
}

}  // namespace

bool fromSite(const std::string& text, std::map<int, std::string>& out) {
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error) || !v.has("descs")) return false;
    out.clear();
    wordsOf(v["descs"], out);
    return true;
}

std::string write(const File& f) {
    json::Value v = json::Value::object();
    v.set("v", json::Value(1));
    v.set("at", json::Value(int64_t(f.at)));
    for (int l = 0; l < settings::kLangCount; ++l) {
        json::Value o = json::Value::object();
        for (const auto& [id, words] : f.words[l]) o.set(std::to_string(id), json::Value(words));
        v.set(kKeys[l], std::move(o));
    }
    return json::dump(v);
}

bool read(const std::string& text, File& out) {
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error) || !v.has("at")) return false;
    File f;
    f.at = static_cast<long long>(v["at"].asNumber());
    for (int l = 0; l < settings::kLangCount; ++l) wordsOf(v[kKeys[l]], f.words[l]);
    out = std::move(f);
    return true;
}

bool fresh(const File& f, long long now) { return f.at > 0 && now - f.at >= 0 && now - f.at < kFreshSeconds; }

std::vector<std::string> lines(const File& f, settings::Lang l, int skill) {
    const std::map<int, std::string>* in = &f.words[int(l)];
    auto it = in->find(skill);
    if (it == in->end()) {
        in = &f.words[int(settings::Lang::En)];
        it = in->find(skill);
        if (it == in->end()) return {};
    }
    std::vector<std::string> out;
    size_t from = 0;
    const std::string& s = it->second;
    while (from <= s.size()) {
        const size_t nl = s.find('\n', from);
        std::string line = s.substr(from, nl == std::string::npos ? std::string::npos : nl - from);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (!line.empty()) out.push_back(std::move(line));
        if (nl == std::string::npos) break;
        from = nl + 1;
    }
    return out;
}

}  // namespace uagfx::refskills
