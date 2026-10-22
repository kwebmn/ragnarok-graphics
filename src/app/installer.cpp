#include "app/installer.hpp"

#include <utility>

#include "app/files.hpp"
#include "app/text.hpp"
#include "core/layout.hpp"
#include "core/update_check.hpp"

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

}  // namespace uagfx::app::installer
