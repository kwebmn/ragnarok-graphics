#include "app/reference_pack.hpp"

#include <ctime>
#include <vector>

#include "app/download.hpp"
#include "app/files.hpp"
#include "app/text.hpp"
#include "core/layout.hpp"
#include "core/reference_live.hpp"
#include "core/world_map.hpp"

namespace uagfx::app {

namespace {

constexpr const char* kManifestPath = "/api/public/game/pack.json";
constexpr const char* kOnlinePath = "/api/public/online.json";  // 3.0.11-beta.10: the site's «Онлайн»
constexpr const char* kWorldPath = "/assets/worldmap/worldmap.json";  // 3.0.11-beta.11: the site's world map
constexpr long long kOnlineAgain = 90;  // seconds: younger counts are kept (several game windows, several watchers)

std::wstring folderOf(const std::wstring& dir) { return dir + L"\\" + wide(layout::kReference); }
std::wstring manifestOf(const std::wstring& dir) { return dir + L"\\" + wide(layout::kReferenceManifest); }

std::optional<refpack::Manifest> manifestThere(const std::wstring& dir) {
    std::string text;
    refpack::Manifest m;
    if (!readFile(manifestOf(dir), text) || !refpack::parse(text, m)) return std::nullopt;
    return m;
}

}  // namespace

std::optional<refpack::Manifest> referenceThere(const std::wstring& dir) {
    const auto m = manifestThere(dir);
    if (!m || fileSize(folderOf(dir) + L"\\" + wide(refpack::fileName(*m))) != (long long)m->size) return std::nullopt;
    return m;
}

bool noteReference(const std::wstring& dir, std::string& error) {
    std::string text;
    if (!fetchPath(kManifestPath, text, error, 4u << 10)) return false;
    refpack::Manifest site;
    if (!refpack::parse(text, site)) {
        error = "uaro.kiev.ua: not a reference pack's manifest";
        return false;
    }
    const std::wstring folder = folderOf(dir);
    const auto there = manifestThere(dir);
    const long long have = there ? fileSize(folder + L"\\" + wide(refpack::fileName(*there))) : -1;
    if (!refpack::needs(site, there, have)) return true;

    makeFolder(dir + L"\\" + wide(layout::kOurs));  // a client the app never ran in yet
    makeFolder(folder);
    const std::wstring name = wide(refpack::fileName(site));
    const std::wstring file = folder + L"\\" + name;
    if (!downloadPath(refpack::sitePath(site), file, [](uint64_t, uint64_t) { return true; }, error)) return false;
    std::string bytes;
    if (!readFile(file, bytes) || !refpack::verify(bytes, site)) {
        deleteFile(file);  // a pack of other bytes: never kept, the next start fetches it whole
        error = "uaro.kiev.ua: the reference pack differs from its manifest (size or SHA-256)";
        return false;
    }
    if (!writeFileAtomic(manifestOf(dir), refpack::write(site))) {
        error = "the reference's manifest could not be written";
        return false;
    }
    for (const std::wstring& n : listFolder(folder, false)) {  // older packs and pieces of cut downloads go
        if (n != name && n.rfind(L"pack-", 0) == 0) deleteFile(folder + L"\\" + n);
    }
    return true;
}

bool noteWorld(const std::wstring& dir, std::string& error) {
    std::string text, there;
    if (!fetchPath(kWorldPath, text, error, 1u << 20)) return false;
    worldmap::Sheet sheet;
    if (!worldmap::parse(text, sheet)) {
        error = "uaro.kiev.ua: not the world map's sheet";
        return false;
    }
    const std::wstring file = dir + L"\\" + wide(layout::kReferenceWorld);
    if (readFile(file, there) && there == text) return true;
    makeFolder(dir + L"\\" + wide(layout::kOurs));
    makeFolder(folderOf(dir));
    if (!writeFileAtomic(file, text)) {
        error = "the world map could not be written";
        return false;
    }
    return true;
}

bool noteOnline(const std::wstring& dir, std::string& error) {
    std::string text;
    reflive::Online there;  // another game window's watcher asked a moment ago: once for all of them
    const long long now = static_cast<long long>(std::time(nullptr));
    if (readFile(dir + L"\\" + wide(layout::kReferenceOnline), text) && reflive::read(text, there) &&
        now - there.at >= 0 && now - there.at < kOnlineAgain)
        return true;
    if (!fetchPath(kOnlinePath, text, error, 1u << 20)) return false;
    reflive::Online o;
    if (!reflive::fromSite(text, now, o)) {
        error = "uaro.kiev.ua: not the players online";
        return false;
    }
    makeFolder(dir + L"\\" + wide(layout::kOurs));
    makeFolder(folderOf(dir));
    if (!writeFileAtomic(dir + L"\\" + wide(layout::kReferenceOnline), reflive::write(o))) {
        error = "the players online could not be written";
        return false;
    }
    return true;
}

}  // namespace uagfx::app
