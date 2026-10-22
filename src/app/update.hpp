#pragma once
#include <string>

#include "core/update_check.hpp"

namespace uagfx::app {

// 0.49: the newest version, asked on a thread of its own; 2.10: from uaro.kiev.ua (app/update_run checkNow, which
// keeps uaro-graphics\update.ini for the game's start).
struct UpdateState {
    enum Kind { Idle, Checking, Done, Failed } kind = Idle;
    std::string latest;  // Done: the site's newest version
    std::string offer;   // Done: the version to offer what is installed, "" none
    update::Manifest site;
    std::string error;  // Failed: why, in English
};
void startUpdateCheck();  // for the folder of this app
UpdateState updateState();

}  // namespace uagfx::app
