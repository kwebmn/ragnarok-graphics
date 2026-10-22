#include "app/update_run.hpp"

#include <map>

#include "app/download.hpp"
#include "app/files.hpp"
#include "app/installer.hpp"
#include "app/system.hpp"
#include "app/text.hpp"
#include "core/layout.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/sha256.hpp"
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

// uaro-graphics\update.ini: the site's text, then "[local]" and this computer's choices.
void keepInfo(const std::wstring& dir, std::string site, const std::string& skip) {
    if (!site.empty() && site.back() != '\n') site += "\r\n";
    if (!skip.empty()) site += "[local]\r\nskip = " + skip + "\r\n";
    writeFileAtomic(infoOf(dir), site);
}

std::string siteText(const std::string& kept) {
    const size_t local = kept.find("[local]");
    return local == std::string::npos ? kept : kept.substr(0, local);
}

// The archive at `path` is the site's: its size and SHA-256.
bool sameAsSite(const std::wstring& path, const update::Archive& a) {
    std::string bytes;
    return readFile(path, bytes) && bytes.size() == a.size && sha256::hexOf(bytes) == a.sha256;
}

// `a` of the site into uaro-graphics\update\<its file>, unless it is there already; checked.
bool fetchArchive(const std::wstring& dir, const update::Archive& a, Step::What what, const Report& report,
                  std::wstring& path, std::wstring& error) {
    path = join(workOf(dir), wide(a.file));
    if (sameAsSite(path, a)) return true;
    std::string e;
    const bool got = makeFolders(workOf(dir)) && download(a.file, path, [&](uint64_t done, uint64_t total) {
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
    if (!sameAsSite(path, a)) {
        deleteFile(path);
        error = L"the downloaded " + wide(a.file) + L" is not the site's (its size or SHA-256 differs): try again";
        return false;
    }
    return true;
}

// The archive's files into `to` (their tree as in it).
bool unpack(const std::wstring& zipPath, const std::wstring& to, std::wstring& error) {
    std::string bytes, e;
    zip::Index z;
    if (!readFile(zipPath, bytes) || !z.openBytes(std::move(bytes), [](const std::string&) { return true; }, e)) {
        error = L"cannot read " + zipPath + (e.empty() ? L"" : L": " + wide(e));
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

// A part of the site (DLSS, the models) downloaded, checked, unpacked and put in, all or nothing.
bool runPart(const std::wstring& dir, const update::Archive& a, Step::What what, const wchar_t* mustHave,
             const Report& report, std::wstring& error) {
    if (a.file.empty()) {
        error = L"the site has none";
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
    keepInfo(dir, siteText(kept), version);
}

CheckResult checkNow(const std::wstring& dir) {
    CheckResult c;
    std::string text;
    if (!fetch(update::kManifest, text, c.error) || !update::parseManifest(text, c.site, c.error)) return c;
    const std::string skip = skipped(dir);
    keepInfo(dir, text, skip);
    c.offer = update::offer(c.site, installedVersion(dir), skip);
    c.ok = true;
    return c;
}

bool nvidiaHere() {
    for (const win::AdapterProbe& a : probeSystem().adapters)
        if (a.vendor == 0x10de) return true;
    return false;
}

bool runUpdate(const std::wstring& dir, const update::Manifest& m, bool repair, const Report& report,
               std::wstring& error) {
    const std::wstring work = workOf(dir), files = join(work, L"files");
    removeTree(join(work, L"rolled"));  // a rollback's leftovers (the uaro-graphics.exe that ran it)
    std::wstring zipPath;
    if (!fetchArchive(dir, m.package, Step::Download, report, zipPath, error) || !unpack(zipPath, files, error))
        return false;
    if (!fileExists(join(files, L"uaro-graphics\\ddraw.dll")) ||
        !fileExists(join(files, L"uaro-graphics\\version.txt"))) {
        error = wide(m.package.file) + L" is not a uaRO Graphics package";
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
    std::wstring dlssError;
    const std::string dlss = firstLine(join(dir, L"uaro-graphics\\dlss\\version.txt"));
    if (!m.dlss.file.empty() && nvidiaHere() && dlss != m.dlss.version)
        runDlss(dir, m, report, dlssError);  // DLSS stays as it was when it cannot come now
    removeTree(work);
    Step done;
    done.what = Step::Done;
    report(done);
    return true;
}

bool runDlss(const std::wstring& dir, const update::Manifest& m, const Report& report, std::wstring& error) {
    return runPart(dir, m.dlss, Step::Dlss, L"uaro-graphics\\dlss\\sl.interposer.dll", report, error);
}

bool runModels(const std::wstring& dir, const update::Manifest& m, const Report& report, std::wstring& error) {
    return runPart(dir, m.models, Step::Models, L"uaro-graphics\\ai\\models-version.txt", report, error);
}

}  // namespace uagfx::app
