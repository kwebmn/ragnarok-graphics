#include "core/webp_image.hpp"

#include <webp/decode.h>
#include <webp/encode.h>

#include <algorithm>
#include <cstring>

namespace uagfx::webp {
namespace {

int toMemory(const uint8_t* data, size_t size, const WebPPicture* picture) {
    auto* out = static_cast<std::vector<uint8_t>*>(picture->custom_ptr);
    out->insert(out->end(), data, data + size);
    return 1;
}

}  // namespace

std::vector<uint8_t> encode(const uint32_t* argb, int width, int height, int quality) {
    std::vector<uint8_t> out;
    if (!argb || width <= 0 || height <= 0) return out;
    WebPConfig config;
    WebPPicture picture;
    if (!WebPConfigInit(&config) || !WebPPictureInit(&picture)) return out;
    config.lossless = quality >= 100 ? 1 : 0;
    config.quality = float(std::clamp(quality, 0, 100));
    config.method = 4;
    config.alpha_quality = 100;
    config.exact = 1;
    picture.use_argb = 1;
    picture.width = width;
    picture.height = height;
    // ARGB values in memory are B, G, R, A bytes on the little-endian processors this runs on.
    if (!WebPPictureImportBGRA(&picture, reinterpret_cast<const uint8_t*>(argb), width * 4)) {
        WebPPictureFree(&picture);
        return out;
    }
    picture.writer = toMemory;
    picture.custom_ptr = &out;
    const bool ok = WebPEncode(&config, &picture) != 0;
    WebPPictureFree(&picture);
    if (!ok) out.clear();
    return out;
}

bool decode(const uint8_t* data, size_t size, std::vector<uint32_t>& argb, int& width, int& height) {
    int w = 0, h = 0;
    uint8_t* bgra = data && size ? WebPDecodeBGRA(data, size, &w, &h) : nullptr;
    if (!bgra || w <= 0 || h <= 0) {
        if (bgra) WebPFree(bgra);
        return false;
    }
    argb.resize(size_t(w) * size_t(h));
    std::memcpy(argb.data(), bgra, argb.size() * 4);
    WebPFree(bgra);
    width = w;
    height = h;
    return true;
}

}  // namespace uagfx::webp
