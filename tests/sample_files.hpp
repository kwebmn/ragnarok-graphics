#pragma once
// Sample map and model files the tests share: a map file (.rsw) of version 2.1 with one object of each kind - its
// model tree.rsm - and that model (.rsm, version 1.4) with the textures bark.bmp and leaf.bmp.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace testfiles {

// A map file of version 2.1 with one object of each kind.
struct RswWriter {
    std::vector<uint8_t> b;
    void bytes(const void* p, size_t n) {
        b.insert(b.end(), static_cast<const uint8_t*>(p), static_cast<const uint8_t*>(p) + n);
    }
    void i32(int32_t v) { bytes(&v, 4); }
    void f32(float v) { bytes(&v, 4); }
    void text(const char* s, size_t n) {
        std::vector<char> z(n, 0);
        std::memcpy(z.data(), s, std::min(n, std::strlen(s)));
        bytes(z.data(), n);
    }
};

inline std::vector<uint8_t> rsw() {
    RswWriter w;
    w.bytes("GRSW", 4);
    const uint8_t version[2] = {2, 1};
    w.bytes(version, 2);
    for (int i = 0; i < 4; ++i) w.text("x", 40);  // ini, gnd, gat, src
    w.f32(-5.0f);                                  // water: level, type, wave height, speed, pitch, animation
    w.i32(4);
    w.f32(1.0f);
    w.f32(2.0f);
    w.f32(50.0f);
    w.i32(3);
    w.i32(45);  // light: longitude, latitude, diffuse, ambient, opacity
    w.i32(45);
    for (int i = 0; i < 6; ++i) w.f32(0.5f);
    w.f32(1.0f);
    for (int i = 0; i < 4; ++i) w.i32(0);  // the ground box
    w.i32(4);                              // objects
    w.i32(1);                              // a model
    w.text("tree", 40);
    w.i32(0);
    w.f32(1.0f);
    w.i32(0);
    w.text("tree.rsm", 80);
    w.text("node", 80);
    for (int i = 0; i < 9; ++i) w.f32(1.0f);
    w.i32(2);  // a light
    w.text("lamp", 80);
    w.f32(10.0f);
    w.f32(-20.0f);
    w.f32(30.0f);
    w.f32(1.0f);
    w.f32(0.5f);
    w.f32(0.25f);
    w.f32(120.0f);
    w.i32(3);  // a sound
    w.text("s", 80);
    w.text("s.wav", 80);
    for (int i = 0; i < 3; ++i) w.f32(0.0f);
    w.f32(1.0f);
    w.i32(10);
    w.i32(10);
    w.f32(50.0f);
    w.f32(4.0f);  // its cycle (2.0 and later)
    w.i32(4);     // an effect
    w.text("torch", 80);
    w.f32(-100.0f);
    w.f32(-12.0f);
    w.f32(200.0f);
    w.i32(690);
    w.f32(0.0f);
    for (int i = 0; i < 4; ++i) w.f32(float(i));
    return w.b;
}

struct RsmBytes {
    std::vector<uint8_t> b;
    template <typename T>
    void put(T v) {
        const auto* p = reinterpret_cast<const uint8_t*>(&v);
        b.insert(b.end(), p, p + sizeof v);
    }
    void name(const std::string& s, size_t width) {
        std::string t = s;
        t.resize(width, '\0');
        b.insert(b.end(), t.begin(), t.end());
    }
};

// A model of version 1.4, one node: a trunk (texture 0, two faces) 2 wide, 10 tall over y 0-10 of the file, and a
// crown (texture 1, one face) 8 wide over y -4 to 2 - the file's y down, as the game's (up along -y).
inline std::vector<uint8_t> rsm() {
    RsmBytes w;
    w.b = {'G', 'R', 'S', 'M', 1, 4};
    w.put<int32_t>(0);
    w.put<int32_t>(1);
    w.put<uint8_t>(255);
    for (int i = 0; i < 16; ++i) w.put<uint8_t>(0);
    w.put<int32_t>(2);
    w.name("bark.bmp", 40);
    w.name("leaf.bmp", 40);
    w.name("main", 40);
    w.put<int32_t>(1);
    w.name("main", 40);
    w.name("", 40);
    w.put<int32_t>(2);
    w.put<int32_t>(0);
    w.put<int32_t>(1);
    const float mat[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    for (float f : mat) w.put(f);
    for (int i = 0; i < 3; ++i) w.put(0.0f);  // offset
    for (int i = 0; i < 3; ++i) w.put(0.0f);  // pos
    w.put(0.0f);                              // angle
    w.put(0.0f), w.put(1.0f), w.put(0.0f);    // axis
    w.put(1.0f), w.put(1.0f), w.put(1.0f);    // scale
    const float v[][3] = {{-1, 10, 0}, {1, 10, 0}, {1, 0, 0}, {-1, 0, 0}, {-4, 2, -4}, {4, 2, 4}, {0, -4, 0}};
    w.put<int32_t>(7);
    for (const auto& p : v)
        for (float f : p) w.put(f);
    w.put<int32_t>(0);  // texture coordinates
    w.put<int32_t>(3);
    const uint16_t faces[][4] = {{0, 1, 2, 0}, {0, 2, 3, 0}, {4, 5, 6, 1}};
    for (const auto& f : faces) {
        w.put(f[0]), w.put(f[1]), w.put(f[2]);
        w.put<uint16_t>(0), w.put<uint16_t>(0), w.put<uint16_t>(0);
        w.put(f[3]);
        w.put<uint16_t>(0);
        w.put<int32_t>(0);
        w.put<int32_t>(0);
    }
    w.put<int32_t>(0);  // rotation keys
    return w.b;
}

// 3.0.4-beta.14: a wall of 4 x 6 standing on the ground at x 1..5, its texture's coordinates (version 1.4: a colour
// and u, v each), the second of its two faces drawn on both sides
inline std::vector<uint8_t> rsmWall() {
    RsmBytes w;
    w.b = {'G', 'R', 'S', 'M', 1, 4};
    w.put<int32_t>(0);
    w.put<int32_t>(1);
    w.put<uint8_t>(255);
    for (int i = 0; i < 16; ++i) w.put<uint8_t>(0);
    w.put<int32_t>(1);
    w.name("wall.bmp", 40);
    w.name("main", 40);
    w.put<int32_t>(1);
    w.name("main", 40);
    w.name("", 40);
    w.put<int32_t>(1);
    w.put<int32_t>(0);
    const float mat[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    for (float f : mat) w.put(f);
    for (int i = 0; i < 3; ++i) w.put(0.0f);  // offset
    for (int i = 0; i < 3; ++i) w.put(0.0f);  // pos
    w.put(0.0f);                              // angle
    w.put(0.0f), w.put(1.0f), w.put(0.0f);    // axis
    w.put(1.0f), w.put(1.0f), w.put(1.0f);    // scale
    const float v[][3] = {{1, 0, 0}, {5, 0, 0}, {5, -6, 0}, {1, -6, 0}};  // up along -y
    w.put<int32_t>(4);
    for (const auto& p : v)
        for (float f : p) w.put(f);
    const float uv[][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    w.put<int32_t>(4);
    for (const auto& t : uv) w.put<uint32_t>(0xffffffffu), w.put(t[0]), w.put(t[1]);
    w.put<int32_t>(2);
    const uint16_t faces[][3] = {{0, 1, 2}, {0, 2, 3}};
    for (int k = 0; k < 2; ++k) {
        for (uint16_t i : faces[k]) w.put(i);
        for (uint16_t i : faces[k]) w.put(i);  // its corners' texture coordinates
        w.put<uint16_t>(0);                    // its texture
        w.put<uint16_t>(0);
        w.put<int32_t>(k == 1 ? 1 : 0);  // both sides
        w.put<int32_t>(0);
    }
    w.put<int32_t>(0);  // rotation keys
    return w.b;
}

// 2.3: a ground file (.gnd, version 1.7): its textures, one lightmap, a top surface a cell. cells[y * width + x]:
// the cell's texture and its corners' UV ({u0, u1, u2, u3}, {v0, v1, v2, v3}: (x, y), (x+1, y), (x, y+1),
// (x+1, y+1)); texture -1: no top surface there.
struct GndCell {
    int texture = -1;
    float u[4] = {0, 1, 0, 1};
    float v[4] = {0, 0, 1, 1};
};

inline std::vector<uint8_t> gnd(const std::vector<std::string>& textures, int width, int height,
                                const std::vector<GndCell>& cells) {
    RswWriter w;
    w.bytes("GRGN", 4);
    const uint8_t version[2] = {1, 7};
    w.bytes(version, 2);
    w.i32(width);
    w.i32(height);
    w.f32(10.0f);
    w.i32(int32_t(textures.size()));
    w.i32(80);
    for (const std::string& t : textures) w.text(t.c_str(), 80);
    w.i32(1);  // one lightmap of 8 x 8, 1 cell
    w.i32(8);
    w.i32(8);
    w.i32(1);
    std::vector<uint8_t> light(8 * 8 * 4, 0x80);
    w.bytes(light.data(), light.size());
    int surfaces = 0;
    for (const GndCell& c : cells) surfaces += c.texture >= 0;
    w.i32(surfaces);
    for (const GndCell& c : cells) {
        if (c.texture < 0) continue;
        for (float u : c.u) w.f32(u);
        for (float v : c.v) w.f32(v);
        const int16_t texture = int16_t(c.texture);
        const uint16_t lightmap = 0;
        w.bytes(&texture, 2);
        w.bytes(&lightmap, 2);
        const uint8_t colour[4] = {255, 255, 255, 255};
        w.bytes(colour, 4);
    }
    int surface = 0;
    for (const GndCell& c : cells) {
        const float heights[4] = {0, 0, 0, 0};
        w.bytes(heights, sizeof heights);
        w.i32(c.texture >= 0 ? surface++ : -1);  // top
        w.i32(-1);                                // front
        w.i32(-1);                                // right
    }
    return w.b;
}

// 2.3: a 24-bit BMP of w x h from rows of 0xRRGGBB (top first) - a world texture as the game's files have it.
inline std::vector<uint8_t> bmp24(int w, int h, const std::vector<uint32_t>& top) {
    RswWriter b;
    const uint32_t stride = (uint32_t(w) * 3 + 3) & ~3u, bits = 0x36;
    const auto u16 = [&b](uint32_t v) {
        const uint16_t x = uint16_t(v);
        b.bytes(&x, 2);
    };
    const auto u32 = [&b](uint32_t v) { b.bytes(&v, 4); };
    b.bytes("BM", 2);
    u32(bits + stride * uint32_t(h));
    u32(0);
    u32(bits);
    u32(40);
    u32(uint32_t(w));
    u32(uint32_t(h));
    u16(1);
    u16(24);
    for (int i = 0; i < 6; ++i) u32(0);
    for (int y = h - 1; y >= 0; --y) {
        for (int x = 0; x < w; ++x) {
            const uint32_t v = top[size_t(y * w + x)];
            const uint8_t px[3] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16)};
            b.bytes(px, 3);
        }
        for (uint32_t pad = uint32_t(w) * 3; pad < stride; ++pad) b.bytes("\0", 1);
    }
    return b.b;
}

// 2.3: a sprite (SPR 2.1) of one palette frame (w x h of palette indices, no runs of zeros) and its palette.
inline std::vector<uint8_t> spr(int w, int h, const std::vector<uint8_t>& indices,
                                const std::vector<uint8_t>& palette) {
    RswWriter b;
    const auto u16 = [&b](uint32_t v) {
        const uint16_t x = uint16_t(v);
        b.bytes(&x, 2);
    };
    b.bytes("SP\x01\x02", 4);
    u16(1);  // palette frames
    u16(0);  // 32-bit frames
    u16(uint32_t(w));
    u16(uint32_t(h));
    u16(uint32_t(indices.size()));
    b.bytes(indices.data(), indices.size());
    b.bytes(palette.data(), palette.size());
    return b.b;
}

// 2.3: a palette (1024 bytes, RGBA) whose colour 1 is (r, g, b), colour 2 magenta, colour 3 light grey.
inline std::vector<uint8_t> palette(uint8_t r, uint8_t g, uint8_t b) {
    std::vector<uint8_t> p(1024, 0);
    p[4] = r, p[5] = g, p[6] = b;
    p[8] = 255, p[9] = 0, p[10] = 255;
    p[12] = p[13] = p[14] = 220;
    return p;
}

// 2.3: w x h palette indices, 1 and 3 in a checker - a frame with detail (one of a flat colour is soft light)
inline std::vector<uint8_t> checker(int w, int h) {
    std::vector<uint8_t> out(size_t(w) * size_t(h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) out[size_t(y * w + x)] = uint8_t((x + y) % 2 ? 3 : 1);
    return out;
}

}  // namespace testfiles
