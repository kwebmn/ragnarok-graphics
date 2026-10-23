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
    bool asked = false;  // the site answered with a manifest of this folder's client
    std::string error;
    client_patch::Manifest manifest, last;  // 3.0.1: last - the record of the last put
    std::vector<client_patch::File> needed;
    std::vector<client_patch::File> kept, remove;  // 3.0.1: the player's own left; withdrawn ones to take away
    uint64_t bytes = 0;  // of the needed files
    bool work() const { return !needed.empty() || !remove.empty(); }
};
// 3.0.1: this folder's client - the build's uaro-client.json (its name made fit), else the old «uaRO» with no files
client_patch::Manifest clientOf(const std::wstring& dir);
// The site's list of this folder's client against the disk: the listed files hashed only when the record of the last
// put does not match; a list with nothing to do is recorded at once
PatchCheck checkClientPatch(const std::wstring& dir);
// Every needed file fetched into uaro-graphics\update\client and checked (size, sha256) before any is put; then put
// in their places (folders made), the withdrawn ones taken away, and recorded. false and why: the game, its renderer
// or uaRO Studio of this folder running, a fetch or a check failed - the files that were there stay.
bool applyClientPatch(const std::wstring& dir, const PatchCheck& c,
                      const std::function<bool(uint64_t done, uint64_t total)>& progress, std::string& error);

// «Гра»'s check on a thread of its own (app/client_patch_thread.cpp): at the app's start and after a put
struct PatchState {
    enum Kind { Idle, Asking, Done } kind = Idle;
    PatchCheck check;
};
void startClientPatchCheck(const std::wstring& dir);
PatchState clientPatchState();

}  // namespace uagfx::app
