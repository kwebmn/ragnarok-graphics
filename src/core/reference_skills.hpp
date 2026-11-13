#pragma once
#include <map>
#include <string>
#include <vector>

#include "core/settings/strings.hpp"

namespace uagfx::refskills {

// 3.0.18-beta.244 («Довідник» › «Навички»): the site's skills' words - GET /api/public/skilldesc.json?lang=ua|ru|en
// answers {"descs": {"<skill id>": "<words, lines by \n>"}, "mtime": N}. The settings app's watcher asks it in the three
// languages once a day and keeps them in uaro-graphics\reference\skilldesc.json; a skill's card shows them in the
// player's language (the English when that one has none; the game's own table when the file has none). No Windows here.
bool fromSite(const std::string& json, std::map<int, std::string>& out);  // false: not the site's words

constexpr long long kFreshSeconds = 86400;
struct File {
    long long at = 0;                                    // when they were asked
    std::map<int, std::string> words[settings::kLangCount];  // by settings::Lang
};
std::string write(const File& f);  // {"v":1,"at":N,"en":{"5":"..."},"uk":{...},"ru":{...}}
bool read(const std::string& text, File& out);
bool fresh(const File& f, long long now);
std::vector<std::string> lines(const File& f, settings::Lang l, int skill);

}  // namespace uagfx::refskills
