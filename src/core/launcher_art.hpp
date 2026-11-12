#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::launcherart {

// 3.0.18-beta.193 (docs/superpowers/specs/2026-11-12-launcher-design.md): the launcher's art - uaRO HD's shots of
// Prontera's fountain at dawn, by day, at sunset and in winter (uaro-graphics\launcher\art-*.jpg) - read, laid over the
// window's client as a cover (scaled to fill it, the middle kept), the next one faded in over kCrossfadeMs every
// kHoldMs while the window is in front and the game is not running (the owner chose the living art).

struct Image {
    int w = 0, h = 0;
    std::vector<uint32_t> argb;  // 0xAARRGGBB: opaque for the art, straight alpha for the logo
};

constexpr uint32_t kGround = 0xFF0B0E14u;  // where there is no art: the launcher's dark ground
constexpr int kCrossfadeMs = 2000, kHoldMs = 20000;

// A JPEG's bytes as an Image; empty (w 0) for anything else
Image decode(const std::string& bytes);
// `src` scaled to cover w x h (cut at the sides or the top and bottom, its middle kept), nearest texel; out w * h
// pixels, all kGround when `src` is empty
void cover(const Image& src, int w, int h, std::vector<uint32_t>& out);
// a = a * (1 - t) + b * t, channel by channel (the same size; alpha kept opaque)
void blend(std::vector<uint32_t>& a, const std::vector<uint32_t>& b, float t);

// 3.0.18-beta.203 (the owner: «лого на картинке с заставки самого клиента взять»): the logo - logo.webp beside the
// art, cut out of the client's own login picture (bgi_temp.bmp in uaRO HD's uaro.grf) by tools/launcher_logo.py
Image decodeLogo(const std::string& bytes);  // a WebP's pixels, straight alpha; empty (w 0) for anything else
// `src` (straight alpha) made w x h - the mean of what each pixel covers when smaller, smooth between its pixels when
// bigger - premultiplied (the colours times alpha); all clear when `src` is empty
void fitted(const Image& src, int w, int h, std::vector<uint32_t>& out);
// premultiplied `src` (w x h) laid over the opaque `dst` (dw x dh) at (x, y), cut at dst's edges
void over(std::vector<uint32_t>& dst, int dw, int dh, const std::vector<uint32_t>& src, int w, int h, int x, int y);

}  // namespace uagfx::launcherart
