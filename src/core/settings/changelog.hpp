#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace uagfx::settings::changelog {

// 3.0.18-beta.192 (the owner: «хотелось бы ченжлог... что там происходит»; the launcher's «Журнал змін»,
// docs/superpowers/specs/2026-11-12-launcher-design.md): every version's notes from uaro-graphics\notes\TEST.<lang>.md
// - each «# uaRO Graphics X — <word>[ · YYYY-MM-DD]» heading and the points under it, the newest first as the file
// has them.

struct Point {
    std::string text;      // ** and ` left out; a numbered step under it on a line of its own
    bool section = false;  // a release's «## X» line
};
struct Version {
    std::string version, date;  // date "" when the heading has none (before 3.0.18-beta.56's were put back)
    bool beta = false;
    std::vector<Point> points;
};

std::vector<Version> parse(const std::string& markdown);
// How many lead the list newer than `seen` (the version the player last saw); 0 when `seen` is empty
size_t newerThan(const std::vector<Version>& v, const std::string& seen);
// The beta channel: all. The release channel: the releases alone, each with its betas' points after its own
// (X-beta.N under X); the betas of a version not released yet left out.
std::vector<Version> forChannel(const std::vector<Version>& all, bool beta);
// The «…» names in a point - a new setting's, for «Спробувати»
std::vector<std::string> quotedNames(const std::string& point);

}  // namespace uagfx::settings::changelog
