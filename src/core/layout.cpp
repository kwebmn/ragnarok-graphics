#include "core/layout.hpp"

#include <cctype>

namespace uagfx::layout {
namespace {

struct Place {
    const char* now;
    const char* before;
    bool folder;
};

// In the order of File.
const Place kPlaces[kFileCount] = {
    {"uaro-graphics\\uaro-graphics.ini", "uaro-graphics.ini", false},
    {"uaro-graphics\\logs\\uaro-graphics.log", "uaro-graphics.log", false},
    {"uaro-graphics\\logs\\uaro-render64.log", "uaro-render64.log", false},
    {"uaro-graphics\\logs\\uaro-pack.log", "uaro-pack.log", false},
    {"uaro-graphics\\logs\\uaro-studio-engine.log", "uaro-studio-engine.log", false},
    {"uaro-graphics\\logs\\uaro-graphics-report.txt", "uaro-graphics-report.txt", false},
    {"uaro-graphics\\cache\\pipelines.txt", "uaro-graphics-pipelines.txt", false},
    {"uaro-graphics\\cache\\shaders-32.bin", "uaro-graphics\\shaders-32.bin", false},
    {"uaro-graphics\\cache\\shaders-64.bin", "uaro-graphics\\shaders-64.bin", false},
    {"uaro-graphics\\cache\\shaders-d3d11.bin", "uaro-graphics\\shaders-d3d11.bin", false},
    {"uaro-graphics\\cache\\studio-shaders-64.bin", "uaro-graphics\\studio-shaders-64.bin", false},
    {"uaro-graphics\\textures\\uaro-textures.zip", "uaro-textures.zip", false},
    {"uaro-graphics\\backup", "uaro-graphics-backup", true},
    {"uaro-graphics\\dgvoodoo", "dgvoodoo", true},
    // 2.9: uaRO Pad, a part of the driver now - its settings, the characters' layouts, its log
    {"uaro-graphics\\uaro-pad.ini", "uaro-pad.ini", false},
    {"uaro-graphics\\uaro-pad-binds.ini", "uaro-pad-binds.ini", false},
    {"uaro-graphics\\logs\\uaro-pad.log", "uaro-pad.log", false},
};

const Place& place(File f) {
    const int i = int(f);
    return kPlaces[i >= 0 && i < kFileCount ? i : 0];
}

bool sameName(const std::string& a, const char* b) {
    size_t i = 0;
    for (; i < a.size() && b[i]; ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    return i == a.size() && !b[i];
}

}  // namespace

const char* now(File f) { return place(f).now; }

const char* before(File f) { return place(f).before; }

bool isFolder(File f) { return place(f).folder; }

std::string aiShadersBeside(const std::string& shaderCache) {
    if (shaderCache.empty()) return {};
    const size_t name = shaderCache.find_last_of("\\/") + 1;  // npos + 1: a bare name
    return shaderCache.substr(0, name) + "ai-" + shaderCache.substr(name);
}

std::string notes(const std::string& langToken) { return "uaro-graphics\\notes\\TEST." + langToken + ".md"; }

std::string join(const std::string& gameDir, const std::string& relative) { return gameDir + "\\" + relative; }

std::string locate(const std::string& gameDir, File f, const std::function<bool(const std::string&)>& exists) {
    const std::string fresh = join(gameDir, now(f));
    if (exists(fresh)) return fresh;
    const std::string old = join(gameDir, before(f));
    return exists(old) ? old : fresh;
}

std::string relativeNow(const std::string& relative) {
    for (const Place& p : kPlaces)
        if (sameName(relative, p.before)) return p.now;
    return relative;
}

}  // namespace uagfx::layout
