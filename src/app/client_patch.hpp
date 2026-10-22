#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/client_patch.hpp"

namespace uagfx::app {

// 2.14: the client patcher (core/client_patch) - the game's files of «Оновлення клієнта» on uaro.kiev.ua: «Гра» asks
// on its thread, «Грати» brings what is missing first, while the game is closed. Errors in English.
struct PatchCheck {
    bool asked = false;  // the site answered with a manifest
    std::string error;
    client_patch::Manifest manifest;
    std::vector<client_patch::File> needed;
    uint64_t bytes = 0;  // of the needed files
};
// The site's list against the disk: the listed files hashed only when the record of the last put does not match
PatchCheck checkClientPatch(const std::wstring& dir);
// Every needed file fetched into uaro-graphics\update\client and checked (size, sha256) before any is put; then put
// in their places (folders made) and recorded. false and why: the game running, a fetch or a check failed - the
// files that were there stay.
bool applyClientPatch(const std::wstring& dir, const PatchCheck& c,
                      const std::function<bool(uint64_t done, uint64_t total)>& progress, std::string& error);

}  // namespace uagfx::app
