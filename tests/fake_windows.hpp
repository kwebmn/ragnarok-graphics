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
    uagfx::settings::GameRegistry registry;
    std::vector<uagfx::app::CompatEntry> compat;
    bool gameRunning = false;
    uagfx::app::SystemInfo system;
    int clock = 0;  // seconds after 2026-10-13 12:00:00: backup folders are named after it
};

// A fresh one: the client folder C:\uaRO, the main monitor 3840 x 2160 at 175 %, no game key, no files.
Windows& reset();
Windows& windows();

std::wstring path(const std::wstring& relative);  // in the client folder
// The game's key as setup.exe leaves it for a window of `width` x `height`, in the copy given.
void gameKey(uagfx::settings::RegCopy& copy, int width, int height, bool fullscreen);

}  // namespace fake
