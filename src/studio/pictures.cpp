#include "studio/pictures.hpp"

#include <algorithm>
#include <exception>

#include <windows.h>

#include "app/text.hpp"
#include "core/webp_image.hpp"
#include "studio/catalog_work.hpp"

namespace uagfx::studio {

std::wstring wideOfGame(const std::string& s) {
    const int n = MultiByteToWideChar(949, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring w(size_t(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(949, 0, s.data(), int(s.size()), w.data(), n);
    return w;
}

std::string utf8OfGame(const std::string& cp949) { return app::utf8(wideOfGame(cp949)); }

pack::Choice choiceOf(const catalog::Catalog& c) {
    pack::Choice choice;
    choice.maxHairColour = c.maxHair;
    choice.maxClothColour = c.maxCloth;
    choice.minSide = c.minSide;
    choice.maxSide = c.maxSide;
    choice.bodyColours = c.bodyColours;
    return choice;
}

bool originalOf(CatalogWork& files, const catalog::Catalog& c, const catalog::FileEntry& f, const catalog::PictureOf& p,
                Image& out) {
    std::vector<uint8_t> data, palette;
    if (!files.read(f.name, data)) return false;
    if (!p.palette.empty() && (!files.read(p.palette, palette) || palette.size() < 1024)) return false;
    const pack::Source s{pack::Source::Type(f.type), ai::Kind(f.kind)};
    try {
        for (pack::Picture& pic :
             pack::picturesOf(s, data.data(), data.size(), choiceOf(c), palette.empty() ? nullptr : palette.data()))
            if (pic.key == p.key) {
                out.width = pic.width;
                out.height = pic.height;
                out.argb = std::move(pic.argb);
                return true;
            }
    } catch (const std::exception&) {
    }
    return false;
}

bool packPictureOf(pack::Reader& r, const catalog::PictureOf& p, const pack::Index& index, Image& out) {
    std::string name = pack::pictureName(pack::Group(p.group), p.key);
    if (!r.has(name)) {
        const auto alias = index.aliases.find(p.key);
        if (alias == index.aliases.end() || !r.has(alias->second)) return false;
        name = alias->second;
    }
    std::vector<uint8_t> bytes;
    return r.read(name, bytes) && webp::decode(bytes.data(), bytes.size(), out.argb, out.width, out.height);
}

std::vector<uint32_t> fitted(const Image& in, int side) {
    std::vector<uint32_t> out(size_t(side) * size_t(side));
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x) out[size_t(y * side + x)] = ((x / 4 + y / 4) & 1) ? 0xFF5A5E66u : 0xFF464A52u;
    if (in.empty() || in.width <= 0 || in.height <= 0) return out;
    const float k = float(side) / float(std::max(in.width, in.height));
    const int w = std::max(1, int(float(in.width) * k)), h = std::max(1, int(float(in.height) * k));
    const int x0 = (side - w) / 2, y0 = (side - h) / 2;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const uint32_t v = in.argb[size_t(std::min(in.height - 1, y * in.height / h)) * size_t(in.width) +
                                       size_t(std::min(in.width - 1, x * in.width / w))];
            if ((v >> 24) >= 128) out[size_t((y0 + y) * side + x0 + x)] = v | 0xFF000000u;
        }
    return out;
}

}  // namespace uagfx::studio
