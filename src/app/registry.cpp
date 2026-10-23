#include "app/registry.hpp"

#include <windows.h>

#include "app/text.hpp"
#include "core/settings/compat_flags.hpp"

namespace uagfx::app {
namespace {

using settings::RegCopy;
using settings::RegType;
using settings::RegValue;
using settings::RegWrite;

const wchar_t kLayers[] = L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers";

bool wow64() {
    BOOL w = FALSE;
    return IsWow64Process(GetCurrentProcess(), &w) && w;
}

RegValue toValue(DWORD type, const std::vector<uint8_t>& data) {
    if (type == REG_SZ) {
        std::wstring w(data.size() / 2, L'\0');
        if (!w.empty()) memcpy(&w[0], data.data(), w.size() * 2);
        const size_t nul = w.find(L'\0');
        if (nul != std::wstring::npos) w.resize(nul);  // the game writes buffers with bytes after the NUL
        return RegValue::sz(utf8(w));
    }
    RegValue v;
    v.type = type;
    v.bytes = data;
    return v;
}

void readTree(HKEY key, const std::string& path, RegCopy& out) {
    auto& values = out.keys[path];
    for (DWORD i = 0;; ++i) {
        wchar_t name[16384];
        DWORD nameLen = 16384;
        DWORD type = 0;
        DWORD size = 0;
        LONG r = RegEnumValueW(key, i, name, &nameLen, nullptr, &type, nullptr, &size);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS && r != ERROR_MORE_DATA) break;
        std::vector<uint8_t> data(size);
        nameLen = 16384;
        r = RegEnumValueW(key, i, name, &nameLen, nullptr, &type, data.empty() ? nullptr : data.data(), &size);
        if (r != ERROR_SUCCESS) continue;
        data.resize(size);
        values[utf8(std::wstring(name, nameLen))] = toValue(type, data);
    }
    for (DWORD i = 0;; ++i) {
        wchar_t name[256];
        DWORD nameLen = 256;
        if (RegEnumKeyExW(key, i, name, &nameLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        HKEY sub = nullptr;
        if (RegOpenKeyExW(key, name, 0, KEY_READ, &sub) != ERROR_SUCCESS) continue;
        const std::string child = utf8(std::wstring(name, nameLen));
        readTree(sub, path.empty() ? child : path + "\\" + child, out);
        RegCloseKey(sub);
    }
}

RegCopy readCopy(HKEY root, const std::wstring& path, REGSAM view) {
    RegCopy copy;
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, path.c_str(), 0, KEY_READ | view, &key) != ERROR_SUCCESS) return copy;
    copy.exists = true;
    readTree(key, "", copy);
    RegCloseKey(key);
    return copy;
}

std::wstring keyOf(const std::string& subkey) {
    const std::wstring base = gameKeyPaths().personal;
    return subkey.empty() ? base : base + L"\\" + wide(subkey);
}

LONG setValue(HKEY key, const std::string& name, const RegValue& v) {
    const std::wstring n = wide(name);
    if (v.isSz()) {
        const std::wstring w = wide(v.text);
        return RegSetValueExW(key, n.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(w.c_str()),
                              DWORD((w.size() + 1) * sizeof(wchar_t)));
    }
    return RegSetValueExW(key, n.c_str(), 0, v.type, v.bytes.empty() ? nullptr : v.bytes.data(), DWORD(v.bytes.size()));
}

std::string failure(const std::string& name, LONG r) { return name + " (error " + std::to_string(r) + ")"; }

}  // namespace

GameKeyPaths gameKeyPaths() {
    const bool w = wow64();
    GameKeyPaths p;
    p.personal = std::wstring(L"Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\") +
                 (w ? L"WOW6432Node\\" : L"") + L"Gravity Soft\\Ragnarok";
    p.personalRoot = "HKEY_CURRENT_USER\\" + utf8(p.personal);
    p.commonRoot = std::string("HKEY_LOCAL_MACHINE\\SOFTWARE\\") + (w ? "WOW6432Node\\" : "") + "Gravity Soft\\Ragnarok";
    return p;
}

RegCopy readPersonal() { return readCopy(HKEY_CURRENT_USER, gameKeyPaths().personal, KEY_WOW64_64KEY); }

// The game's own path: a 32-bit process opening HKLM\SOFTWARE gets WOW6432Node by itself.
RegCopy readCommon() { return readCopy(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Gravity Soft\\Ragnarok", KEY_WOW64_32KEY); }

std::string writePersonalValue(const RegWrite& w) {
    HKEY key = nullptr;
    LONG r = w.op == settings::RegOp::Set
                 ? RegCreateKeyExW(HKEY_CURRENT_USER, keyOf(w.subkey).c_str(), 0, nullptr, 0,
                                   KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &key, nullptr)
                 : RegOpenKeyExW(HKEY_CURRENT_USER, keyOf(w.subkey).c_str(), 0, KEY_SET_VALUE | KEY_WOW64_64KEY, &key);
    if (r == ERROR_FILE_NOT_FOUND && w.op == settings::RegOp::Delete) return "";  // nothing to delete
    if (r != ERROR_SUCCESS) return failure(w.name, r);
    r = w.op == settings::RegOp::Set ? setValue(key, w.name, w.value) : RegDeleteValueW(key, wide(w.name).c_str());
    RegCloseKey(key);
    if (r != ERROR_SUCCESS && !(r == ERROR_FILE_NOT_FOUND && w.op == settings::RegOp::Delete))
        return failure(w.name, r);
    return "";
}

std::string replacePersonal(const RegCopy& copy) {
    const std::wstring base = gameKeyPaths().personal;
    HKEY parent = nullptr;
    const size_t slash = base.find_last_of(L'\\');
    if (RegOpenKeyExW(HKEY_CURRENT_USER, base.substr(0, slash).c_str(), 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY,
                      &parent) == ERROR_SUCCESS) {
        const LONG r = RegDeleteTreeW(parent, base.substr(slash + 1).c_str());
        RegCloseKey(parent);
        if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) return failure("Ragnarok", r);
    }
    if (!copy.exists) return "";
    std::string failed;  // each key and value on its own (spec §4.5): the rest is written all the same
    const auto add = [&failed](const std::string& f) { failed += (failed.empty() ? "" : ", ") + f; };
    for (const auto& key : copy.keys) {
        HKEY k = nullptr;  // keys without values are made too
        const LONG r = RegCreateKeyExW(HKEY_CURRENT_USER, keyOf(key.first).c_str(), 0, nullptr, 0,
                                       KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &k, nullptr);
        if (r != ERROR_SUCCESS) {
            add(failure(key.first.empty() ? "Ragnarok" : key.first, r));
            continue;
        }
        RegCloseKey(k);
        for (const auto& v : key.second) {
            const std::string e = writePersonalValue(RegWrite{key.first, v.first, settings::RegOp::Set, v.second});
            if (!e.empty()) add(e);
        }
    }
    return failed;
}

std::vector<CompatEntry> readCompat(const std::wstring& exePath) {
    std::vector<CompatEntry> out;
    for (bool all : {false, true}) {
        HKEY key = nullptr;
        if (RegOpenKeyExW(all ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, kLayers, 0, KEY_READ | KEY_WOW64_64KEY,
                          &key) != ERROR_SUCCESS)
            continue;
        RegCopy values;
        readTree(key, "", values);
        RegCloseKey(key);
        for (const auto& v : values.keys[""])
            if (v.second.isSz() && settings::samePath(v.first, utf8(exePath)))
                out.push_back(CompatEntry{all, v.second.text});
    }
    return out;
}

bool writeUserCompat(const std::wstring& exePath, const std::string& flags) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kLayers, 0, nullptr, 0, KEY_READ | KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr,
                        &key, nullptr) != ERROR_SUCCESS)
        return false;
    // The value's name is the path as Windows wrote it: find it again, whatever its case.
    RegCopy values;
    readTree(key, "", values);
    std::wstring name = exePath;
    for (const auto& v : values.keys[""])
        if (settings::samePath(v.first, utf8(exePath))) name = wide(v.first);
    LONG r;
    if (flags.empty()) {
        r = RegDeleteValueW(key, name.c_str());
        if (r == ERROR_FILE_NOT_FOUND) r = ERROR_SUCCESS;
    } else {
        r = setValue(key, utf8(name), RegValue::sz(flags));
    }
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

namespace {

const wchar_t kAppsParent[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
const wchar_t kAppsPrefix[] = L"uaRO Graphics";

// A text value of a key under the Uninstall key, "" when there is none
std::wstring appsText(HKEY parent, const std::wstring& key, const wchar_t* name) {
    HKEY k = nullptr;
    if (RegOpenKeyExW(parent, key.c_str(), 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &k) != ERROR_SUCCESS) return L"";
    wchar_t text[1024] = {};
    DWORD size = sizeof text - sizeof text[0], type = 0;  // room for the end a value may lack
    const LONG r = RegQueryValueExW(k, name, nullptr, &type, reinterpret_cast<BYTE*>(text), &size);
    RegCloseKey(k);
    return r == ERROR_SUCCESS && type == REG_SZ ? std::wstring(text) : L"";
}

}  // namespace

std::vector<AppsEntry> appsEntries() {
    std::vector<AppsEntry> out;
    HKEY parent = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kAppsParent, 0, KEY_READ | KEY_WOW64_64KEY, &parent) != ERROR_SUCCESS)
        return out;
    const size_t prefix = sizeof kAppsPrefix / sizeof kAppsPrefix[0] - 1;
    for (DWORD i = 0;; ++i) {
        wchar_t name[256];
        DWORD n = 256;
        const LONG r = RegEnumKeyExW(parent, i, name, &n, nullptr, nullptr, nullptr, nullptr);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS || n < prefix) continue;
        if (_wcsicmp(std::wstring(name, prefix).c_str(), kAppsPrefix) != 0) continue;  // not ours
        AppsEntry e;
        e.name.assign(name, n);
        e.dir = appsText(parent, e.name, L"InstallLocation");
        e.version = utf8(appsText(parent, e.name, L"DisplayVersion"));
        out.push_back(e);
    }
    RegCloseKey(parent);
    return out;
}

bool writeAppsEntry(const std::wstring& name, const std::wstring& dir, const std::string& version) {
    HKEY key = nullptr;
    const std::wstring path = std::wstring(kAppsParent) + L"\\" + name;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr,
                        &key, nullptr) != ERROR_SUCCESS)
        return false;
    bool ok = true;
    const auto text = [&](const wchar_t* value, const std::wstring& v) {
        ok = RegSetValueExW(key, value, 0, REG_SZ, reinterpret_cast<const BYTE*>(v.c_str()),
                            DWORD((v.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS && ok;
    };
    const auto number = [&](const wchar_t* value, DWORD v) {
        ok = RegSetValueExW(key, value, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&v), sizeof v) == ERROR_SUCCESS &&
             ok;
    };
    const std::wstring exe = dir + L"\\uaro-graphics.exe";
    text(L"DisplayName", name);
    text(L"DisplayVersion", wide(version));
    text(L"Publisher", L"KWEB Team");
    text(L"DisplayIcon", exe + L",0");
    text(L"InstallLocation", dir);
    text(L"UninstallString", L"\"" + exe + L"\" --uninstall");
    text(L"URLInfoAbout", L"https://uaro.kiev.ua");
    number(L"NoModify", 1);
    number(L"NoRepair", 1);
    RegCloseKey(key);
    return ok;
}

bool removeAppsEntry(const std::wstring& name) {
    HKEY parent = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kAppsParent, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &parent) != ERROR_SUCCESS)
        return false;
    const LONG r = RegDeleteTreeW(parent, name.c_str());
    RegCloseKey(parent);
    return r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND;
}

}  // namespace uagfx::app
