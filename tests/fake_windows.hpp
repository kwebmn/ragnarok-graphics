#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

#include "app/registry.hpp"
#include "app/system.hpp"

namespace fake {

// The Windows the settings app's model sees in the tests: link-time stand-ins for src/app/files, registry, system and
// the UTF-8 conversions (tests/fake_windows.cpp). Files and folders in memory, the two copies of the game's key,
// whether the game runs.
struct Windows {
    std::map<std::wstring, std::string> files;  // full path -> bytes
    std::set<std::wstring> folders;             // full paths
    std::set<std::wstring> locked;              // 2.8: held open - moving them fails (a pack being made)
    uagfx::settings::GameRegistry registry;
    std::vector<uagfx::app::CompatEntry> compat;
    std::set<std::string> refused;  // value names the registry refuses to write (error 5)
    bool gameRunning = false;
    uagfx::app::SystemInfo system;
    // 2.10: the entry in Windows' Apps (app/registry writeAppsEntry): its values, empty when there is none
    std::map<std::wstring, std::wstring> appsEntry;
    // 2.10: the web (app/download) - a path of uaro.kiev.ua (fetchPath, downloadPath) or, 3.0.0-beta.2, a URL of
    // GitHub's (fetchUrl, downloadUrl) - and its bytes; waits: waitMs's calls (the game is closed by the first one)
    std::map<std::string, std::string> web;
    int waits = 0;
    // 2.13: the site's POSTs (app/download postPath): a path's status and answer - none there: no answer came; what
    // was posted; the keys randomKey made
    struct Post {
        std::string path, body;
    };
    std::map<std::string, std::pair<long, std::string>> postAnswers;
    std::vector<Post> posts;
    int keys = 0;
    // 2.11: what was started (launch's file), and whether this process marked it as ours (UARO_LAUNCHER)
    std::vector<std::wstring> launched;
    bool marked = false;
    int clock = 0;  // seconds after 2026-10-13 12:00:00: backup folders are named after it
};

// A fresh one: the client folder C:\uaRO, the main monitor 3840 x 2160 at 175 %, no game key, no files.
Windows& reset();
Windows& windows();

std::wstring path(const std::wstring& relative);  // in the client folder
// The game's key as setup.exe leaves it for a window of `width` x `height`, in the copy given.
void gameKey(uagfx::settings::RegCopy& copy, int width, int height, bool fullscreen);

}  // namespace fake
