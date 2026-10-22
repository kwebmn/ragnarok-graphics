#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::update {

// 0.49: the settings app's update check - GitHub's newest commit of main (by its number: no cache keeps that old, as
// update.bat takes it) and the version its src/core/version.cpp names.
constexpr const char* kHost = "api.github.com";
constexpr const char* kCommitPath = "/repos/kwebmn/ragnarok-graphics/commits/main";
constexpr const char* kRawHost = "raw.githubusercontent.com";
std::string versionPath(const std::string& commit);  // "/kwebmn/ragnarok-graphics/<commit>/src/core/version.cpp"

bool isCommit(const std::string& text);              // 40 hex digits: the API's answer to "Accept: ...sha"
std::string versionOf(const std::string& versionCpp);  // its kVersion; "" none
// a is a later version than b (by their numbers); 2.10: a pre-release ("3.0.0-beta.2") comes before its release
bool newer(const std::string& a, const std::string& b);
std::string bare(const std::string& version);  // "2.10.0 build 2026-10-22 18:00" -> "2.10.0"

// 2.10: updates come from uaro.kiev.ua - its update.ini (written by tools/publish_site.py at a release) names the
// newest release and the archives beside it: [release] format = 1, version, withdrawn (versions between spaces);
// [package] file, size, sha256; [dlss] and [models] the same and their own version line. Every release to everyone.
constexpr const char* kSite = "uaro.kiev.ua";
constexpr const char* kFiles = "/files/graphics/";  // update.ini and the archives
constexpr const char* kManifest = "update.ini";
struct Archive {
    std::string version, file, sha256;  // file "": none; sha256 64 lower-case hex digits
    uint64_t size = 0;
};
struct Manifest {
    Archive package, dlss, models;
    std::vector<std::string> withdrawn;  // releases taken back: who has one is offered package.version
};
// false and why (the field it names) for anything else - an error page, a cut file, another format. Sections it does
// not know (the settings app's [local]) are left alone.
bool parseManifest(const std::string& text, Manifest& out, std::string& error);
// The version to offer an installed `installed`: the site's when it is newer, or when `installed` was withdrawn (even
// an older one); "" none, or when it is `skip` (a version rolled back from).
std::string offer(const Manifest& m, const std::string& installed, const std::string& skip);

}  // namespace uagfx::update
