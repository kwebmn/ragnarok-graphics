#include "core/map_textures.hpp"

#include <cstring>
#include <set>

#include "core/rsm.hpp"
#include "core/rsw.hpp"
#include "core/zip.hpp"

namespace uagfx::map_textures {

std::vector<std::string> groundTextures(const uint8_t* data, size_t size) {
    std::vector<std::string> out;
    // "GRGN", the version (2 bytes), width, height, zoom; then the textures' count and the length of a name
    if (!data || size < 26 || std::memcmp(data, "GRGN", 4) != 0) return out;
    uint32_t count = 0, length = 0;
    std::memcpy(&count, data + 18, 4);
    std::memcpy(&length, data + 22, 4);
    if (!length || length > 256 || count > 4096 || 26 + uint64_t(count) * length > size) return out;
    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reinterpret_cast<const char*>(data + 26 + size_t(i) * length);
        out.emplace_back(name, strnlen(name, length));
    }
    return out;
}

std::string mapName(const std::string& text) {
    std::string s = zip::normalName(text);
    const size_t from = s.find_first_not_of(" \t");
    const size_t to = s.find_last_not_of(" \t");
    s = from == std::string::npos ? std::string() : s.substr(from, to - from + 1);
    if (s.compare(0, 5, "data\\") == 0) s = s.substr(5);
    for (const char* ext : {".gnd", ".rsw", ".gat"})
        if (s.size() > 4 && s.compare(s.size() - 4, 4, ext) == 0) s.resize(s.size() - 4);
    if (s.empty() || s.find_first_of("\\:*?\"<>|") != std::string::npos || s.find("..") != std::string::npos)
        return "";
    return s;
}

std::vector<std::string> filesOf(const std::string& map, bool ground, bool models, const Read& read) {
    const std::string name = mapName(map);
    if (name.empty()) return {};
    std::set<std::string> out;
    const auto texture = [&](const std::string& t) {
        if (!t.empty()) out.insert(zip::normalName("data\\texture\\" + t));
    };
    std::vector<uint8_t> data;
    if (ground && read("data\\" + name + ".gnd", data))
        for (const std::string& t : groundTextures(data.data(), data.size())) texture(t);
    if (models && read("data\\" + name + ".rsw", data)) {
        rsw::World world;
        std::string error;
        if (rsw::parse(data.data(), data.size(), world, error)) {
            std::set<std::string> seen;
            for (const rsw::Model& m : world.models) {
                const std::string file = zip::normalName("data\\model\\" + m.file);
                if (!seen.insert(file).second || !read(file, data)) continue;
                rsm::Model model;
                if (rsm::parse(data.data(), data.size(), model, error))
                    for (const rsm::Part& p : model.parts) texture(p.texture);
            }
        }
    }
    return {out.begin(), out.end()};
}

}  // namespace uagfx::map_textures
