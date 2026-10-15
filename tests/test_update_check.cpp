#include <string>

#include "core/update_check.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(update_check_reads_the_newest_version_from_github) {
    // 0.49: the settings app asks GitHub for main's newest commit and reads the version from its version.cpp.
    CHECK(update::versionOf("namespace uagfx {\nconst char* const kVersion = \"0.49.0\";\n}\n") == "0.49.0");
    CHECK(update::versionOf("no version here").empty());
    CHECK(update::versionOf("kVersion = \"0.49.0\n").empty());  // cut short
    CHECK(update::isCommit("b56151fd990d987430195f6a8d8bb0ec3a48f383"));
    CHECK(!update::isCommit("b56151fd"));
    CHECK(!update::isCommit("{\"message\": \"API rate limit exceeded\"}"));
}

TEST(update_check_compares_versions_by_their_numbers) {
    CHECK(update::newer("0.49.0", "0.48.0"));
    CHECK(update::newer("0.49.10", "0.49.9"));  // numbers, not text
    CHECK(update::newer("1.0.0", "0.99.99"));
    CHECK(!update::newer("0.48.0", "0.48.0"));
    CHECK(!update::newer("0.47.1", "0.48.0"));
    CHECK(!update::newer("", "0.48.0"));
}
