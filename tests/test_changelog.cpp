// 3.0.18-beta.192 (the launcher's «Журнал змін», docs/superpowers/plans/2026-11-12-launcher.md Task 2): every version's
// notes from uaro-graphics\notes\TEST.<lang>.md - its headings (with the date the ship script writes), its points
#include <string>
#include <vector>

#include "core/settings/changelog.hpp"
#include "test.hpp"

using namespace uagfx::settings;

namespace {

const char* const kFile =
    "# uaRO Graphics 3.0.18-beta.191 — що нового · 2026-11-12\n"
    "\n"
    "- **Факели карт:** «Новий вогонь» ховається за стінами.\n"
    "- Контрольні знімки — усі п'ять.\n"
    "\n"
    "# uaRO Graphics 3.0.18-beta.190 — що нового · 2026-11-11\n"
    "\n"
    "- Enter у вікні `кількості`.\n"
    "\n"
    "# uaRO Graphics 3.0.17 — що нового\n"
    "\n"
    "Простий старт: вітальний екран.\n"
    "\n"
    "## Простий старт\n"
    "\n"
    "- **Вітальний екран «Гра»:** «Грати».\n"
    "  1. крок\n"
    "\n"
    "# uaRO Graphics 3.0.17-beta.27 — що нового\n"
    "\n"
    "- плитки\n";

}  // namespace

TEST(changelog_parse_headings_and_points) {
    const std::vector<changelog::Version> v = changelog::parse(kFile);
    CHECK_EQ(v.size(), size_t(4));
    CHECK_EQ(v[0].version, std::string("3.0.18-beta.191"));
    CHECK_EQ(v[0].date, std::string("2026-11-12"));
    CHECK(v[0].beta);
    CHECK_EQ(v[0].points.size(), size_t(2));
    CHECK_EQ(v[0].points[0].text, std::string("Факели карт: «Новий вогонь» ховається за стінами."));
    CHECK(!v[0].points[0].section);
    CHECK_EQ(v[1].points[0].text, std::string("Enter у вікні кількості."));
    CHECK_EQ(v[2].version, std::string("3.0.17"));
    CHECK_EQ(v[2].date, std::string());
    CHECK(!v[2].beta);
    CHECK_EQ(v[2].points.size(), size_t(3));
    CHECK_EQ(v[2].points[0].text, std::string("Простий старт: вітальний екран."));
    CHECK(v[2].points[1].section);
    CHECK_EQ(v[2].points[1].text, std::string("Простий старт"));
    CHECK_EQ(v[2].points[2].text, std::string("Вітальний екран «Гра»: «Грати».\n1. крок"));
    // Windows' line ends read alike
    std::string crlf = kFile;
    for (size_t at = crlf.find('\n'); at != std::string::npos; at = crlf.find('\n', at + 2)) crlf.insert(at, "\r");
    const std::vector<changelog::Version> w = changelog::parse(crlf);
    CHECK_EQ(w.size(), size_t(4));
    CHECK_EQ(w[0].date, std::string("2026-11-12"));
    CHECK_EQ(w[2].points[2].text, v[2].points[2].text);
}

TEST(changelog_newer_than_seen) {
    const std::vector<changelog::Version> v = changelog::parse(kFile);
    CHECK_EQ(changelog::newerThan(v, "3.0.17"), size_t(2));
    CHECK_EQ(changelog::newerThan(v, "3.0.18-beta.191"), size_t(0));
    CHECK_EQ(changelog::newerThan(v, "3.0.17-beta.30"), size_t(3));  // not listed: by the versions' order
    CHECK_EQ(changelog::newerThan(v, "9.9.9"), size_t(0));
    CHECK_EQ(changelog::newerThan(v, ""), size_t(0));  // the first start: nothing marked new
}

TEST(changelog_release_channel_folds_betas) {
    const std::vector<changelog::Version> v = changelog::parse(kFile);
    const std::vector<changelog::Version> beta = changelog::forChannel(v, true);
    CHECK_EQ(beta.size(), size_t(4));
    const std::vector<changelog::Version> release = changelog::forChannel(v, false);
    CHECK_EQ(release.size(), size_t(1));  // 3.0.18's betas left out while 3.0.18 is not out
    CHECK_EQ(release[0].version, std::string("3.0.17"));
    CHECK_EQ(release[0].points.size(), size_t(4));  // its own three, then its beta's
    CHECK_EQ(release[0].points[3].text, std::string("плитки"));
}

TEST(changelog_quoted_names) {
    const std::vector<std::string> names = changelog::quotedNames("«Новий вогонь» і «Склад», а «» нічого");
    CHECK_EQ(names.size(), size_t(2));
    CHECK_EQ(names[0], std::string("Новий вогонь"));
    CHECK_EQ(names[1], std::string("Склад"));
    CHECK(changelog::quotedNames("без лапок").empty());
}

TEST(changelog_parse_odd_files) {
    const std::vector<changelog::Version> v = changelog::parse(
        "- stray\n\n# uaRO Graphics 1.0 — x\n\n# Not a version\n\n# uaRO Graphics 0.9 — y\n\n- p\n");
    CHECK_EQ(v.size(), size_t(2));  // the point before the first heading and the other heading left out
    CHECK_EQ(v[0].version, std::string("1.0"));
    CHECK(v[0].points.empty());
    CHECK_EQ(v[1].version, std::string("0.9"));
    CHECK_EQ(v[1].points.size(), size_t(1));
    CHECK(changelog::parse("").empty());
}
