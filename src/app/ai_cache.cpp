#include "app/ai_cache.hpp"

#include <windows.h>

#include <unordered_set>

#include "app/text.hpp"
#include "core/texture_pack.hpp"

namespace uagfx::app {

uint64_t fileBytes(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &a)) return 0;
    return uint64_t(a.nFileSizeHigh) << 32 | a.nFileSizeLow;
}

bool readPackIndex(const std::wstring& path, pack::Index& out) {
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    pack::Reader r;
    std::string error;
    std::vector<uint8_t> data;
    if (!r.open(ansi(path), error) || !r.read(pack::kIndexName, data)) return false;
    out = pack::parseIndexFile(std::string(data.begin(), data.end()));
    return true;
}

PackFile readPack(const std::wstring& path) {
    PackFile f;
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return f;
    f.exists = true;
    f.bytes = fileBytes(path);
    pack::Reader r;
    if (!r.open(ansi(path), f.error)) return f;
    std::vector<uint8_t> data;
    const pack::Index index =
        r.read(pack::kIndexName, data) ? pack::parseIndexFile(std::string(data.begin(), data.end())) : pack::Index();
    for (const std::string& name : r.names()) {
        if (name == pack::kIndexName) continue;
        ai::CacheEntry e;
        e.name = name;
        e.bytes = r.sizeOf(name);
        const auto it = index.pictures.find(name);
        if (it != index.pictures.end()) {
            e.kind = it->second.kind;
            e.maps = it->second.maps;
        }
        f.entries.push_back(std::move(e));
    }
    return f;
}

std::string rewritePack(const std::wstring& path, const std::vector<std::string>& keep) {
    if (keep.empty()) return removeFile(path);
    const std::unordered_set<std::string> names(keep.begin(), keep.end());
    const std::wstring temp = path + L".new";
    std::string error;
    size_t kept = 0;
    bool ok = pack::rewrite(ansi(path), ansi(temp), [&](const std::string& n) { return names.count(n) > 0; }, kept,
                            error);
    if (ok && !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = "cannot replace it (error " + std::to_string(GetLastError()) + ")";
        ok = false;
    }
    if (!ok) {
        DeleteFileW(temp.c_str());
        return error.empty() ? "cannot write uaro-textures.zip.new" : error;
    }
    return "";
}

std::string removeFile(const std::wstring& path) {
    if (DeleteFileW(path.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND) return "";
    return "cannot delete it (error " + std::to_string(GetLastError()) + ")";
}

bool packInUse() {
    for (const wchar_t* name : {L"Local\\uaro-textures-zip", L"Local\\uaro-texture-pack-builder"}) {
        HANDLE h = OpenMutexW(SYNCHRONIZE, FALSE, name);
        if (!h) continue;
        CloseHandle(h);
        return true;
    }
    return false;
}

}  // namespace uagfx::app
