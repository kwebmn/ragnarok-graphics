#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace uagfx::refpack {

// 3.0.11-beta.5 (the owner: «база сайта оновлювалась, а панель підкачувала дані з бази сайту»): the game's reference
// («Довідник») - the site's game database as one pack (uaro.kiev.ua builds it after each import of the server's repo:
// /api/public/game/pack.json = {v, data, commit, date, size, sha256, url}). Kept in the folder layout::kReference:
// pack.json - the manifest of the pack there, pack-<data>.json.gz - the pack (gzip of JSON, format 1). The game's
// watcher asks at the game's start; the pack comes only when the site has another one, and the copy there serves
// while the site is down.
constexpr int kFormat = 1;
constexpr uint64_t kMostSize = 32u << 20;  // a pack the site could not mean (format 1 is about 270 KB)

struct Manifest {
    int format = 0;       // "v"
    long long data = 0;   // the site's data stamp (the import's time, unix seconds) - the pack's name
    std::string commit;   // the server repo's commit the data came from
    std::string date;     // its date (ISO 8601)
    uint64_t size = 0;    // the pack's bytes
    std::string sha256;   // 64 lower-case hex digits
};

// false: no manifest of a format we read (another "v", no data, a size of nothing or past kMostSize, no sha256)
bool parse(const std::string& json, Manifest& out);
std::string write(const Manifest& m);  // what pack.json keeps (parse reads it back)
std::string fileName(const Manifest& m);  // "pack-<data>.json.gz"
// The site's path of the pack: built from the stamp - the manifest's own url is never followed (a pack only from
// uaro.kiev.ua, whatever an answer says)
std::string sitePath(const Manifest& m);
// The pack to fetch: there is none, another one is newer on the site, or the one there is gone or cut short
// (`haveBytes`: the size of pack-<data>.json.gz there, -1 none)
bool needs(const Manifest& site, const std::optional<Manifest>& there, long long haveBytes);
bool verify(const std::string& bytes, const Manifest& m);  // its size and SHA-256
std::string dateOf(const Manifest& m);  // "2026-10-23": the commit's day, "" none

}  // namespace uagfx::refpack
