#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::pack {

// 0.49: the pictures the game decodes itself - its .jpg ones (the loading screens, the water), through ijl15.dll -
// come out a level or two apart from the builder's decoder: their keys differ, so the game finds their pictures in the
// pack by their likeness instead - the game's texture size, the picture's in its top left corner, and 16 x 16 cells of
// that picture's average colour.
constexpr int kLikenessCells = 16;
struct Likeness {
    int width = 0, height = 0;               // the game's texture
    int contentWidth = 0, contentHeight = 0;  // the picture in its top left corner
    std::array<uint8_t, kLikenessCells * kLikenessCells * 3> grid{};  // R, G, B a cell, row by row
};
Likeness likenessOf(const uint32_t* argb, int width, int height, int contentWidth, int contentHeight);
std::string likenessText(const Likeness& l);  // "1024\t1024\t800\t600\t<hex>": the index's line after the name
bool parseLikeness(const std::string& text, Likeness& out);
double distance(const Likeness& a, const Likeness& b);  // the cells' average difference, 0-255

class NearPictures {
public:
    void add(const std::string& name, const Likeness& l);
    bool has(int width, int height) const;
    // The picture most like the game's texture of width x height, when it is near enough to be the same one.
    bool find(const uint32_t* argb, int width, int height, std::string& name) const;
    size_t size() const { return all_.size(); }

private:
    std::vector<std::pair<std::string, Likeness>> all_;
};

}  // namespace uagfx::pack
