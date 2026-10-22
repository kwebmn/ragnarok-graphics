#pragma once
#include <cstdint>
#include <functional>
#include <string>

#include "core/update_check.hpp"

namespace uagfx::app {

// 2.10: updates with no .bat and no console (core/update_check; 3.0.0-beta.2: from the GitHub branch of the computer's
// channel): what the settings app's update, rollback, repair and the downloads of DLSS and the AI models run - on a
// thread of the progress window.

struct Step {  // where it is, for the progress window
    enum What { Download, Wait, Install, Dlss, Models, Done } what = Download;
    uint64_t done = 0, total = 0;  // the downloads: bytes
};
using Report = std::function<bool(const Step&)>;  // false: stop - before any file of the game's folder changed

struct CheckResult {
    bool ok = false;
    update::Channel channel = update::Channel::Release;  // 3.0.0-beta.2: the branch asked
    update::Manifest branch;                             // its update.ini
    std::string offer;  // the version to offer, "" none
    std::string error;  // !ok: why, in English
};
// 3.0.0-beta.2: the computer's channel - uaro-graphics.ini's [update] channel, else by the installed build
// (update::channelOf).
update::Channel channelOf(const std::wstring& dir);
// The update.ini of the channel's branch read now and kept as uaro-graphics\update.ini - its text, then this
// computer's [local] section - for the game's next start; the version to offer what is installed.
CheckResult checkNow(const std::wstring& dir);
std::string installedVersion(const std::wstring& dir);  // uaro-graphics\version.txt's, else this app's
std::string skipped(const std::wstring& dir);  // [local] skip: the version rolled back from, "" none
void setSkipped(const std::wstring& dir, const std::string& version);
bool nvidiaHere();  // a card of NVIDIA's among the adapters (DLSS)

// The branch's package downloaded into uaro-graphics\update (kept while it is the named one: a stop does not lose
// it), checked (its size and SHA-256), unpacked; the game waited for; put in (app/installer, all or nothing) and
// finished; then DLSS on an NVIDIA card when its version is not the branch's (a failure there leaves DLSS as it was).
// `repair`: installer::putPackage's. false and why for the player.
bool runUpdate(const std::wstring& dir, const update::Manifest& m, bool repair, const Report& report,
               std::wstring& error);
// The setup's: the package it carries (the zip's bytes) put in the same way; DLSS from the branch when it answers.
bool runInstall(const std::wstring& dir, std::string zip, const Report& report, std::wstring& error);
bool runDlss(const std::wstring& dir, const update::Manifest& m, const Report& report, std::wstring& error);
bool runModels(const std::wstring& dir, const update::Manifest& m, const Report& report, std::wstring& error);

}  // namespace uagfx::app
