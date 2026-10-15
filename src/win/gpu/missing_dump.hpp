#pragma once
#include <cstdint>
#include <string>

namespace uagfx::gpu {

// 0.49.1: the first 64 pictures of a run the texture pack had none of - uaro-graphics\missing\<what>-<key>.bmp in the
// game's folder - for the developers to find why (a sprite frame the builder makes otherwise, say).
class MissingDump {
public:
    void setGameDir(const std::string& dir) { dir_ = dir + "\\uaro-graphics\\missing"; }
    void save(const char* what, const std::string& key, const uint32_t* argb, int width, int height);

private:
    std::string dir_;
    uint32_t saved_ = 0;
};

}  // namespace uagfx::gpu
