#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

namespace uagfx::uitheme {

// 3.0.18-beta.13 (Claude Doc «Концепция: единая библиотека интерфейса uaRO Graphics», the themes): the new
// interface's parts drawn by uaUI (core/ui_scene) in a theme - their colours through it, their corners and edges by
// its factors. [interface] theme = standard | classic | contrast; uaro-graphics\ui\palette.txt over it («#RRGGBB =
// #RRGGBB», «radius = 0.5», «edge = 1.5», «#» a remark).
enum class Greys : uint8_t { Keep, Warm, Spread };
struct Theme {
    std::string name = "standard";
    Greys greys = Greys::Keep;  // the colours with no hue to speak of (the grounds, the words, the edges)
    float radius = 1, edge = 1;
    std::unordered_map<uint32_t, uint32_t> exact;  // the palette's own, before the greys' rule
};

Theme named(const std::string& name);  // an unknown name: standard
uint32_t colour(const Theme& t, uint32_t rgb);  // 0xRRGGBB
// The palette's lines over `t`; the first line not understood in `bad` ("" none)
void read(const std::string& text, Theme& t, std::string& bad);

}  // namespace uagfx::uitheme
