#pragma once
#include <string>
#include <vector>

#include "core/settings/strings.hpp"

namespace uagfx::launcher {

// 2.11: the launcher «Гра» (the settings app's first page) - uaro.kiev.ua's GET /api/public/launcher?lang=uk|ru|en:
// the server's online count and the newest published news in the player's language (the site's LauncherController).
constexpr const char* kPath = "/api/public/launcher";
constexpr const char* kSiteUrl = "https://uaro.kiev.ua/";

struct News {
    std::string title, excerpt, date, url;  // url: the news on the site, "" when it is not the site's
    bool pinned = false;
};
struct Info {
    int online = -1, peak = -1;
    std::vector<News> news;  // as the site orders them: the pinned first
};

// false and why for anything that is not that answer (an error page, another shape).
bool parse(const std::string& json, Info& out, std::string& error);
std::string langToken(settings::Lang l);  // the site's: "uk", "ru", "en"

}  // namespace uagfx::launcher
