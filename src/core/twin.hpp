#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/image.hpp"
#include "core/twin_grid.hpp"

namespace uagfx {

// Stage 6 (spec §2.5, §3): a pixel of a game window picture in its twin. The game uploads its pictures with alpha
// forced to 0xFF and makes every pixel whose 5-bit colour is magenta a transparent texel; the twin keeps the 8-bit
// colour, opaque, or transparent - premultiplied 0xAARRGGBB, as every twin pixel.
bool transparentAtUpload(uint32_t pixel);
uint32_t twinValue(uint32_t pixel);
// Premultiplied src over dst.
uint32_t over(uint32_t src, uint32_t dst);

// The game's CopyFrom clipping (docs/research/text.md §1.1, slot 5): x or y at or past the picture - negative ones
// too, the game compares them unsigned - copies nothing; otherwise the copy is cut at the right and bottom edges.
// false: nothing is copied.
bool copyFromRegion(int x, int y, int w, int h, int width, int height, Rect& out);

// The game's CopyPixels (slot 4, docs/research/text.md §1.1) at (x, y), h rows high: how many rows it writes. It
// stops at the first row whose first destination pixel is 0x00000000. dib: the picture before the call.
int copyPixelsRows(const uint32_t* dib, int width, int height, int x, int y, int h);

// A game window picture (CDCBitmap) as the driver keeps it (spec §4): its twin at K and a copy of the game's pixels
// the twin shows. dib: the game's pixels, width x height, row by row.
class TwinPicture {
public:
    TwinPicture(const uint32_t* dib, int width, int height, double K);  // the twin enlarges dib, the copy is dib

    int width() const { return w_; }
    int height() const { return h_; }
    double scale() const { return k_; }
    int twinWidth() const { return ex_.back(); }
    int twinHeight() const { return ey_.back(); }
    std::vector<uint32_t>& twin() { return twin_; }
    const std::vector<uint32_t>& twin() const { return twin_; }
    size_t bytes() const { return (twin_.size() + copy_.size()) * sizeof(uint32_t); }
    // What a picture of width x height takes at K, before it is made.
    static size_t bytesFor(int width, int height, double K);

    // The game's Clear or FillRgn wrote r: every pixel's block takes its new value, whatever the twin had there.
    void overwrite(const uint32_t* dib, Rect r);
    // The pixels of r whose colour differs from the copy go to the twin as blocks and the copy follows; the alpha
    // byte is ignored (the game sets it before every upload). Returns how many.
    int sync(const uint32_t* dib, Rect r);
    // The game drew text into r, which the twin got at its own size: the copy takes the new pixels, the twin stays.
    void absorb(const uint32_t* dib, Rect r);
    // The game's CopyPixels wrote `rows` rows of the w-wide source at (x, y): every pixel but the ones its key skips
    // (exactly 0xFFFF00FF), at flat index (y + r) * width + x + c - past the right edge into the next row, as the
    // game does. Their blocks take the new values, whatever the twin had there (review 0.5.0 I1).
    void copyPixels(const uint32_t* dib, int x, int y, int w, int rows, const uint32_t* src, bool key);
    // CopyFrom (spec §3, §4): child's twin over this twin on the game pixels [x, x + w) x [y, y + h), x and y not
    // negative; then the copy takes the new pixels there. A child made at another scale goes in through sync.
    void copyFrom(const uint32_t* dib, const TwinPicture& child, int x, int y, int w, int h);
    // 0.14, AI icons: a picture of w x h straight ARGB pixels (the icon at r, made larger by the AI textures) into the
    // twin pixels of the game rectangle r, inside the picture: each twin pixel takes the average of the picture's
    // pixels it covers, alpha included - over what the twin has there, or (over false, 0.14.2) in its place, as the
    // game's CopyPixels without its key writes every pixel and its magenta ones become holes in the window. The copy
    // stays: absorb() takes the game's pixels.
    void paint(Rect r, const uint32_t* argb, int w, int h, bool over = true);
    // 0.15: what a call drew into the picture since the copy last took its pixels (a sprite in the equipment window):
    // the box of the game pixels in r that differ from the copy (the alpha byte aside), and its picture - those
    // pixels opaque (the window's magenta transparent), the rest transparent. false: nothing differs.
    bool changedPicture(const uint32_t* dib, Rect r, Rect& box, std::vector<uint32_t>& argb) const;
    // The twin pixels of r - twin coordinates, inside the twin - row by row: what a tile's texture loads.
    void texels(const Rect& r, std::vector<uint32_t>& out) const;
    // The twin pixels changed since the last call (a new twin: all of it); empty when none.
    Rect takeChanged();
    // drawGlyph and other direct writers of twin() say which twin pixels they changed.
    void touch(Rect twinPixels);

private:
    void fillRow(const uint32_t* dib, int y, int x0, int x1);
    bool clip(Rect& r) const { return clipRect(Rect{0, 0, w_, h_}, r); }

    int w_;
    int h_;
    double k_;
    std::vector<int> ex_;         // twinEdge(g, K) for g = 0..width
    std::vector<int> ey_;         // for g = 0..height
    std::vector<uint32_t> twin_;  // twinWidth x twinHeight
    std::vector<uint32_t> copy_;  // width x height
    Rect changed_;                // twin pixels
};

}  // namespace uagfx
