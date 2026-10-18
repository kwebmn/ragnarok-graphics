#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/pack_build.hpp"
#include "core/pack_source.hpp"
#include "core/texture_catalog.hpp"
#include "core/texture_pack.hpp"

namespace uagfx::studio {

class CatalogWork;

// 2.3: a picture of the studio's - the original as the game uploads it, or the pack's (any size).
struct Image {
    int width = 0, height = 0;
    std::vector<uint32_t> argb;
    bool empty() const { return argb.empty(); }
};

// The game's names (CP949 bytes) for the window (UTF-16) and the studio's own texts (UTF-8).
std::wstring wideOfGame(const std::string& cp949);
std::string utf8OfGame(const std::string& cp949);

pack::Choice choiceOf(const catalog::Catalog& c);  // the choice the catalog's keys were made with
// The original of a catalog picture: its file read again and decoded (in its colour's palette), the picture of its
// key; false when the file changed since the catalog (no picture of that key in it).
bool originalOf(CatalogWork& files, const catalog::Catalog& c, const catalog::FileEntry& f, const catalog::PictureOf& p,
                Image& out);
// The pack's picture of a key (its group's name), or of the picture its alias names; false: none.
bool packPictureOf(pack::Reader& r, const catalog::PictureOf& p, const pack::Index& index, Image& out);
// side x side: the picture fitted (nearest, its proportions kept) on a grey checker where it is transparent.
std::vector<uint32_t> fitted(const Image& in, int side);

}  // namespace uagfx::studio
