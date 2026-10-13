#include "core/settings/input_module.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace uagfx::settings {
namespace {

std::string lower(std::string s) {
    for (char& ch : s) ch = char(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

bool contains(const std::vector<uint8_t>& data, const char* text) {
    const size_t n = std::strlen(text);
    return std::search(data.begin(), data.end(), text, text + n) != data.end();
}

// The first NUL-terminated ASCII string ".\<name>.ini" in the DLL.
std::string iniNamed(const std::vector<uint8_t>& data) {
    for (size_t i = 0; i + 2 < data.size(); ++i) {
        if (data[i] != '.' || data[i + 1] != '\\' || (i > 0 && data[i - 1] != 0)) continue;
        size_t end = i + 2;
        while (end < data.size() && data[end] >= 0x20 && data[end] < 0x7F && data[end] != '\\' && data[end] != '/')
            ++end;
        if (end >= data.size() || data[end] != 0) continue;
        const std::string name(data.begin() + long(i) + 2, data.begin() + long(end));
        if (name.size() > 4 && lower(name.substr(name.size() - 4)) == ".ini") return name;
    }
    return "";
}

int number(const std::map<std::string, std::string>& ini, const char* key) {
    const auto it = ini.find(key);
    return it == ini.end() ? 0 : int(std::strtol(it->second.c_str(), nullptr, 10));
}

}  // namespace

ModuleInfo identifyModule(const std::string& fileName, const std::vector<uint8_t>& dll) {
    ModuleInfo m;
    if (contains(dll, "uaro-pad.ini")) {
        m.kind = ModuleKind::UaroPad;
        m.ini = "uaro-pad.ini";
    } else if (contains(dll, "ROExt")) {
        m.kind = ModuleKind::Roext;
        m.ini = iniNamed(dll);
        if (m.ini.empty()) {
            const size_t dot = fileName.find('.');
            m.ini = fileName.substr(0, dot) + ".ini";
        }
    }
    return m;
}

bool loadedByGame(const std::string& fileName) {
    const std::string name = lower(fileName);
    return name == "dinput.dll" || name == "dinput8.dll";
}

RoextWindow roextWindow(const std::map<std::string, std::string>& ini) {
    RoextWindow w;
    w.active = true;
    w.lock = number(ini, "roext.windowlock") != 0;
    const int width = number(ini, "roext.windowwidth");
    const int height = number(ini, "roext.windowheight");
    if (width > 0 && height > 0) w.size = {width, height};  // ROExt ignores its size when either side is 0
    return w;
}

}  // namespace uagfx::settings
