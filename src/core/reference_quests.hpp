#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace uagfx::refquests {

// 3.0.11-beta.16 (the owner: «сделай как можно больше»): the site's quests for «Довідник» - its CMS quests
// (published), each in three languages: /api/public/game/quests.json = {v, sha256, size, count, url}, the file
// quests-<sha12>.json.gz = gzip of {v, quests: [[slug, {ua, ru, en} title, category, min level, NPC, NPC's map,
// {lang: summary}, {lang: [steps]}, [[item, how many]], {lang: [rewards]}]]}. Its address has its hash: an edit in the
// CMS is a new address (the cache of uaro.kiev.ua's front never serves an old one). Kept in uaro-graphics\reference:
// quests.json (the manifest) and the file. No Windows here.

struct Manifest {
    int format = 0;
    std::string sha256;
    uint64_t size = 0;
    int count = 0;
};
bool parse(const std::string& json, Manifest& out);
std::string write(const Manifest& m);
std::string fileName(const Manifest& m);  // "quests-<sha12>.json.gz"
std::string sitePath(const Manifest& m);  // "/api/public/game/quests-<sha12>.json.gz" - never the answer's url
bool needs(const Manifest& site, const std::optional<Manifest>& there, long long haveBytes);
bool verify(const std::string& bytes, const Manifest& m);

enum Lang : int { Uk, Ru, En, kLangs };  // the site's keys "ua", "ru", "en"
struct Quest {
    std::string slug, category, npc, npcMap;
    int minLevel = 0;
    std::string title[kLangs], summary[kLangs];
    std::vector<std::string> steps[kLangs], rewards[kLangs];
    std::vector<std::pair<int, int>> items;  // item, how many
};
// The file's JSON (gunzipped) into the quests; false when it is not one
bool read(const std::string& json, std::vector<Quest>& out);
// A text in a language, else English, else Ukrainian (the CMS fills them unevenly)
const std::string& textOf(const std::string (&texts)[kLangs], int lang);
const std::vector<std::string>& listOf(const std::vector<std::string> (&lists)[kLangs], int lang);

}  // namespace uagfx::refquests
