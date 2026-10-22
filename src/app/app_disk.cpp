#include "app/app_disk.hpp"

#include <algorithm>

#include "app/files.hpp"
#include "app/text.hpp"

namespace uagfx::app {

bool AppDisk::isFile(const std::string& path) { return fileExists(wide(path)); }

bool AppDisk::isFolder(const std::string& path) { return dirExists(wide(path)); }

bool AppDisk::makeFolder(const std::string& path) { return app::makeFolder(wide(path)); }

bool AppDisk::move(const std::string& from, const std::string& to, bool replace) {
    return moveFile(wide(from), wide(to), replace);
}

std::vector<std::string> AppDisk::names(const std::string& folder) {
    std::vector<std::string> out;
    for (const bool folders : {false, true})
        for (const std::wstring& name : listFolder(wide(folder), folders)) out.push_back(utf8(name));
    std::sort(out.begin(), out.end());
    return out;
}

bool AppDisk::removeFile(const std::string& path) { return deleteFile(wide(path)); }

bool AppDisk::removeEmptyFolder(const std::string& path) { return app::removeEmptyFolder(wide(path)); }

bool AppDisk::readStart(const std::string& path, size_t most, std::string& out) {
    if (!readFile(wide(path), out)) return false;  // the notes it looks at are small
    if (out.size() > most) out.resize(most);
    return true;
}

std::wstring ourFile(const std::wstring& dir, layout::File f) {
    const auto there = [](const std::string& p) { return fileExists(wide(p)) || dirExists(wide(p)); };
    return wide(layout::locate(utf8(dir), f, there));
}

std::wstring ourPlace(const std::wstring& dir, const std::string& relative) {
    return wide(layout::join(utf8(dir), relative));
}

}  // namespace uagfx::app
