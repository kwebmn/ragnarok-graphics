#pragma once
#include <string>

#include "core/update_check.hpp"

namespace uagfx::app {

// 0.49: the newest version, asked on a thread of its own (app/update_run checkNow, which keeps uaro-graphics\update.ini
// for the game's start); 3.0.0-beta.2: from the GitHub branch of the computer's channel.
struct UpdateState {
    enum Kind { Idle, Checking, Done, Failed } kind = Idle;
    std::string latest;  // Done: the branch's newest version
    std::string offer;   // Done: the version to offer what is installed, "" none
    update::Manifest branch;  // the update.ini of the computer's channel
    std::string error;  // Failed: why, in English
};
void startUpdateCheck();  // for the folder of this app
UpdateState updateState();

}  // namespace uagfx::app
