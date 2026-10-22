#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include "core/layout.hpp"

namespace uagfx::layout {

// The file system as the move sees it: Windows' in the driver (ANSI paths) and in the settings app (UTF-8 paths,
// wide calls), memory in the tests.
struct Disk {
    virtual ~Disk() = default;
    virtual bool isFile(const std::string& path) = 0;
    virtual bool isFolder(const std::string& path) = 0;
    virtual bool makeFolder(const std::string& path) = 0;  // one level; true when it is there
    // One rename; replace: a file already at `to` goes. A folder moves whole. false: nothing moved.
    virtual bool move(const std::string& from, const std::string& to, bool replace) = 0;
    virtual std::vector<std::string> names(const std::string& folder) = 0;  // its files and folders, sorted
    virtual bool removeFile(const std::string& path) = 0;
    virtual bool removeEmptyFolder(const std::string& path) = 0;
    virtual bool readStart(const std::string& path, size_t most, std::string& out) = 0;  // its first bytes
};

// 2.8: a 2.x install's files moved where layout::now keeps them, at the driver's and the settings app's start - a
// second run finds nothing to do. One line for each thing moved, removed or left ("layout: ..."), for the log. What
// cannot move stays where it is (locate finds it there); a player's file is never deleted, only our own TEST.*.md
// notes; the texture pack never.
std::vector<std::string> migrate(const std::string& gameDir, Disk& disk);

}  // namespace uagfx::layout
