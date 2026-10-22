#pragma once
#include <string>
#include <vector>

#include "core/layout_migration.hpp"

namespace uagfx::win {

// 2.8: the game's folder for core/layout_migration - Windows' ANSI calls, as the rest of the driver. A move is one
// rename (MoveFileExA without MOVEFILE_COPY_ALLOWED): a texture pack of gigabytes is never copied.
class WinDisk : public layout::Disk {
public:
    bool isFile(const std::string& path) override;
    bool isFolder(const std::string& path) override;
    bool makeFolder(const std::string& path) override;
    bool move(const std::string& from, const std::string& to, bool replace) override;
    std::vector<std::string> names(const std::string& folder) override;
    bool removeFile(const std::string& path) override;
    bool removeEmptyFolder(const std::string& path) override;
    bool readStart(const std::string& path, size_t most, std::string& out) override;
};

}  // namespace uagfx::win
