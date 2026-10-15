#include "win/gpu/missing_dump.hpp"

#include <windows.h>

#include <cstdio>
#include <vector>

#include "core/bmp_write.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

void MissingDump::save(const char* what, const std::string& key, const uint32_t* argb, int width, int height) {
    if (dir_.empty() || saved_ >= 64 || !argb) return;
    if (saved_ == 0) {
        CreateDirectoryA(dir_.c_str(), nullptr);
        LOG_INFO("AI textures: the first pictures the pack has none of are saved in uaro-graphics\\missing");
    }
    ++saved_;
    std::string name = key;
    for (char& c : name)
        if (c == '\\' || c == '/') c = '_';
    const std::vector<uint8_t> b = bmp::write32(argb, width, height);
    if (std::FILE* f = std::fopen((dir_ + "\\" + what + "-" + name + ".bmp").c_str(), "wb")) {
        std::fwrite(b.data(), 1, b.size(), f);
        std::fclose(f);
    }
}

}  // namespace uagfx::gpu
