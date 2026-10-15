#include <cstdint>
#include <string>
#include <vector>

#include "core/near_picture.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// A picture of width x height whose top-left cw x ch holds a pattern of its own (seed), the rest `fill`.
std::vector<uint32_t> picture(int width, int height, int cw, int ch, int seed, uint32_t fill = 0) {
    std::vector<uint32_t> p(size_t(width) * size_t(height), fill);
    for (int y = 0; y < ch; ++y)
        for (int x = 0; x < cw; ++x) {
            // Smooth across the picture and different for every seed, as photographs are.
            const uint32_t r = uint32_t((x * 255 / cw + seed * 37) & 255);
            const uint32_t g = uint32_t((y * 255 / ch + seed * 61) & 255);
            const uint32_t b = uint32_t((x * y / 2000 + seed * 53) & 255);
            p[size_t(y) * size_t(width) + size_t(x)] = 0xFF000000u | r << 16 | g << 8 | b;
        }
    return p;
}

// What another decoder gives: a level or two off here and there, (the game's 16-bit textures) the low bits gone.
std::vector<uint32_t> decodedElsewhere(std::vector<uint32_t> p) {
    for (size_t i = 0; i < p.size(); ++i) {
        uint32_t v = p[i];
        const int d = int(uint32_t(i) * 2654435761u >> 30) - 1;  // -1, 0, 1, 2
        uint32_t out = v & 0xFF000000u;
        for (int c = 0; c < 3; ++c) {
            int x = int((v >> (8 * c)) & 255) + d;
            x = x < 0 ? 0 : x > 255 ? 255 : x;
            out |= uint32_t(x & 0xF8) << (8 * c);
        }
        p[i] = out;
    }
    return p;
}

}  // namespace

TEST(near_picture_likeness_round_trips_as_text) {
    // 0.49: the pack's index keeps a JPG picture's likeness - the game's texture size, the picture's in its top left
    // corner, 16 x 16 cells of average colour - on a line of its own.
    const std::vector<uint32_t> p = picture(1024, 1024, 800, 600, 1);
    const pack::Likeness l = pack::likenessOf(p.data(), 1024, 1024, 800, 600);
    CHECK(l.width == 1024 && l.height == 1024 && l.contentWidth == 800 && l.contentHeight == 600);
    pack::Likeness back;
    CHECK(pack::parseLikeness(pack::likenessText(l), back));
    CHECK(back.grid == l.grid && back.width == 1024 && back.contentHeight == 600);
    CHECK(!pack::parseLikeness("1024\t1024\t800\t600\tzz", back));
    CHECK(!pack::parseLikeness("", back));
}

TEST(near_picture_finds_its_picture_decoded_elsewhere) {
    // The loading screens and the water are .jpg: the game decodes them itself (ijl15.dll), the builder with
    // stb_image - the same picture a level or two apart is found, another one is not.
    pack::NearPictures near;
    for (int seed = 0; seed < 20; ++seed) {
        const std::vector<uint32_t> p = picture(1024, 1024, 800, 600, seed);
        near.add("ui/1024x1024/" + std::to_string(seed) + ".webp", pack::likenessOf(p.data(), 1024, 1024, 800, 600));
    }
    CHECK(near.has(1024, 1024));
    CHECK(!near.has(256, 256));
    std::string name;
    // The game's texture: the picture in its corner, the rest whatever it holds.
    const std::vector<uint32_t> game = decodedElsewhere(picture(1024, 1024, 800, 600, 7, 0xFF123456u));
    CHECK(near.find(game.data(), 1024, 1024, name));
    CHECK(name == "ui/1024x1024/7.webp");
    const std::vector<uint32_t> other = picture(1024, 1024, 800, 600, 99);
    CHECK(!near.find(other.data(), 1024, 1024, name));
    const std::vector<uint32_t> small = picture(256, 256, 256, 256, 7);
    CHECK(!near.find(small.data(), 256, 256, name));
}

TEST(near_picture_tells_the_frames_of_an_animation_apart) {
    // The water's frames differ a little from one to the next: each still finds its own.
    pack::NearPictures near;
    std::vector<std::vector<uint32_t>> frames;
    for (int f = 0; f < 32; ++f) {
        std::vector<uint32_t> p(128 * 128);
        for (int y = 0; y < 128; ++y)
            for (int x = 0; x < 128; ++x) {
                const int wave = int(40.0 * ((x + y + f * 4) % 128) / 128.0);  // 32 frames, each its own
                p[size_t(y) * 128 + size_t(x)] = 0xFF000000u | uint32_t(40 + wave) << 8 | uint32_t(120 + wave);
            }
        near.add("world/128x128/" + std::to_string(f) + ".webp", pack::likenessOf(p.data(), 128, 128, 128, 128));
        frames.push_back(p);
    }
    std::string name;
    for (int f = 0; f < 32; ++f) {
        const std::vector<uint32_t> game = decodedElsewhere(frames[size_t(f)]);
        CHECK(near.find(game.data(), 128, 128, name));
        CHECK(name == "world/128x128/" + std::to_string(f) + ".webp");
    }
}
