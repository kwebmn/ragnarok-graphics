#include "app/installer.hpp"

#include <set>
#include <utility>

#include "app/app_disk.hpp"
#include "app/files.hpp"
#include "app/registry.hpp"
#include "app/shortcuts.hpp"
#include "app/text.hpp"
#include "core/ini_upgrade.hpp"
#include "core/layout.hpp"
#include "core/layout_migration.hpp"
#include "core/settings/pad_files.hpp"
#include "core/update_check.hpp"
#include "core/version.hpp"

namespace uagfx::app::installer {
namespace {

std::wstring join(const std::wstring& a, const std::wstring& b) { return a + L"\\" + b; }

std::wstring parentOf(const std::wstring& path) {
    const size_t slash = path.find_last_of(L'\\');
    return slash == std::wstring::npos ? L"" : path.substr(0, slash);
}

// A folder and the ones above it that are missing.
bool makeFolders(const std::wstring& path) {
    if (path.empty() || dirExists(path)) return true;
    const std::wstring up = parentOf(path);
    return (up.empty() || makeFolders(up)) && makeFolder(path);
}

// Every file under `folder`, relative to it ("a\\b.txt"), folders' files after the folder's own.
void filesUnder(const std::wstring& folder, const std::wstring& prefix, std::vector<std::wstring>& out) {
    for (const std::wstring& f : listFolder(folder, false)) out.push_back(prefix + f);
    for (const std::wstring& d : listFolder(folder, true)) filesUnder(join(folder, d), prefix + d + L"\\", out);
}

std::vector<std::wstring> filesUnder(const std::wstring& folder) {
    std::vector<std::wstring> out;
    if (dirExists(folder)) filesUnder(folder, L"", out);
    return out;
}

bool ours(const std::string& bytes) { return bytes.find("uaRO Graphics") != std::string::npos; }

bool samePath(std::wstring a, std::wstring b) {
    for (std::wstring* s : {&a, &b})
        for (wchar_t& c : *s) c = c >= L'A' && c <= L'Z' ? wchar_t(c - L'A' + L'a') : c == L'/' ? L'\\' : c;
    return a == b;
}

std::wstring nameOf(const std::wstring& path) { return path.substr(path.find_last_of(L'\\') + 1); }

// 2.11: once - the desktop's shortcuts of the game go aside into uaro-graphics\shortcuts and «uaRO» (the launcher,
// the game's icon) takes their place; a player who deleted «uaRO» does not get it back from an update.
void placeShortcut(const std::wstring& dir, Result& r) {
    const std::wstring aside = join(dir, wide(layout::kShortcuts)), marker = join(aside, L"made.txt");
    const std::wstring desk = desktopFolder();
    if (fileExists(marker) || desk.empty() || !makeFolders(aside)) return;
    for (const Shortcut& s : desktopShortcuts())
        if (samePath(s.target, join(dir, L"uaRO.exe")) && moveFile(s.path, join(aside, nameOf(s.path)), true))
            r.log.push_back("installer: the game's shortcut " + utf8(nameOf(s.path)) + " aside");
    if (makeShortcut(join(desk, L"uaRO.lnk"), join(dir, L"uaro-graphics.exe"), dir, join(dir, L"uaRO.exe")))
        r.log.push_back("installer: the shortcut uaRO on the desktop");
    writeFileAtomic(marker, "uaRO.lnk\r\n");
}

// The removal's: the game's shortcuts back where they were, «uaRO» away.
void restoreShortcuts(const std::wstring& dir) {
    const std::wstring aside = join(dir, wide(layout::kShortcuts)), desk = desktopFolder();
    if (desk.empty()) return;
    for (const Shortcut& s : desktopShortcuts())
        if (samePath(s.target, join(dir, L"uaro-graphics.exe"))) deleteFile(s.path);
    for (const std::wstring& name : listFolder(aside, false))
        if (name.size() > 4 && name.compare(name.size() - 4, 4, L".lnk") == 0 && !fileExists(join(desk, name)))
            moveFile(join(aside, name), join(desk, name), false);
}

// The moves done, undone in the reverse order when a later one fails.
struct Journal {
    std::vector<std::pair<std::wstring, std::wstring>> done;
    bool move(const std::wstring& from, const std::wstring& to) {
        if (!makeFolders(parentOf(to)) || !moveFile(from, to, false)) return false;
        done.emplace_back(from, to);
        return true;
    }
    void undo() {
        for (auto it = done.rbegin(); it != done.rend(); ++it) moveFile(it->second, it->first, true);
        done.clear();
    }
};

Result failed(Journal& j, const std::wstring& what) {
    j.undo();
    Result r;
    r.ok = false;
    r.error = what;
    return r;
}

}  // namespace

Result putPackage(const std::wstring& dir, const std::wstring& staged, bool repair) {
    const std::wstring work = join(dir, wide(layout::kUpdateWork));
    const std::wstring previousNew = join(work, L"previous"), replaced = join(work, L"replaced");
    const std::wstring root = join(work, L"root");
    Journal j;
    if (!fileExists(join(staged, L"uaro-graphics\\ddraw.dll")))
        return failed(j, L"the package has no uaro-graphics\\ddraw.dll");
    for (const std::wstring& left : {previousNew, replaced, root}) removeTree(left);
    // The root's copies of ddraw.dll and uaro-graphics.exe, from the package's uaro-graphics\.
    std::vector<std::pair<std::wstring, std::wstring>> files;  // the staged file, where it goes (relative)
    for (const std::wstring& f : filesUnder(join(staged, L"uaro-graphics")))
        files.emplace_back(join(join(staged, L"uaro-graphics"), f), L"uaro-graphics\\" + f);
    for (const wchar_t* name : {L"ddraw.dll", L"uaro-graphics.exe"}) {
        std::string bytes;
        if (!readFile(join(join(staged, L"uaro-graphics"), name), bytes)) continue;
        if (!makeFolders(root) || !writeFileAtomic(join(root, name), bytes))
            return failed(j, L"cannot write " + join(root, name));
        files.emplace_back(join(root, name), name);
    }
    int put = 0, kept = 0;
    for (const auto& [from, relative] : files) {
        const std::wstring to = join(dir, relative);
        std::string now, next;
        if (readFile(to, now)) {
            if (readFile(from, next) && next == now) continue;
            std::wstring aside = join(repair ? replaced : previousNew, relative);
            if (relative == L"ddraw.dll" && !ours(now)) {  // dgVoodoo's, kept for the removal
                aside = join(dir, wide(layout::now(layout::File::DgVoodoo))) + L"\\DDraw.dll";
                if (fileExists(aside) && !j.move(aside, join(replaced, L"dgvoodoo-DDraw.dll")))
                    return failed(j, L"cannot replace " + aside);
            } else if (!repair) {
                ++kept;
            }
            if (!j.move(to, aside)) return failed(j, L"cannot replace " + to);
        }
        if (!j.move(from, to)) return failed(j, L"cannot put " + to);
        ++put;
    }
    Result r;
    if (!filesUnder(previousNew).empty()) {
        const std::wstring previous = join(dir, wide(layout::kPrevious));
        removeTree(previous);
        if (!moveFile(previousNew, previous, false)) r.log.push_back("installer: the rollback point was not kept");
    }
    removeTree(replaced);
    r.log.push_back("installer: " + std::to_string(put) + " file(s) put, " + std::to_string(kept) +
                    " kept in uaro-graphics\\previous");
    return r;
}

Result putFiles(const std::wstring& dir, const std::wstring& staged) {
    const std::wstring replaced = join(join(dir, wide(layout::kUpdateWork)), L"replaced");
    Journal j;
    removeTree(replaced);
    int put = 0;
    for (const std::wstring& relative : filesUnder(staged)) {
        const std::wstring from = join(staged, relative), to = join(dir, relative);
        std::string now, next;
        if (readFile(to, now) && readFile(from, next) && now == next) continue;
        if (fileExists(to) && !j.move(to, join(replaced, relative))) return failed(j, L"cannot replace " + to);
        if (!j.move(from, to)) return failed(j, L"cannot put " + to);
        ++put;
    }
    removeTree(replaced);
    Result r;
    r.log.push_back("installer: " + std::to_string(put) + " file(s) put");
    return r;
}

bool canRollBack(const std::wstring& dir) { return !filesUnder(join(dir, wide(layout::kPrevious))).empty(); }

std::string previousVersion(const std::wstring& dir) {
    std::string text;
    if (!readFile(join(dir, wide(layout::kPrevious)) + L"\\uaro-graphics\\version.txt", text)) return "";
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n')) text.pop_back();
    return update::bare(text);
}

Result rollBack(const std::wstring& dir) {
    const std::wstring previous = join(dir, wide(layout::kPrevious));
    const std::wstring rolled = join(join(dir, wide(layout::kUpdateWork)), L"rolled");
    Journal j;
    const std::vector<std::wstring> files = filesUnder(previous);
    if (files.empty()) return failed(j, L"there is no previous version");
    removeTree(rolled);
    for (const std::wstring& relative : files) {
        const std::wstring to = join(dir, relative);
        if (fileExists(to) && !j.move(to, join(rolled, relative))) return failed(j, L"cannot replace " + to);
        if (!j.move(join(previous, relative), to)) return failed(j, L"cannot put back " + to);
    }
    removeTree(previous);
    removeTree(rolled);  // the running uaro-graphics.exe among them stays until the next update's start
    Result r;
    r.log.push_back("installer: " + std::to_string(files.size()) + " file(s) of the previous version back");
    return r;
}

Result finishInstall(const std::wstring& dir, bool scripts) {
    Result r;
    // A 2.x root settings file: what older versions lack goes in before it moves (a 2.8 file is never upgraded).
    const std::wstring rootIni = join(dir, wide(layout::before(layout::File::Ini)));
    std::string text;
    if (readFile(rootIni, text)) {
        const std::string upgraded = upgradeIni(text);
        if (upgraded != text && writeFileAtomic(rootIni, upgraded))
            r.log.push_back("installer: the 2.x settings file got the keys it lacked");
    }
    AppDisk disk;
    for (const std::string& line : layout::migrate(utf8(dir), disk)) r.log.push_back(line);
    for (const char* folder : layout::kMadeAtStart) makeFolders(ourPlace(dir, folder));
    const std::wstring dg = join(dir, wide(layout::now(layout::File::DgVoodoo)));
    if (fileExists(join(dir, L"D3DImm.dll")) && makeFolders(dg) &&
        moveFile(join(dir, L"D3DImm.dll"), join(dg, L"D3DImm.dll"), true))
        r.log.push_back("installer: dgVoodoo's D3DImm.dll into uaro-graphics\\dgvoodoo");
    std::string conf;
    if (readFile(join(dir, L"dgVoodoo.conf"), conf) && makeFolders(dg))
        writeFileAtomic(join(dg, L"dgVoodoo.conf"), conf);  // install.bat copied it: the player's one stays
    const std::wstring ini = join(dir, wide(layout::now(layout::File::Ini)));
    std::string templ;
    if (!fileExists(ourFile(dir, layout::File::Ini)) && readFile(join(dir, wide(layout::kIniTemplate)), templ) &&
        writeFileAtomic(ini, templ))
        r.log.push_back("installer: the settings file from the package's template");
    removeOldPad(dir);
    removeTree(join(dir, L"uaro-graphics\\pad"));
    if (scripts) removeOldScripts(dir);
    placeShortcut(dir, r);
    std::string version;
    readFile(join(dir, L"uaro-graphics\\version.txt"), version);
    while (!version.empty() && (version.back() == '\r' || version.back() == '\n')) version.pop_back();
    if (!writeAppsEntry(dir, version.empty() ? std::string(kVersion) : update::bare(version)))
        r.log.push_back("installer: Windows refused the entry in Apps");
    return r;
}

void removeOldPad(const std::wstring& dir) {
    std::string installed;
    if (!readFile(dir + L"\\dinput.dll", installed) ||
        settings::classifyPad(installed, "") != settings::PadFile::Pad)
        return;
    if (!deleteFile(dir + L"\\dinput.dll")) return;
    std::string moved;
    readFile(dir + L"\\uaro-pad.moved", moved);
    while (!moved.empty() && (moved.back() == '\r' || moved.back() == '\n' || moved.back() == ' ')) moved.pop_back();
    if (moved == "dinput8.dll" || moved == "dinput.dll.before-pad") {
        const std::wstring from = dir + L"\\" + wide(moved);
        std::string bytes;
        if (readFile(from, bytes) && writeFileAtomic(dir + L"\\dinput.dll", bytes)) deleteFile(from);
    }
    deleteFile(dir + L"\\uaro-pad.moved");
}

void removeOldScripts(const std::wstring& dir) {
    for (const wchar_t* name : {L"install.bat", L"update.bat", L"uninstall.bat", L"get-dlss.bat", L"get-models.bat",
                                L"use-dgvoodoo.bat", L"use-uaro.bat"}) {
        std::string bytes;
        if (readFile(join(dir, name), bytes) && ours(bytes)) deleteFile(join(dir, name));
    }
    for (const wchar_t* left :
         {L"uaro-graphics-update", L"uaro-graphics-dlss-download", L"uaro-graphics-models-download"})
        removeTree(join(dir, left));
}

Result uninstall(const std::wstring& dir, bool keep) {
    Result r;
    restoreShortcuts(dir);  // 2.11: before uaro-graphics\ goes
    std::string bytes;
    if (readFile(join(dir, L"ddraw.dll"), bytes) && ours(bytes) && !deleteFile(join(dir, L"ddraw.dll"))) {
        r.ok = false;
        r.error = L"cannot remove " + join(dir, L"ddraw.dll");
        return r;
    }
    for (const char* place : {layout::now(layout::File::DgVoodoo), layout::before(layout::File::DgVoodoo)}) {
        const std::wstring dg = join(dir, wide(place));
        for (const wchar_t* name : {L"DDraw.dll", L"D3DImm.dll"})
            if (fileExists(join(dg, name)) && !fileExists(join(dir, name)) &&
                moveFile(join(dg, name), join(dir, name), false))
                r.log.push_back("installer: dgVoodoo's " + utf8(name) + " back");
        deleteFile(join(dg, L"dgVoodoo.conf"));
        removeEmptyFolder(dg);
    }
    if (fileExists(join(dir, L"uaro-graphics.exe")) && !deleteFile(join(dir, L"uaro-graphics.exe")))
        r.log.push_back("installer: self");
    const std::wstring folder = join(dir, wide(layout::kOurs));
    if (!keep) {
        removeTree(folder);
    } else {
        const auto name = [](const char* place) { return wide(place).substr(std::string(layout::kOurs).size() + 1); };
        const std::set<std::wstring> kept = {
            name(layout::now(layout::File::Ini)), name(layout::now(layout::File::PadIni)),
            name(layout::now(layout::File::PadBinds)), name(layout::now(layout::File::Backups)),
            name(layout::kTextures)};
        for (const std::wstring& f : listFolder(folder, false))
            if (!kept.count(f)) deleteFile(join(folder, f));
        for (const std::wstring& d : listFolder(folder, true))
            if (!kept.count(d)) removeTree(join(folder, d));
    }
    if (!keep && dirExists(folder)) r.log.push_back("installer: uaro-graphics\\ could not go whole");
    removeOldScripts(dir);
    removeAppsEntry();
    return r;
}

}  // namespace uagfx::app::installer
