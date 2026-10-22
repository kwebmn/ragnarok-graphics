#include "core/launcher.hpp"

#include "core/json.hpp"

namespace uagfx::launcher {

bool parse(const std::string& text, Info& out, std::string& error) {
    json::Value v;
    if (!json::parse(text, v, error)) return false;
    if (v.type() != json::Value::Object || v["news"].type() != json::Value::Array) {
        error = "not the site's launcher answer";
        return false;
    }
    Info info;
    info.online = v["online"].type() == json::Value::Number ? int(v["online"].asInt()) : -1;
    info.peak = v["peak"].type() == json::Value::Number ? int(v["peak"].asInt()) : -1;
    for (const json::Value& n : v["news"].items()) {
        if (n.type() != json::Value::Object) continue;
        News item;
        item.title = n["title"].asString();
        item.excerpt = n["excerpt"].asString();
        item.date = n["date"].asString();
        item.pinned = n["pinned"].asBool();
        const std::string& url = n["url"].asString();
        if (url.rfind(kSiteUrl, 0) == 0) item.url = url;  // the launcher opens the site's pages only
        if (!item.title.empty()) info.news.push_back(item);
    }
    out = info;
    return true;
}

std::string langToken(settings::Lang l) {
    return l == settings::Lang::Uk ? "uk" : l == settings::Lang::Ru ? "ru" : "en";
}

bool redirect(bool startedByLauncher, bool directStart, bool appThere, long sinceLast) {
    return !startedByLauncher && !directStart && appThere && (sinceLast < 0 || sinceLast >= kRedirectGap);
}

}  // namespace uagfx::launcher
