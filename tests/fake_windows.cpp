#include "fake_windows.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>

#include "app/download.hpp"
#include "app/files.hpp"
#include "app/text.hpp"

namespace fake {
namespace {

const std::wstring kDir = L"C:\\uaRO";

std::wstring parentOf(const std::wstring& p) {
    const size_t slash = p.find_last_of(L'\\');
    return slash == std::wstring::npos ? L"" : p.substr(0, slash);
}

bool under(const std::wstring& p, const std::wstring& folder) {
    return p.size() > folder.size() && p.compare(0, folder.size(), folder) == 0 && p[folder.size()] == L'\\';
}

}  // namespace

Windows& windows() {
    static Windows w;
    return w;
}

Windows& reset() {
    Windows& w = windows();
    w = Windows{};
    w.folders = {L"C:", kDir};
    uagfx::settings::MonitorInfo m;
    m.primary = true;
    m.pixels = {3840, 2160};
    m.work = {3840, 2076};
    m.scale = 175;
    m.refresh = 60;
    w.system.monitors = {m};
    w.system.windows = {10, 0, 19045};
    w.system.d3d12Probed = true;
    w.system.d3d12.ok = true;
    return w;
}

std::wstring path(const std::wstring& relative) { return kDir + L"\\" + relative; }

void gameKey(uagfx::settings::RegCopy& copy, int width, int height, bool fullscreen) {
    using uagfx::settings::RegValue;
    copy.exists = true;
    copy.keys[""]["ISFULLSCREENMODE"] = RegValue::dword(fullscreen ? 1 : 0);
    copy.keys[""]["WIDTH"] = RegValue::dword(uint32_t(width));
    copy.keys[""]["HEIGHT"] = RegValue::dword(uint32_t(height));
    copy.keys[""]["BITPERPIXEL"] = RegValue::dword(32);
}

}  // namespace fake

namespace uagfx::app {

using fake::windows;

// files.hpp

std::wstring exeFolder() { return fake::kDir; }

bool readFile(const std::wstring& path, std::string& out) {
    const auto it = windows().files.find(path);
    if (it == windows().files.end()) return false;
    out = it->second;
    return true;
}

bool writeFileAtomic(const std::wstring& path, const std::string& bytes) {
    if (!windows().folders.count(fake::parentOf(path))) return false;  // as Windows: not into a missing folder
    windows().files[path] = bytes;
    return true;
}

bool fileExists(const std::wstring& path) { return windows().files.count(path) > 0; }

bool dirExists(const std::wstring& path) { return windows().folders.count(path) > 0; }

std::vector<std::wstring> listFolder(const std::wstring& dir, bool folders) {
    std::vector<std::wstring> out;
    auto add = [&](const std::wstring& p) {
        if (fake::parentOf(p) == dir) out.push_back(p.substr(dir.size() + 1));
    };
    if (folders)
        for (const std::wstring& f : windows().folders) add(f);
    else
        for (const auto& f : windows().files) add(f.first);
    std::sort(out.begin(), out.end());
    return out;
}

bool makeFolder(const std::wstring& path) {
    if (!windows().folders.count(fake::parentOf(path))) return false;
    windows().folders.insert(path);
    return true;
}

bool removeTree(const std::wstring& path) {
    fake::Windows& w = windows();
    for (auto it = w.files.begin(); it != w.files.end();) it = fake::under(it->first, path) ? w.files.erase(it) : ++it;
    for (auto it = w.folders.begin(); it != w.folders.end();)
        it = fake::under(*it, path) ? w.folders.erase(it) : ++it;
    return w.folders.erase(path) > 0;
}

bool deleteFile(const std::wstring& path) { return windows().files.erase(path) > 0; }

bool moveFile(const std::wstring& from, const std::wstring& to, bool replace) {
    fake::Windows& w = windows();
    if (w.locked.count(from) || !w.folders.count(fake::parentOf(to)) || w.folders.count(to)) return false;
    if (w.files.count(from)) {
        if (w.files.count(to) && !replace) return false;
        w.files[to] = w.files[from];
        w.files.erase(from);
        return true;
    }
    if (!w.folders.count(from) || w.files.count(to)) return false;
    std::map<std::wstring, std::string> files;  // a folder moves whole
    for (const auto& f : w.files)
        files[fake::under(f.first, from) ? to + f.first.substr(from.size()) : f.first] = f.second;
    std::set<std::wstring> folders;
    for (const std::wstring& d : w.folders)
        folders.insert(d == from || fake::under(d, from) ? to + d.substr(from.size()) : d);
    w.files.swap(files);
    w.folders.swap(folders);
    return true;
}

bool removeEmptyFolder(const std::wstring& path) {
    fake::Windows& w = windows();
    for (const auto& f : w.files)
        if (fake::parentOf(f.first) == path) return false;
    for (const std::wstring& d : w.folders)
        if (fake::parentOf(d) == path) return false;
    return w.folders.erase(path) > 0;
}

bool folderWritable(const std::wstring&) { return true; }

bool underProgramFiles(const std::wstring&) { return false; }

std::wstring timestampName() {
    const int s = 12 * 3600 + windows().clock++;
    char buf[32];
    std::snprintf(buf, sizeof buf, "2026-10-13-%02d%02d%02d", s / 3600, s / 60 % 60, s % 60);
    return wide(buf);
}

std::string regFileToUtf8(const std::string& bytes) { return bytes; }  // the fake keeps .reg files in UTF-8

std::string utf8ToRegFile(const std::string& utf8) { return utf8; }

unsigned long launch(const std::wstring& file, const std::wstring&) {
    windows().launched.push_back(file);
    return 0;
}

unsigned long launchArgs(const std::wstring&, const std::wstring&, const std::wstring&, bool) { return 0; }

void openInShell(const std::wstring&) {}

std::string defaultIni() {
    std::ifstream f(UAGFX_SOURCE_DIR "/dist/uaro-graphics.ini", std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

// registry.hpp

GameKeyPaths gameKeyPaths() {
    GameKeyPaths p;
    p.personal = L"Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\WOW6432Node\\Gravity Soft\\Ragnarok";
    p.personalRoot = "HKEY_CURRENT_USER\\" + utf8(p.personal);
    p.commonRoot = "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Gravity Soft\\Ragnarok";
    return p;
}

settings::RegCopy readPersonal() { return windows().registry.personal; }

settings::RegCopy readCommon() { return windows().registry.common; }

std::string writePersonalValue(const settings::RegWrite& w) {
    if (windows().refused.count(w.name)) return w.name + " (error 5)";
    settings::applyWrites(windows().registry.personal, {w});
    return "";
}

std::string replacePersonal(const settings::RegCopy& copy) {
    windows().registry.personal = copy;
    return "";
}

std::vector<CompatEntry> readCompat(const std::wstring&) { return windows().compat; }

bool writeUserCompat(const std::wstring&, const std::string& flags) {
    std::vector<CompatEntry>& c = windows().compat;
    c.erase(std::remove_if(c.begin(), c.end(), [](const CompatEntry& e) { return !e.allUsers; }), c.end());
    if (!flags.empty()) c.push_back(CompatEntry{false, flags});
    return true;
}

// system.hpp

bool writeAppsEntry(const std::wstring& dir, const std::string& version) {
    std::map<std::wstring, std::wstring>& e = windows().appsEntry;
    e[L"DisplayName"] = L"uaRO Graphics";
    e[L"DisplayVersion"] = wide(version);
    e[L"InstallLocation"] = dir;
    e[L"UninstallString"] = L"\"" + dir + L"\\uaro-graphics.exe\" --uninstall";
    return true;
}

bool removeAppsEntry() {
    windows().appsEntry.clear();
    return true;
}

SystemInfo probeSystem() { return windows().system; }

bool gameRunning(const std::wstring&) { return windows().gameRunning; }

bool elevatedNow() { return false; }

void markOurStart() { windows().marked = true; }

void waitMs(unsigned) {  // the game closes while the update waits
    ++windows().waits;
    windows().gameRunning = false;
}

bool download(const std::string& file, const std::wstring& to,
              const std::function<bool(uint64_t done, uint64_t total)>& progress, std::string& error) {
    const auto it = windows().web.find(file);
    if (it == windows().web.end()) {
        error = "uaro.kiev.ua answered 404 for " + file;
        return false;
    }
    const uint64_t size = it->second.size();
    if (!progress(size / 2, size) || !progress(size, size)) {
        error = "stopped";
        return false;
    }
    if (!writeFileAtomic(to, it->second)) {
        error = "cannot write " + utf8(to);
        return false;
    }
    return true;
}

bool fetchPath(const std::string& path, std::string& out, std::string& error) {
    const auto it = windows().web.find(path);
    if (it == windows().web.end()) {
        error = "no answer from uaro.kiev.ua";
        return false;
    }
    out = it->second;
    return true;
}

bool fetch(const std::string& file, std::string& out, std::string& error) {
    const auto it = windows().web.find(file);
    if (it == windows().web.end()) {
        error = "uaro.kiev.ua answered 404 for " + file;
        return false;
    }
    out = it->second;
    return true;
}

// text.hpp: wchar_t holds a whole code point on Linux.

std::wstring wide(const std::string& s) {
    std::wstring out;
    for (size_t i = 0; i < s.size();) {
        const auto c = static_cast<unsigned char>(s[i]);
        const size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
        if (n == 0 || i + n > s.size()) {
            out += wchar_t(0xFFFD);
            ++i;
            continue;
        }
        uint32_t cp = n == 1 ? c : c & (0x7Fu >> n);
        for (size_t k = 1; k < n; ++k) cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3Fu);
        out += wchar_t(cp);
        i += n;
    }
    return out;
}

std::string utf8(const std::wstring& w) {
    std::string out;
    for (wchar_t ch : w) {
        const auto cp = uint32_t(ch);
        if (cp < 0x80) {
            out += char(cp);
        } else if (cp < 0x800) {
            out += char(0xC0 | (cp >> 6));
            out += char(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += char(0xE0 | (cp >> 12));
            out += char(0x80 | ((cp >> 6) & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        } else {
            out += char(0xF0 | (cp >> 18));
            out += char(0x80 | ((cp >> 12) & 0x3F));
            out += char(0x80 | ((cp >> 6) & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

}  // namespace uagfx::app
