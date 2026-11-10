#pragma once
#include <cstdint>
#include <functional>
#include <utility>
#include <string>
#include <vector>

namespace uagfx::spritefile {

// 3.0.11-beta.14 (the owner: monster pictures in «Довідник» from the client's own sprites, «делай, и не спрашивай»):
// the game's sprite files - .spr (its pictures: palette ones, RLE packed from 2.1, and true colour ones; the palette
// at the file's end, index 0 see-through) and .act (its actions: frames of layers - a picture, where its centre is from
// the feet, mirrored, tinted, scaled). A monster's picture is its first action's first frame (standing, facing down).
// No Windows here.

struct Image {
    int w = 0, h = 0;
    std::vector<uint32_t> argb;  // rows top down; 0 see-through
};
struct Sprite {
    std::vector<Image> palette, truecolour;  // as the .act's layers name them: type 0 and type 1
};
bool readSpr(const std::string& bytes, Sprite& out);

struct Layer {
    int x = 0, y = 0;  // the picture's centre from the feet
    int index = -1;    // -1: none
    bool mirror = false;
    uint32_t tint = 0xFFFFFFFF;  // ARGB, multiplied in
    float scaleX = 1, scaleY = 1;
    int type = 0;  // 0 palette, 1 true colour
};
struct Frame {
    std::vector<Layer> layers;
    // 3.0.18-beta.111: its anchors (x, y from the feet) - where the next part fixes (the body's neck, a head's top)
    std::vector<std::pair<int, int>> anchors;
};
struct Act {
    std::vector<std::vector<Frame>> actions;
};
bool readAct(const std::string& bytes, Act& out);

// An action's frame put together (its layers in order, over each other) and cut to what it shows; false: nothing
bool frameImage(const Sprite& s, const Act& a, size_t action, size_t frame, Image& out);
// 3.0.18-beta.111 (the owner: the character in «Спорядження» from the game's sprites): parts put together in order,
// each over the ones before - the body where it stands, an anchored one moved by the body's anchor less its own (as
// the game puts a head and its headgear on the body), then by (dx, dy). `footX`, `footY` where the feet are
struct Part {
    const Sprite* sprite = nullptr;
    const Act* act = nullptr;
    size_t action = 0, frame = 0;
    bool anchored = true;
    int dx = 0, dy = 0;
    bool body = false;  // the anchors' source (else the first part): one drawn under the body (a cart) may come first
};
bool compose(const std::vector<Part>& parts, Image& out, int& footX, int& footY);
// 3.0.11-beta.76: the same `factor` times its size - each palette layer's picture from `bigger` (its index) when it
// gives one (factor times that picture: the AI pack's frame), else the picture itself stretched
bool frameImageScaled(const Sprite& s, const Act& a, size_t action, size_t frame, int factor,
                      const std::function<const Image*(int index)>& bigger, Image& out);
// A picture fitted into a w x h box (premultiplied ARGB, see-through around it): small ones grown by whole steps (the
// pixels kept sharp, at most `most` times), big ones shrunk smoothly
Image fitted(const Image& in, int w, int h, int most = 3);
// 3.0.11-beta.196: a face from an NPC's whole picture - the square at its top, as wide as the picture or `share` of its
// height (the narrower), in its middle
Image topSquare(const Image& in, float share);

}  // namespace uagfx::spritefile
