#include "core/reference_now.hpp"

#include <cstdint>
#include <cstring>

#include "core/json.hpp"

namespace uagfx::refnow {
namespace {

constexpr const char* kSite = "https://uaro.kiev.ua/";
constexpr size_t kTitleMost = 240;  // bytes: one line under the login, cut further to the screen

// One line: the control characters a space each, the ends trimmed, no longer than kTitleMost (whole characters)
std::string oneLine(const std::string& s) {
    std::string out;
    for (const char c : s) out += (unsigned char)c < 0x20 || c == 0x7F ? ' ' : c;
    const size_t a = out.find_first_not_of(' '), b = out.find_last_not_of(' ');
    out = a == std::string::npos ? std::string() : out.substr(a, b - a + 1);
    if (out.size() > kTitleMost) {
        size_t cut = kTitleMost;
        while (cut > 0 && ((unsigned char)out[cut] & 0xC0) == 0x80) --cut;  // not inside a character
        out.resize(cut);
    }
    return out;
}

bool sites(const std::string& url) {
    return url.compare(0, std::strlen(kSite), kSite) == 0 &&
           url.find_first_of(" \"<>\\^`{|}") == std::string::npos;
}

bool fromValue(const json::Value& item, Now& out) {
    if (item.type() != json::Value::Object) return false;
    Now n;
    n.title = oneLine(item["title"].asString());
    if (n.title.empty()) return false;
    const std::string url = item["url"].asString();
    if (sites(url)) n.url = url;
    n.startsAt = item["starts_at"].asInt(0);
    n.endsAt = item["ends_at"].asInt(0);
    out = std::move(n);
    return true;
}

json::Value toValue(const Now& n) {
    json::Value v = json::Value::object();
    v.set("title", json::Value(n.title));
    v.set("url", json::Value(n.url));
    v.set("starts_at", json::Value(int64_t(n.startsAt)));
    v.set("ends_at", json::Value(int64_t(n.endsAt)));
    return v;
}

}  // namespace

bool parse(const std::string& text, Now& out) {
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error) || v.type() != json::Value::Object) return false;
    return fromValue(v["item"], out);
}

bool current(const Now& n, long long now) { return n.endsAt <= 0 || now < n.endsAt; }

std::string write(const File& f) {
    json::Value items = json::Value::object();
    for (int i = 0; i < settings::kLangCount; ++i)
        items.set(settings::langToken(settings::Lang(i)), f.items[i] ? toValue(*f.items[i]) : json::Value());
    json::Value v = json::Value::object();
    v.set("v", json::Value(1));
    v.set("at", json::Value(int64_t(f.at)));
    v.set("items", std::move(items));
    return json::dump(v);
}

bool read(const std::string& text, File& out) {
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error) || v.type() != json::Value::Object || v["v"].asInt(0) != 1) return false;
    File f;
    f.at = v["at"].asInt(0);
    for (int i = 0; i < settings::kLangCount; ++i) {
        Now n;
        if (fromValue(v["items"][settings::langToken(settings::Lang(i))], n)) f.items[i] = std::move(n);
    }
    out = std::move(f);
    return true;
}

bool fresh(const File& f, long long now) { return f.at > 0 && now >= f.at - 60 && now - f.at <= kFreshSeconds; }

}  // namespace uagfx::refnow
