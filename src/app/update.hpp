#pragma once
#include <string>

namespace uagfx::app {

// 0.49: the Service page's update - the newest version on GitHub, asked on a thread of its own (WinHTTP), and
// update.bat started in a console of its own (it waits for the settings app to close and replaces it too).
struct UpdateState {
    enum Kind { Idle, Checking, Done, Failed } kind = Idle;
    std::string latest;  // Done: the newest version
    std::string error;   // Failed: why, in English
};
void startUpdateCheck();
UpdateState updateState();
// update.bat of the game's folder, started; "" or why not, in English.
std::string startUpdate(const std::wstring& gameDir);
// 2.5.27: get-models.bat likewise - the AI models that come apart (Game AI 2.0), downloaded in its console.
std::string startGetModels(const std::wstring& gameDir);

}  // namespace uagfx::app
