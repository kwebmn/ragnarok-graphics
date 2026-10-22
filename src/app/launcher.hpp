#pragma once
#include <string>

#include "core/launcher.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::app {

// 2.11: the launcher «Гра» - the site's news and online count (core/launcher), asked on a thread at the app's start;
// no answer leaves «Грати» working.
struct LauncherState {
    enum Kind { Idle, Asking, Done, Failed } kind = Idle;
    launcher::Info info;
    std::string error;  // Failed: why, in English
};
LauncherState launcherNow(settings::Lang l);  // asked now
void startLauncherCheck(settings::Lang l);    // launcherNow on a thread (app/launcher_thread.cpp)
LauncherState launcherState();

}  // namespace uagfx::app
