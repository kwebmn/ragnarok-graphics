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

// A client's name in a query: a space as %20 (clientName lets only letters, digits and " -_.()" through)
std::string query(const std::string& name) {
    std::string out;
    for (char c : name) out += c == ' ' ? std::string("%20") : std::string(1, c);
    return out;
}

// The list put recorded: what the patch made (nothing was at its place) - the last record's and this put's
bool writeRecord(const std::wstring& dir, const PatchCheck& c, const std::set<std::string>& made) {
    client_patch::Manifest m = c.manifest;
    client_patch::carryMade(m, c.last);
    for (client_patch::File& f : m.files) f.made = f.made || made.count(f.path) > 0;
    return writeFileAtomic(join(dir, wide(layout::kClientPatchRecord)), client_patch::record(m));
}

}  // namespace

client_patch::Manifest clientOf(const std::wstring& dir) {
    client_patch::Manifest m;
    std::string text, error;
    if (!readFile(join(dir, wide(client_patch::kClientFile)), text) || !client_patch::parse(text, m, error))
        m = client_patch::Manifest{};
    m.client = client_patch::clientName(m.client);
    return m;
}

PatchCheck checkClientPatch(const std::wstring& dir) {
    PatchCheck c;
    const client_patch::Manifest build = clientOf(dir);
    std::string text;
    if (!fetchPath(std::string(client_patch::kPath) + "?client=" + query(build.client), text, c.error, 4u << 20) ||
        !client_patch::parse(text, c.manifest, c.error))
        return c;
    if (!client_patch::sameClient(c.manifest.client, build.client)) {  // 3.0.1: never another client's files
        c.error = "the site's list is for " + c.manifest.client + ", this game is " + build.client;
        c.manifest = client_patch::Manifest{};
        return c;
    }
    c.asked = true;
    std::string record;
    readFile(join(dir, wide(layout::kClientPatchRecord)), record);
    client_patch::readRecord(record, c.last);
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
    const client_patch::Plan plan = client_patch::plan(c.manifest, c.last, build, look);
    c.needed = plan.put;
    c.kept = plan.kept;
    c.remove = plan.remove;
    for (const client_patch::File& f : c.needed) c.bytes += f.size;
    if (!c.work()) writeRecord(dir, c, {});  // nothing to put or take away: not hashed again at the next start
    return c;
}

bool applyClientPatch(const std::wstring& dir, const PatchCheck& c,
                      const std::function<bool(uint64_t done, uint64_t total)>& progress, std::string& error) {
    if (!c.asked) return true;  // no site: the game starts as it is
    std::set<std::string> made;  // the files put where nothing was
    if (c.work()) {
        // 3.0.1: the game, its renderer and uaRO Studio of THIS folder hold the game's archives (the other client's may
        // run from its own folder)
        const std::pair<const wchar_t*, const char*> holders[] = {
            {L"uaRO.exe", "the game is running - its files are put once it is closed"},
            {L"uaro-graphics\\uaro-render64.exe", "uaro-render64.exe still runs - the files are put once it ends"},
            {L"uaro-graphics\\uaro-studio.exe", "uaRO Studio is open - the game's files are put once it is closed"}};
        for (const auto& h : holders)
            if (running(join(dir, h.first))) {
                error = h.second;
                return false;
            }
    }
    if (!c.needed.empty()) {
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
            const std::wstring from = join(work, wide(f.sha256)), place = placeOf(dir, f.path);
            const bool there = fileExists(place);
            std::string bytes;  // a file wanted at another place after this one: copied, the last one moved
            bool put = makeFoldersFor(dir, f.path);
            if (put && again)
                put = readFile(from, bytes) && writeFileAtomic(place, bytes);
            else if (put)
                put = moveFile(from, place, true);
            if (!put) {
                error = "cannot put " + f.path + " in its place";
                return false;
            }
            if (!there) made.insert(f.path);
        }
        removeTree(work);
    }
    for (const client_patch::File& f : c.remove)  // 3.0.1: the withdrawn ones the patch made
        if (!deleteFile(placeOf(dir, f.path)) && fileExists(placeOf(dir, f.path))) {
            error = "cannot take " + f.path + " away";
            return false;
        }
    if (!writeRecord(dir, c, made)) {
        error = "cannot write " + std::string(layout::kClientPatchRecord);
        return false;
    }
    return true;
}

}  // namespace uagfx::app
