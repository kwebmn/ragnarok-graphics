#include "app/client_patch.hpp"

#include <algorithm>
#include <set>

#include "app/download.hpp"
#include "app/files.hpp"
#include "app/system.hpp"
#include "app/text.hpp"
#include "core/layout.hpp"
#include "core/sha256.hpp"

namespace uagfx::app {
namespace {

std::wstring join(const std::wstring& a, const std::wstring& b) { return a + L"\\" + b; }
std::wstring placeOf(const std::wstring& dir, const std::string& path) {
    return join(dir, wide(client_patch::localPath(path)));
}

// The folders of a path under the game's folder, each made (also when there)
bool makeFoldersFor(const std::wstring& dir, const std::string& path) {
    const std::string local = client_patch::localPath(path);
    std::wstring at = dir;
    for (size_t from = 0, slash; (slash = local.find('\\', from)) != std::string::npos; from = slash + 1) {
        at = join(at, wide(local.substr(from, slash - from)));
        if (!makeFolder(at)) return false;
    }
    return true;
}

bool makeWork(const std::wstring& dir) {
    std::wstring at = dir;
    for (const wchar_t* part : {L"uaro-graphics", L"update", L"client"}) {
        at = join(at, part);
        if (!makeFolder(at)) return false;
    }
    return true;
}

}  // namespace

PatchCheck checkClientPatch(const std::wstring& dir) {
    PatchCheck c;
    std::string text;
    if (!fetchPath(client_patch::kPath, text, c.error) || !client_patch::parse(text, c.manifest, c.error)) return c;
    c.asked = true;
    std::string record;
    readFile(join(dir, wide(layout::kClientPatchRecord)), record);
    const auto sizeOf = [&](const std::string& path) { return fileSize(placeOf(dir, path)); };
    if (client_patch::upToDate(c.manifest, record, sizeOf)) return c;
    const auto look = [&](const std::string& path, const std::vector<uint64_t>& sizes) {
        client_patch::Have h;
        const long long size = sizeOf(path);
        h.exists = size >= 0;
        h.size = h.exists ? uint64_t(size) : 0;
        std::string bytes;
        if (h.exists && std::find(sizes.begin(), sizes.end(), h.size) != sizes.end() &&
            readFile(placeOf(dir, path), bytes))
            h.sha256 = sha256::hexOf(bytes);
        return h;
    };
    c.needed = client_patch::plan(c.manifest, {}, {}, look).put;
    for (const client_patch::File& f : c.needed) c.bytes += f.size;
    return c;
}

bool applyClientPatch(const std::wstring& dir, const PatchCheck& c,
                      const std::function<bool(uint64_t done, uint64_t total)>& progress, std::string& error) {
    if (!c.asked) return true;  // no site: the game starts as it is
    if (!c.needed.empty()) {
        if (gameRunning(join(dir, L"uaRO.exe"))) {
            error = "the game is running - its files are put once it is closed";
            return false;
        }
        if (!makeWork(dir)) {
            error = "cannot make " + std::string(layout::kClientPatchWork);
            return false;
        }
        const std::wstring work = join(dir, wide(layout::kClientPatchWork));
        uint64_t done = 0;
        std::set<std::string> fetched;  // one picture for two places comes once
        for (const client_patch::File& f : c.needed) {  // all of them here and checked before any is put
            if (!fetched.insert(f.sha256).second) continue;
            const std::wstring to = join(work, wide(f.sha256));
            const auto step = [&](uint64_t got, uint64_t) { return progress(done + got, c.bytes); };
            std::string bytes;
            if (!downloadPath(std::string(client_patch::kFiles) + f.sha256, to, step, error)) return false;
            if (!readFile(to, bytes) || bytes.size() != f.size || sha256::hexOf(bytes) != f.sha256) {
                deleteFile(to);
                error = f.path + " arrived damaged - try again";
                return false;
            }
            done += f.size;
        }
        for (size_t i = 0; i < c.needed.size(); ++i) {
            const client_patch::File& f = c.needed[i];
            const bool again = std::count_if(c.needed.begin() + std::ptrdiff_t(i) + 1, c.needed.end(),
                                             [&](const client_patch::File& g) { return g.sha256 == f.sha256; }) > 0;
            const std::wstring from = join(work, wide(f.sha256));
            std::string bytes;  // a file wanted at another place after this one: copied, the last one moved
            const bool put = makeFoldersFor(dir, f.path) &&
                             (again ? readFile(from, bytes) && writeFileAtomic(placeOf(dir, f.path), bytes)
                                    : moveFile(from, placeOf(dir, f.path), true));
            if (!put) {
                error = "cannot put " + f.path + " in its place";
                return false;
            }
        }
        removeTree(work);
    }
    if (!writeFileAtomic(join(dir, wide(layout::kClientPatchRecord)), client_patch::record(c.manifest))) {
        error = "cannot write " + std::string(layout::kClientPatchRecord);
        return false;
    }
    return true;
}

}  // namespace uagfx::app
