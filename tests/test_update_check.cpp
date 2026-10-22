#include <string>
#include <vector>

#include "core/update_check.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(update_check_compares_versions_by_their_numbers) {
    CHECK(update::newer("0.49.0", "0.48.0"));
    CHECK(update::newer("0.49.10", "0.49.9"));  // numbers, not text
    CHECK(update::newer("1.0.0", "0.99.99"));
    CHECK(!update::newer("0.48.0", "0.48.0"));
    CHECK(!update::newer("0.47.1", "0.48.0"));
    CHECK(!update::newer("", "0.48.0"));
}

namespace {

const char* const kSiteText =
    "; uaRO Graphics: the newest release\r\n"
    "[release]\r\nformat = 1\r\nversion = 2.10.0\r\nwithdrawn = 2.9.1 2.9.2\r\n"
    "[package]\r\nfile = uaro-graphics-2.10.0.zip\r\nsize = 35690731\r\n"
    "sha256 = 0123456789abcdef0123456789abcdef0123456789abcdef0123456789ABCDEF\r\n"
    "[dlss]\r\nversion = streamline 2.14.1\r\nfile = uaro-graphics-dlss-streamline-2.14.1.zip\r\nsize = 36007336\r\n"
    "sha256 = 1111111111111111111111111111111111111111111111111111111111111111\r\n"
    "[models]\r\nversion = gameai-2.0 1\r\nfile = uaro-graphics-models-gameai-2.0-1.zip\r\nsize = 30873160\r\n"
    "sha256 = 2222222222222222222222222222222222222222222222222222222222222222\r\n";

std::string replaced(std::string text, const std::string& from, const std::string& to) {
    const size_t at = text.find(from);
    return at == std::string::npos ? text : text.replace(at, from.size(), to);
}

bool refused(const std::string& text, const std::string& field) {
    update::Manifest m;
    std::string error;
    return !update::parseManifest(text, m, error) && error.find(field) != std::string::npos;
}

}  // namespace

// 2.10: the site's update.ini (tools/publish_site.py) - the newest release and its archives.
TEST(manifest_reads_the_newest_release) {
    update::Manifest m;
    std::string error;
    CHECK(update::parseManifest(kSiteText, m, error));
    CHECK_EQ(m.package.version, std::string("2.10.0"));
    CHECK_EQ(m.package.file, std::string("uaro-graphics-2.10.0.zip"));
    CHECK(m.package.size == 35690731u);
    CHECK_EQ(m.package.sha256, std::string("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
    CHECK_EQ(m.dlss.version, std::string("streamline 2.14.1"));
    CHECK_EQ(m.models.file, std::string("uaro-graphics-models-gameai-2.0-1.zip"));
    CHECK(m.withdrawn == std::vector<std::string>({"2.9.1", "2.9.2"}));
    update::Manifest bare;  // no DLSS, no models on the site: none
    CHECK(update::parseManifest(std::string(kSiteText).substr(0, std::string(kSiteText).find("[dlss]")), bare, error));
    CHECK(bare.dlss.file.empty() && bare.models.file.empty() && bare.package.size == 35690731u);
}

TEST(manifest_refuses_what_is_not_format_1) {
    CHECK(refused("<html><body>502 Bad Gateway</body></html>", "format"));
    CHECK(refused(replaced(kSiteText, "format = 1", "format = 2"), "format"));
    CHECK(refused(replaced(kSiteText, "sha256 = 0123", "checksum = 0123"), "sha256"));
    CHECK(refused(replaced(kSiteText, "size = 35690731", "size = -5"), "size"));
    CHECK(refused(replaced(kSiteText, "file = uaro-graphics-2.10.0.zip", "file = ../ddraw.dll"), "file"));
    CHECK(refused(std::string(kSiteText).substr(0, std::string(kSiteText).find("456789ABCDEF")), "sha256"));
    CHECK(refused(replaced(kSiteText, "version = 2.10.0", "version ="), "version"));
}

TEST(offer_is_a_newer_version_or_a_way_back_from_a_withdrawn_one) {
    update::Manifest m;
    std::string error;
    CHECK(update::parseManifest(kSiteText, m, error));
    CHECK_EQ(update::offer(m, "2.9.0", ""), std::string("2.10.0"));
    CHECK_EQ(update::offer(m, "2.10.0", ""), std::string(""));
    CHECK_EQ(update::offer(m, "2.11.0", ""), std::string(""));  // a developer's build
    CHECK_EQ(update::offer(m, "2.9.0", "2.10.0"), std::string(""));  // rolled back from it
    CHECK_EQ(update::offer(m, "2.9.1", ""), std::string("2.10.0"));
    m.package.version = "2.9.0";  // 2.9.1 withdrawn, the site back on 2.9.0: older, still offered
    CHECK_EQ(update::offer(m, "2.9.1", ""), std::string("2.9.0"));
    CHECK_EQ(update::offer(m, "2.9.0 build 2026-10-22 18:00", ""), std::string(""));
    CHECK_EQ(update::bare("2.9.0 build 2026-10-22 18:00"), std::string("2.9.0"));
}

TEST(update_check_orders_pre_releases) {
    CHECK(update::newer("3.0.0-beta.2", "3.0.0-beta.1"));
    CHECK(update::newer("3.0.0-beta.1", "2.10.0"));
    CHECK(update::newer("3.0.0", "3.0.0-beta.2"));
    CHECK(!update::newer("3.0.0-beta.1", "3.0.0"));
    CHECK(update::newer("3.0.0-beta.10", "3.0.0-beta.9"));
}

// 2.10: the game's start asks (through the settings app) when the last check - the watcher's, at an earlier start -
// found an update; uaro-graphics\update.ini is the site's text and this computer's [local] section.
TEST(the_game_asks_when_the_last_check_offered) {
    std::string version;
    CHECK(update::askAtStart(kSiteText, "2.9.0", version));
    CHECK_EQ(version, std::string("2.10.0"));
    CHECK(!update::askAtStart(std::string(kSiteText) + "[local]\r\nskip = 2.10.0\r\n", "2.9.0", version));
    CHECK(!update::askAtStart(kSiteText, "2.10.0", version));  // already it
    CHECK(!update::askAtStart("<html>502</html>", "2.9.0", version));
    CHECK(!update::askAtStart("", "2.9.0", version));
    CHECK(update::kUpdateChosen == 10);  // the settings app's exit code the driver closes the game on
}
