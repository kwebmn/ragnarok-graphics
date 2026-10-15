#pragma once
#include <string>

namespace uagfx::update {

// 0.49: the settings app's update check - GitHub's newest commit of main (by its number: no cache keeps that old, as
// update.bat takes it) and the version its src/core/version.cpp names.
constexpr const char* kHost = "api.github.com";
constexpr const char* kCommitPath = "/repos/kwebmn/ragnarok-graphics/commits/main";
constexpr const char* kRawHost = "raw.githubusercontent.com";
std::string versionPath(const std::string& commit);  // "/kwebmn/ragnarok-graphics/<commit>/src/core/version.cpp"

bool isCommit(const std::string& text);              // 40 hex digits: the API's answer to "Accept: ...sha"
std::string versionOf(const std::string& versionCpp);  // its kVersion; "" none
bool newer(const std::string& a, const std::string& b);  // a is a later version than b (by their numbers)

}  // namespace uagfx::update
