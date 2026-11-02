#pragma once
#include <optional>
#include <string>

#include "core/settings/strings.hpp"

namespace uagfx::refnow {

// 3.0.11-beta.174 (the login concept's news line): the site's one item now - the event on now or next, else the
// newest news - GET /api/public/now.json?lang=ua|ru|en answers {"item": {"kind", "title", "url", "starts_at",
// "ends_at"}} or {"item": null}. The settings app's watcher asks it in the three languages and keeps them in
// uaro-graphics\reference\now.json (app/reference_pack); the login screen shows the one in the player's language.
struct Now {
    std::string title;  // UTF-8, one line
    std::string url;    // the site's page (https://uaro.kiev.ua/...) - empty for any other address: never opened
    long long startsAt = 0, endsAt = 0;  // unix times, 0 not told
};
bool parse(const std::string& json, Now& out);  // false: no item, or not the site's answer
bool current(const Now& n, long long now);      // false once its event is over

constexpr long long kFreshSeconds = 86400;  // the file's items shown for a day after they were asked
struct File {
    long long at = 0;                              // when they were asked
    std::optional<Now> items[settings::kLangCount];  // by settings::Lang; none - the site had none
};
std::string write(const File& f);  // {"v":1,"at":N,"items":{"en":{...}|null,"uk":...,"ru":...}}
bool read(const std::string& text, File& out);
bool fresh(const File& f, long long now);

}  // namespace uagfx::refnow
