#include "app/update_run.hpp"

#include <ctime>
#include <map>

#include "app/app_disk.hpp"
#include "app/download.hpp"
#include "app/files.hpp"
#include "app/installer.hpp"
#include "app/system.hpp"
#include "app/text.hpp"
#include "core/config.hpp"
#include "core/first_game.hpp"
#include "core/install_ping.hpp"
#include "core/layout.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/sha256.hpp"
#include "core/usage.hpp"
#include "core/version.hpp"
#include "core/zip.hpp"

namespace uagfx::app {
namespace {

std::wstring join(const std::wstring& a, const std::wstring& b) { return a + L"\\" + b; }
std::wstring workOf(const std::wstring& dir) { return join(dir, wide(layout::kUpdateWork)); }
std::wstring infoOf(const std::wstring& dir) { return join(dir, wide(layout::kUpdateInfo)); }

bool makeFolders(const std::wstring& path) {
    if (path.empty() || dirExists(path)) return true;
    const size_t slash = path.find_last_of(L'\\');
    return (slash == std::wstring::npos || makeFolders(path.substr(0, slash))) && makeFolder(path);
}

std::string firstLine(const std::wstring& path) {
    std::string text;
    if (!readFile(path, text)) return "";
    const size_t end = text.find_first_of("\r\n");
    return end == std::string::npos ? text : text.substr(0, end);
}

// uaro-graphics\update.ini: the branch's text, then "[local]" and this computer's choices (update::keptInfo).
void keepInfo(const std::wstring& dir, std::string branch, const std::string& skip, update::Channel channel) {
    writeFileAtomic(infoOf(dir), update::keptInfo(std::move(branch), skip, channel));
}

}  // namespace

// 3.0.11-beta.56 (the owner: «сделать» - how many installs and on which versions): the site's anonymous count - this
// install's random number, its version and channel at most every 12 hours (core/install_ping); not told again until
// the site took it. 3.0.11-beta.61 (the autopilot): with the first game's outcome once it is whole (core/first_game) -
// at once when `firstNow` (the watcher: that game just closed)
void pingInstall(const std::wstring& dir, update::Channel channel, bool firstNow) {
    const std::wstring idPath = join(dir, wide(layout::kInstallId)), sentPath = join(dir, wide(layout::kPingSent));
    const std::wstring firstPath = join(dir, wide(layout::kFirstGame));
    const std::wstring firstSent = join(dir, wide(layout::kFirstGameSent));
    std::string stamp, text, local, firstText, first;
    firstgame::Outcome outcome;
    if (!fileExists(firstSent) && readFile(firstPath, firstText) && firstgame::parse(firstText, outcome) &&
        firstgame::complete(outcome))
        first = firstgame::json(outcome);
    const long long now = static_cast<long long>(std::time(nullptr));
    readFile(sentPath, stamp);
    long long last = 0;
    usage::readSentStamp(stamp, last, local);  // the seconds it was told at (no atoll: msvcrt's _atoi64)
    if (!ping::due(now, last) && !(firstNow && !first.empty())) return;
    readFile(idPath, text);
    std::string id = ping::idOf(text);
    if (id.empty()) {  // made once: the clocks, a stack and a heap address mixed - a number, not a secret
        int here = 0;
        const std::string heap(64, ' ');
        id = ping::newId(uint64_t(now) ^ (uint64_t(std::clock()) << 32) ^
                         (uint64_t(reinterpret_cast<uintptr_t>(&here)) << 7) ^
                         (uint64_t(reinterpret_cast<uintptr_t>(heap.data())) << 23));
        if (!makeFolders(join(dir, wide(layout::kOurs))) || !writeFileAtomic(idPath, id + "\n")) return;
    }
    long status = 0;
    std::string answer, error;
    const std::string body = ping::body(id, kVersion, update::channelToken(channel), first);
    if (!postPath("/api/graphics/ping", body, status, answer, error) || status < 200 || status >= 300 ||
        !makeFolders(join(dir, wide(layout::kCache))))
        return;
    writeFileAtomic(sentPath, std::to_string(now) + "\n");
    if (!first.empty()) writeFileAtomic(firstSent, "1\n");
}

namespace {

std::string branchText(const std::string& kept) {
    const size_t local = kept.find("[local]");
    return local == std::string::npos ? kept : kept.substr(0, local);
}

// The archive at `path` is the one update.ini names: its size and SHA-256.
bool named(const std::wstring& path, const update::Archive& a) {
    std::string bytes;
    return readFile(path, bytes) && bytes.size() == a.size && sha256::hexOf(bytes) == a.sha256;
}

// `a` from its commit into uaro-graphics\update\<update::localName>, unless it is there already; checked.
bool fetchArchive(const std::wstring& dir, const update::Archive& a, Step::What what, const Report& report,
                  std::wstring& path, std::wstring& error) {
    path = join(workOf(dir), wide(update::localName(a)));
    if (named(path, a)) return true;
    std::string e;
    const bool got = makeFolders(workOf(dir)) && downloadUrl(update::archiveUrl(a), path, [&](uint64_t done,
                                                                                               uint64_t total) {
        Step s;
        s.what = what;
        s.done = done;
        s.total = total ? total : a.size;
        return report(s);
    }, e);
    if (!got) {
        error = wide(e.empty() ? "cannot write " + utf8(workOf(dir)) : e);
        return false;
    }
    if (!named(path, a)) {
        deleteFile(path);
        error = L"the downloaded " + wide(a.file) +
                L" is not the one update.ini names (its size or SHA-256 differs): try again";
        return false;
    }
    return true;
}

// An archive's files into `to` (their tree as in it).
bool unpackBytes(std::string bytes, const std::wstring& to, std::wstring& error) {
    std::string e;
    zip::Index z;
    if (!z.openBytes(std::move(bytes), [](const std::string&) { return true; }, e)) {
        error = L"the package cannot be read: " + wide(e);
        return false;
    }
    removeTree(to);
    for (const std::string& name : z.names()) {
        if (name.empty() || name.back() == '\\') continue;
        std::vector<uint8_t> data;
        const std::wstring path = join(to, wide(name));
        if (!z.read(name, data, e) || !makeFolders(path.substr(0, path.find_last_of(L'\\'))) ||
            !writeFileAtomic(path, std::string(data.begin(), data.end()))) {
            error = L"cannot unpack " + wide(name) + (e.empty() ? L"" : L": " + wide(e));
            return false;
        }
    }
    return true;
}

bool unpack(const std::wstring& zipPath, const std::wstring& to, std::wstring& error) {
    std::string bytes;
    if (!readFile(zipPath, bytes)) {
        error = L"cannot read " + zipPath;
        return false;
    }
    return unpackBytes(std::move(bytes), to, error);
}

// A part of the branch (DLSS, the models) downloaded, checked, unpacked and put in, all or nothing.
bool runPart(const std::wstring& dir, const update::Archive& a, Step::What what, const wchar_t* mustHave,
             const Report& report, std::wstring& error) {
    if (a.file.empty()) {
        error = L"the branch has none";
        return false;
    }
    std::wstring zipPath;
    if (!fetchArchive(dir, a, what, report, zipPath, error)) return false;
    const std::wstring files = join(workOf(dir), L"part");
    if (!unpack(zipPath, files, error)) return false;
    if (!fileExists(join(files, mustHave))) {
        error = wide(a.file) + L" has no " + mustHave;
        removeTree(files);
        return false;
    }
    const installer::Result put = installer::putFiles(dir, files);
    removeTree(files);
    deleteFile(zipPath);
    if (!put.ok) error = put.error;
    return put.ok;
}

}  // namespace

std::string installedVersion(const std::wstring& dir) {
    const std::string v = update::bare(firstLine(join(dir, L"uaro-graphics\\version.txt")));
    return v.empty() ? std::string(kVersion) : v;
}

std::string skipped(const std::wstring& dir) {
    std::string kept;
    readFile(infoOf(dir), kept);
    const std::map<std::string, std::string> ini = settings::readIni(kept);
    const auto it = ini.find("local.skip");
    return it == ini.end() ? "" : it->second;
}

void setSkipped(const std::wstring& dir, const std::string& version) {
    std::string kept;
    readFile(infoOf(dir), kept);
    keepInfo(dir, branchText(kept), version, update::keptChannel(kept, installedVersion(dir)));
}

void forgetBranch(const std::wstring& dir) { keepInfo(dir, "", skipped(dir), channelOf(dir)); }

update::Channel channelOf(const std::wstring& dir) {
    std::string ini;
    readFile(ourFile(dir, layout::File::Ini), ini);
    const std::map<std::string, std::string> keys = settings::readIni(ini);
    const auto it = keys.find("update.channel");
    return update::channelOf(it == keys.end() ? "" : it->second, installedVersion(dir));
}

CheckResult checkNow(const std::wstring& dir) {
    CheckResult c;
    c.channel = channelOf(dir);
    pingInstall(dir, c.channel);  // 3.0.11-beta.56: the site's count of installs, before GitHub's answer
    std::string text;
    if (!fetchUrl(update::manifestUrl(c.channel), text, c.error) || !update::parseManifest(text, c.branch, c.error))
        return c;
    const std::string skip = skipped(dir);
    keepInfo(dir, text, skip, c.channel);
    c.offer = update::offer(c.branch, installedVersion(dir), skip, c.channel);
    c.back = update::backToRelease(c.channel, c.offer, installedVersion(dir));
    c.ok = true;
    return c;
}

bool nvidiaHere() {
    for (const win::AdapterProbe& a : probeSystem().adapters)
        if (a.vendor == 0x10de) return true;
    return false;
}

namespace {

// The package unpacked in uaro-graphics\update\files put in: the game waited for, the files (all or nothing), what
// install.bat did; then DLSS on an NVIDIA card when the branch's is another version (`branch` null: asked now; a
// failure there leaves DLSS as it was). The work folder goes at the end.
bool installStaged(const std::wstring& dir, bool repair, const update::Manifest* branch, const Report& report,
                   std::wstring& error) {
    const std::wstring work = workOf(dir), files = join(work, L"files");
    if (!fileExists(join(files, L"uaro-graphics\\ddraw.dll")) ||
        !fileExists(join(files, L"uaro-graphics\\version.txt"))) {
        error = L"this is not a uaRO Graphics package";
        removeTree(files);
        return false;
    }
    const std::wstring game = join(dir, L"uaRO.exe");
    while (gameRunning(game)) {
        Step s;
        s.what = Step::Wait;
        if (!report(s)) {
            error = L"stopped";
            return false;
        }
        waitMs(500);
    }
    Step install;
    install.what = Step::Install;
    if (!report(install)) {
        removeTree(files);
        error = L"stopped";
        return false;
    }
    const installer::Result put = installer::putPackage(dir, files, repair);
    if (!put.ok) {
        removeTree(files);
        error = put.error;
        return false;
    }
    installer::finishInstall(dir, true);
    // DLSS on an NVIDIA card; 2.15: the AI models (they left the package) when the AI textures are on - each when the
    // branch's version is not the one here (a failure leaves it as it was)
    std::string ini;
    readFile(ourFile(dir, layout::File::Ini), ini);
    const bool nvidia = nvidiaHere(), ai = parseConfig(ini).ai.on();
    if (nvidia || ai) {
        CheckResult asked;
        if (!branch) asked = checkNow(dir);
        const update::Manifest* m = branch ? branch : asked.ok ? &asked.branch : nullptr;
        const std::string dlss = firstLine(join(dir, L"uaro-graphics\\dlss\\version.txt"));
        const std::string models = firstLine(join(dir, L"uaro-graphics\\ai\\models-version.txt"));
        std::wstring partError;
        if (nvidia && m && !m->dlss.file.empty() && dlss != m->dlss.version) runDlss(dir, *m, report, partError);
        if (ai && m && !m->models.file.empty() && models != m->models.version)
            runModels(dir, *m, report, partError);
    }
    removeTree(work);
    Step done;
    done.what = Step::Done;
    report(done);
    return true;
}

}  // namespace

bool runUpdate(const std::wstring& dir, const update::Manifest& m, bool repair, const Report& report,
               std::wstring& error) {
    removeTree(join(workOf(dir), L"rolled"));  // a rollback's leftovers (the uaro-graphics.exe that ran it)
    std::wstring zipPath;
    if (!fetchArchive(dir, m.package, Step::Download, report, zipPath, error) ||
        !unpack(zipPath, join(workOf(dir), L"files"), error))
        return false;
    return installStaged(dir, repair, &m, report, error);
}

bool runInstall(const std::wstring& dir, std::string zip, const Report& report, std::wstring& error) {
    if (!makeFolders(workOf(dir)) || !unpackBytes(std::move(zip), join(workOf(dir), L"files"), error)) {
        removeTree(workOf(dir));
        if (error.empty()) error = L"cannot write " + workOf(dir);
        return false;
    }
    return installStaged(dir, false, nullptr, report, error);
}

bool runDlss(const std::wstring& dir, const update::Manifest& m, const Report& report, std::wstring& error) {
    return runPart(dir, m.dlss, Step::Dlss, L"uaro-graphics\\dlss\\sl.interposer.dll", report, error);
}

bool runModels(const std::wstring& dir, const update::Manifest& m, const Report& report, std::wstring& error) {
    return runPart(dir, m.models, Step::Models, L"uaro-graphics\\ai\\models-version.txt", report, error);
}

}  // namespace uagfx::app
