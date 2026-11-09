#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::update {

// 0.49: the settings app's update check; 2.10: from uaro.kiev.ua (no GitHub: its limit of 60 questions an hour from
// one address, and update.bat).
// a is a later version than b (by their numbers); 2.10: a pre-release ("3.0.0-beta.2") comes before its release
bool newer(const std::string& a, const std::string& b);
std::string bare(const std::string& version);  // "2.10.0 build 2026-10-22 18:00" -> "2.10.0"

// uaro.kiev.ua: the launcher's news, the reports and the game's files (2.10-3.0.0-beta.1 its update.ini too).
constexpr const char* kSite = "uaro.kiev.ua";
// 3.0.0-beta.2 (the user): updates come from GitHub, two channels - «Реліз» the branch main, every player's;
// «Бета» the branch beta, the testers' builds first. Each branch's release/update.ini (tools/release_ini.py) names its
// newest release and the archives: [release] format = 2, version, withdrawn (versions between spaces); [package]
// ref (the commit that holds it), file (its path there), size, sha256; [dlss] and [models] the same and their own
// version line. Read from raw.githubusercontent.com - no API, so no limit of 60 questions an hour.
enum class Channel { Release, Beta };
constexpr const char* kRawHost = "raw.githubusercontent.com";
const char* channelToken(Channel c);  // "release", "beta"
bool preRelease(const std::string& version);  // "3.0.0-beta.2": a '-' after the numbers
// The computer's channel: its [update] channel line's, else «Бета» for a pre-release build, «Реліз» for a release.
Channel channelOf(const std::string& token, const std::string& installed);
std::string manifestUrl(Channel c);  // https://raw.githubusercontent.com/kwebmn/ragnarok-graphics/<branch>/release/...
struct Archive {
    std::string version, ref, file, sha256;  // file "": none; ref 40 and sha256 64 lower-case hex digits
    uint64_t size = 0;
};
// An archive from its commit: a commit's file never changes, so an update.ini GitHub's cache keeps for 5 minutes
// still names archives that match it.
std::string archiveUrl(const Archive& a);
// Its name in uaro-graphics\update\: "<the SHA-256's first 12>-<the file's name>" - the next version under the same
// name never continues this one's .part.
std::string localName(const Archive& a);
struct Manifest {
    Archive package, dlss, models;
    std::vector<std::string> withdrawn;  // releases taken back: who has one is offered package.version
    // 3.0.17-beta.1 (the concept «простой старт»: a rollback to a recent version): [history] v1..v10 = "<version>
    // <ref> <size> <sha256>" - the packages before this one (up to 5 releases and 5 betas: a release is made on the
    // branch beta), the newest first; a broken line left out. Offered by the computer's channel (rollbackChoices)
    std::vector<Archive> history;
};
// false and why (the field it names) for anything else - an error page, a cut file, another format (uaro.kiev.ua's
// format 1). Sections it does not know (the settings app's [local]) are left alone.
constexpr int kMostHistory = 10;  // 3.0.17-beta.1: [history]'s lines - 5 releases and 5 betas
bool parseManifest(const std::string& text, Manifest& out, std::string& error);
// The version to offer an installed `installed`: the branch's when it is newer, or when `installed` was withdrawn (even
// an older one); "" none, or when it is `skip` (a version rolled back from). 3.0.5-beta.1 (the user: «я выбрал канал
// релиз, и нажал играть. и у меня запустилась моя бета»): on «Реліз» a beta is offered the release, an older one too.
std::string offer(const Manifest& m, const std::string& installed, const std::string& skip, Channel channel);
// 3.0.17-beta.1: the versions a rollback offers - [history]'s older than `installed`, of the channel («Реліз»: no
// betas), the newest first, at most kMostRollback
constexpr int kMostRollback = 5;
std::vector<Archive> rollbackChoices(const Manifest& m, Channel channel, const std::string& installed);
// The offer takes a beta back to an older release - «Реліз» chosen over it: the texts say «Повернутися на реліз».
bool backToRelease(Channel channel, const std::string& offered, const std::string& installed);
// uaro-graphics\update.ini as the settings app keeps it: the branch's text, then "[local]" - skip (the version rolled
// back from) and 3.0.5-beta.1 channel (the one the branch was asked for).
std::string keptInfo(std::string branch, const std::string& skip, Channel channel);
// Its channel: [local] channel's, else channelOf's for `installed` (an older app kept none).
Channel keptChannel(const std::string& updateInfo, const std::string& installed);
// The game's start: uaro-graphics\update.ini as the last check kept it (the branch's text, then [local] skip) offers
// `version` to `installed` - the driver then asks through "uaro-graphics.exe --ask-update", whose exit code
// kUpdateChosen closes the game (the settings app updates it and starts it again).
bool askAtStart(const std::string& updateInfo, const std::string& installed, std::string& version);
constexpr unsigned long kUpdateChosen = 10;
// The setup (uaro-graphics-setup.exe) is uaro-graphics.exe with the package appended by tools/package.sh: the zip, its
// size in eight bytes (little-endian), "UAROSETP". From the file's last 16 bytes and its size: where the zip is.
bool payloadAt(const uint8_t* tail16, uint64_t fileSize, uint64_t& offset, uint64_t& size);

}  // namespace uagfx::update
