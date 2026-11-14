#include "core/mob_pictures.hpp"

#include <algorithm>
#include <cstring>

#include "core/client_libs.hpp"

namespace uagfx::mobpictures {
namespace {

struct MobPictureBlob {
    const unsigned char* data;
    size_t size;
};
struct MobPictureOf {
    int id, picture;
};

#include "core/mob_pictures.inc"

uint32_t le16(const unsigned char* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8; }
uint32_t le32(const unsigned char* p) { return le16(p) | le16(p + 2) << 16; }

}  // namespace

size_t count() { return sizeof kMobPictureOf / sizeof kMobPictureOf[0]; }

bool pixels(int id, std::vector<uint32_t>& argb, int& w, int& h) {
    const MobPictureOf* const end = kMobPictureOf + count();
    const MobPictureOf* const at =
        std::lower_bound(kMobPictureOf, end, id, [](const MobPictureOf& a, int b) { return a.id < b; });
    if (at == end || at->id != id) return false;
    const MobPictureBlob& blob = kMobPictureBlobs[at->picture];
    std::vector<unsigned char> raw(1u << 20);  // the biggest kept is ~250 x 250 colour numbers
    unsigned long len = static_cast<unsigned long>(raw.size());
    if (client_libs::zlibUncompress(raw.data(), &len, blob.data, static_cast<unsigned long>(blob.size)) != 0 ||
        len < 6)
        return false;
    const uint32_t width = le16(raw.data()), height = le16(raw.data() + 2), colours = le16(raw.data() + 4);
    if (!width || !height || !colours || colours > 256 || len < 6 + 4 * colours + width * height) return false;
    std::vector<uint32_t> palette(colours);
    for (uint32_t i = 0; i < colours; ++i) palette[i] = le32(raw.data() + 6 + 4 * i);
    const unsigned char* index = raw.data() + 6 + 4 * colours;
    argb.resize(size_t(width) * height);
    for (size_t i = 0; i < argb.size(); ++i) argb[i] = index[i] < colours ? palette[index[i]] : 0u;
    w = int(width);
    h = int(height);
    return true;
}

}  // namespace uagfx::mobpictures
