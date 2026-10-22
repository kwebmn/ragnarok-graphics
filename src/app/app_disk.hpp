#pragma once
#include <string>
#include <vector>

#include "core/layout_migration.hpp"

namespace uagfx::app {

// 2.8: the client folder for core/layout_migration through app/files (the tests' stand-ins too): UTF-8 paths in,
// wide calls out.
class AppDisk : public layout::Disk {
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

// 2.8: a file of ours (core/layout) in the client folder: where 2.8 keeps it, or where 2.x kept it when it could not
// move (a game under Program Files, a pack held open).
std::wstring ourFile(const std::wstring& dir, layout::File f);
// A place of core/layout (now's) in the client folder.
std::wstring ourPlace(const std::wstring& dir, const std::string& relative);

}  // namespace uagfx::app
