#include <cstring>
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
    "; uaRO Graphics: the newest release of this branch (tools/release_ini.py)\r\n"
    "[release]\r\nformat = 2\r\nversion = 2.10.0\r\nwithdrawn = 2.9.1 2.9.2\r\n"
    "[package]\r\nref = 0dfe661d7f5f3eb871c5defcd398a543196dbfd8\r\nfile = release/uaro-graphics.zip\r\n"
    "size = 35690731\r\nsha256 = 0123456789abcdef0123456789abcdef0123456789abcdef0123456789ABCDEF\r\n"
    "[dlss]\r\nversion = streamline 2.14.1\r\nref = 0dfe661d7f5f3eb871c5defcd398a543196dbfd8\r\n"
    "file = release/uaro-graphics-dlss.zip\r\nsize = 36007336\r\n"
    "sha256 = 1111111111111111111111111111111111111111111111111111111111111111\r\n"
    "[models]\r\nversion = gameai-2.0 1\r\nref = 28aa38a537beb4e8d29c6b6c8b17f5c94255ba48\r\n"
    "file = release/uaro-graphics-models.zip\r\nsize = 30873160\r\n"
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

// 3.0.0-beta.2: GitHub's update.ini of a branch (tools/release_ini.py) - its newest release and the archives, each
// by the commit that holds it.
TEST(manifest_2_reads_the_commits_and_paths) {
    update::Manifest m;
    std::string error;
    CHECK(update::parseManifest(kSiteText, m, error));
    CHECK_EQ(m.package.version, std::string("2.10.0"));
    CHECK_EQ(m.package.ref, std::string("0dfe661d7f5f3eb871c5defcd398a543196dbfd8"));
    CHECK_EQ(m.package.file, std::string("release/uaro-graphics.zip"));
    CHECK(m.package.size == 35690731u);
    CHECK_EQ(m.package.sha256, std::string("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
    CHECK_EQ(m.dlss.version, std::string("streamline 2.14.1"));
    CHECK_EQ(m.models.ref, std::string("28aa38a537beb4e8d29c6b6c8b17f5c94255ba48"));
    CHECK_EQ(m.models.file, std::string("release/uaro-graphics-models.zip"));
    CHECK(m.withdrawn == std::vector<std::string>({"2.9.1", "2.9.2"}));
    update::Manifest bare;  // no DLSS, no models in the branch: none
    CHECK(update::parseManifest(std::string(kSiteText).substr(0, std::string(kSiteText).find("[dlss]")), bare, error));
    CHECK(bare.dlss.file.empty() && bare.models.file.empty() && bare.package.size == 35690731u);
}

TEST(manifest_refuses_what_is_not_format_2) {
    const std::string file = "file = release/uaro-graphics.zip", ref = "ref = 0dfe661d7f5f3eb871c5defcd398a543196dbfd8";
    CHECK(refused("<html><body>502 Bad Gateway</body></html>", "format"));
    CHECK(refused(replaced(kSiteText, "format = 2", "format = 1"), "format"));  // uaro.kiev.ua's of 2.10
    CHECK(refused(replaced(kSiteText, "sha256 = 0123", "checksum = 0123"), "sha256"));
    CHECK(refused(replaced(kSiteText, "size = 35690731", "size = -5"), "size"));
    CHECK(refused(replaced(kSiteText, file, "file = release/../ddraw.dll"), "file"));
    CHECK(refused(replaced(kSiteText, file, "file = /release/uaro-graphics.zip"), "file"));
    CHECK(refused(replaced(kSiteText, file, "file = release\\uaro-graphics.zip"), "file"));
    CHECK(refused(replaced(kSiteText, file, "file = release/"), "file"));
    CHECK(refused(replaced(kSiteText, ref, ref.substr(0, ref.size() - 1)), "ref"));  // 39 hex digits
    CHECK(refused(replaced(kSiteText, "ref = 0dfe661d", "ref = 0DFE661D"), "ref"));
    CHECK(refused(replaced(kSiteText, "ref = 0dfe661d", "ref = 0dfe661g"), "ref"));
    CHECK(refused(replaced(kSiteText, ref + "\r\n", ""), "ref"));
    CHECK(refused(std::string(kSiteText).substr(0, std::string(kSiteText).find("456789ABCDEF")), "sha256"));
    CHECK(refused(replaced(kSiteText, "version = 2.10.0", "version ="), "version"));
}

// 3.0.0-beta.2: «Реліз» is the branch main, «Бета» the branch beta; an archive comes from its commit - a commit's file
// never changes, so an update.ini GitHub's cache keeps for 5 minutes names archives that still match it.
TEST(update_urls_name_the_branch_and_the_commit) {
    CHECK_EQ(update::manifestUrl(update::Channel::Release),
             std::string("https://raw.githubusercontent.com/kwebmn/ragnarok-graphics/main/release/update.ini"));
    CHECK_EQ(update::manifestUrl(update::Channel::Beta),
             std::string("https://raw.githubusercontent.com/kwebmn/ragnarok-graphics/beta/release/update.ini"));
    update::Manifest m;
    std::string error;
    CHECK(update::parseManifest(kSiteText, m, error));
    CHECK_EQ(update::archiveUrl(m.package),
             std::string("https://raw.githubusercontent.com/kwebmn/ragnarok-graphics/"
                         "0dfe661d7f5f3eb871c5defcd398a543196dbfd8/release/uaro-graphics.zip"));
    CHECK_EQ(update::localName(m.package), std::string("0123456789ab-uaro-graphics.zip"));
    update::Archive next = m.package;  // the next version under the same name: its own .part
    next.sha256 = std::string(64, 'f');
    CHECK(update::localName(next) != update::localName(m.package));
}

// 3.0.0-beta.2: the computer's channel - its [update] channel line, else a beta build's is «Бета».
TEST(update_channel_by_the_line_else_by_the_build) {
    using update::Channel;
    CHECK(update::channelOf("beta", "3.0.0") == Channel::Beta);
    CHECK(update::channelOf("release", "3.0.0-beta.2") == Channel::Release);
    CHECK(update::channelOf("", "3.0.0-beta.2") == Channel::Beta);
    CHECK(update::channelOf("", "3.0.0") == Channel::Release);
    CHECK(update::channelOf("nightly", "3.0.0") == Channel::Release);
    CHECK(update::preRelease("3.0.0-beta.2") && !update::preRelease("3.0.0") && !update::preRelease(""));
    CHECK_EQ(std::string(update::channelToken(Channel::Beta)), std::string("beta"));
    CHECK_EQ(std::string(update::channelToken(Channel::Release)), std::string("release"));
}

TEST(offer_is_a_newer_version_or_a_way_back_from_a_withdrawn_one) {
    update::Manifest m;
    std::string error;
    CHECK(update::parseManifest(kSiteText, m, error));
    const update::Channel r = update::Channel::Release;
    CHECK_EQ(update::offer(m, "2.9.0", "", r), std::string("2.10.0"));
    CHECK_EQ(update::offer(m, "2.10.0", "", r), std::string(""));
    CHECK_EQ(update::offer(m, "2.11.0", "", r), std::string(""));  // a developer's build
    CHECK_EQ(update::offer(m, "2.9.0", "2.10.0", r), std::string(""));  // rolled back from it
    CHECK_EQ(update::offer(m, "2.9.1", "", r), std::string("2.10.0"));
    m.package.version = "2.9.0";  // 2.9.1 withdrawn, the site back on 2.9.0: older, still offered
    CHECK_EQ(update::offer(m, "2.9.1", "", r), std::string("2.9.0"));
    CHECK_EQ(update::offer(m, "2.9.0 build 2026-10-22 18:00", "", r), std::string(""));
    CHECK_EQ(update::bare("2.9.0 build 2026-10-22 18:00"), std::string("2.9.0"));
}

// 3.0.5-beta.1 (the user: «я выбрал канал релиз, и нажал играть. и у меня запустилась моя бета. так должно быть?»): a
// beta installed and «Реліз» chosen - the release is offered, an older one too; never on «Бета».
TEST(offer_takes_a_beta_back_to_the_release_on_the_release_channel) {
    using update::Channel;
    update::Manifest m;
    m.package.version = "3.0.4";
    CHECK_EQ(update::offer(m, "3.0.5-beta.2", "", Channel::Release), std::string("3.0.4"));
    CHECK_EQ(update::offer(m, "3.0.5-beta.2", "", Channel::Beta), std::string(""));
    CHECK_EQ(update::offer(m, "3.0.5-beta.2", "3.0.4", Channel::Release), std::string(""));  // rolled back from it
    CHECK_EQ(update::offer(m, "3.0.4-beta.36", "", Channel::Release), std::string("3.0.4"));  // newer anyway
    CHECK_EQ(update::offer(m, "3.0.5", "", Channel::Release), std::string(""));  // a release is not taken back
    m.package.version = "3.0.5-beta.1";  // back only to a release
    CHECK_EQ(update::offer(m, "3.0.5-beta.2", "", Channel::Release), std::string(""));
}

TEST(update_back_to_the_release_is_a_beta_going_to_an_older_release) {
    using update::Channel;
    CHECK(update::backToRelease(Channel::Release, "3.0.4", "3.0.5-beta.2"));
    CHECK(!update::backToRelease(Channel::Beta, "3.0.4", "3.0.5-beta.2"));
    CHECK(!update::backToRelease(Channel::Release, "3.0.5", "3.0.5-beta.2"));  // the beta's own release: an update
    CHECK(!update::backToRelease(Channel::Release, "", "3.0.5-beta.2"));
    CHECK(!update::backToRelease(Channel::Release, "2.9.0", "2.9.1"));  // a withdrawn release's way back
}

TEST(update_the_games_start_asks_to_go_back_to_the_release) {
    using update::Channel;
    std::string version;
    const std::string kept = update::keptInfo(kSiteText, "", Channel::Release);
    CHECK(update::keptChannel(kept, "2.11.0-beta.1") == Channel::Release);
    CHECK(update::askAtStart(kept, "2.11.0-beta.1", version));
    CHECK_EQ(version, std::string("2.10.0"));
    CHECK(!update::askAtStart(update::keptInfo(kSiteText, "", Channel::Beta), "2.11.0-beta.1", version));
    CHECK(!update::askAtStart(kSiteText, "2.11.0-beta.1", version));  // an older app's: no channel, a beta's
    const std::string skipped = update::keptInfo(kSiteText, "2.10.0", Channel::Release);
    CHECK(!update::askAtStart(skipped, "2.11.0-beta.1", version));  // rolled back from it
    CHECK(skipped.find("[local]\r\nskip = 2.10.0\r\nchannel = release\r\n") != std::string::npos);
}

TEST(update_check_orders_pre_releases) {
    CHECK(update::newer("3.0.0-beta.2", "3.0.0-beta.1"));
    CHECK(update::newer("3.0.0-beta.1", "2.10.0"));
    CHECK(update::newer("3.0.0", "3.0.0-beta.2"));
    CHECK(!update::newer("3.0.0-beta.1", "3.0.0"));
    CHECK(update::newer("3.0.0-beta.10", "3.0.0-beta.9"));
}

// 2.10: the game's start asks (through the settings app) when the last check - the watcher's, at an earlier start -
// found an update; uaro-graphics\update.ini is the branch's text and this computer's [local] section.
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

// 2.10: uaro-graphics-setup.exe is uaro-graphics.exe with the package appended (tools/package.sh): the zip, its size
// in eight bytes (little-endian) and "UAROSETP".
TEST(setup_finds_the_package_appended_to_it) {
    uint8_t tail[16] = {};
    const uint64_t zip = 35690731, exe = 3677696;
    for (int i = 0; i < 8; ++i) tail[i] = uint8_t(zip >> (8 * i));
    std::memcpy(tail + 8, "UAROSETP", 8);
    uint64_t offset = 0, size = 0;
    CHECK(update::payloadAt(tail, exe + zip + 16, offset, size));
    CHECK(offset == exe && size == zip);
    tail[15] = 'X';
    CHECK(!update::payloadAt(tail, exe + zip + 16, offset, size));  // the app itself
    tail[15] = 'P';
    CHECK(!update::payloadAt(tail, zip, offset, size));  // larger than the file
    CHECK(!update::payloadAt(tail, 10, offset, size));
}
