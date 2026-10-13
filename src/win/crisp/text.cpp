#include <windows.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "core/twin_text.hpp"
#include "win/context.hpp"
#include "win/crisp/state.hpp"

namespace uagfx::win::crisp {
namespace {

// A game font, by its LOGFONT: the twin needs it K times larger.
struct FontKey {
    std::wstring face;
    LONG height;
    LONG width;
    LONG weight;
    BYTE italic;
    BYTE charset;
    bool operator<(const FontKey& o) const {
        return std::tie(face, height, width, weight, italic, charset) <
               std::tie(o.face, o.height, o.width, o.weight, o.italic, o.charset);
    }
};

struct BigFont {
    HFONT font = nullptr;
    uint32_t id = 0;  // the glyph cache's number for it
};

std::map<FontKey, BigFont> g_fonts;
HDC g_glyphDc = nullptr;  // GetGlyphOutlineW reads the big fonts here
uint32_t g_nextFont = 1;

// The game's font K times larger (spec §4); null when GDI cannot make it.
const BigFont* bigFont(const LOGFONTW& lf, double K) {
    const FontKey key{lf.lfFaceName, lf.lfHeight, lf.lfWidth, lf.lfWeight, lf.lfItalic, lf.lfCharSet};
    auto it = g_fonts.find(key);
    if (it == g_fonts.end()) {
        LOGFONTW big = lf;
        big.lfHeight = LONG(std::lround(lf.lfHeight * K));
        big.lfWidth = LONG(std::lround(lf.lfWidth * K));
        big.lfQuality = ANTIALIASED_QUALITY;
        it = g_fonts.emplace(key, BigFont{CreateFontIndirectW(&big), g_nextFont++}).first;
    }
    return it->second.font ? &it->second : nullptr;
}

// A letter K times larger: from the cache, else from GDI; null when GDI cannot draw it (not a TrueType font).
const GlyphBitmap* glyph(const BigFont& f, wchar_t ch) {
    State& s = state();
    if (const GlyphBitmap* g = s.glyphs.find(f.id, ch)) return g;
    if (!g_glyphDc) g_glyphDc = CreateCompatibleDC(nullptr);
    if (!g_glyphDc || !SelectObject(g_glyphDc, f.font)) return nullptr;
    const MAT2 identity{{0, 1}, {0, 0}, {0, 0}, {0, 1}};
    GLYPHMETRICS gm{};
    const DWORD size = GetGlyphOutlineW(g_glyphDc, ch, GGO_GRAY8_BITMAP, &gm, 0, nullptr, &identity);
    if (size == GDI_ERROR) return nullptr;
    GlyphBitmap g;
    g.originX = gm.gmptGlyphOrigin.x;
    g.originY = gm.gmptGlyphOrigin.y;
    if (size > 0) {  // a space has no pixels
        std::vector<uint8_t> raw(size);
        if (GetGlyphOutlineW(g_glyphDc, ch, GGO_GRAY8_BITMAP, &gm, size, raw.data(), &identity) == GDI_ERROR)
            return nullptr;
        g.width = int(gm.gmBlackBoxX);
        g.height = int(gm.gmBlackBoxY);
        const size_t pitch = (size_t(g.width) + 3) & ~size_t(3);  // rows are DWORD-aligned
        if (pitch * size_t(g.height) > raw.size()) return nullptr;
        g.coverage.resize(size_t(g.width) * size_t(g.height));
        for (int y = 0; y < g.height; ++y)
            std::copy_n(raw.data() + size_t(y) * pitch, size_t(g.width),
                        g.coverage.data() + size_t(y) * size_t(g.width));
    }
    return &s.glyphs.put(f.id, ch, std::move(g));
}

// The picture whose DC this is, while crisp text is on.
Picture* pictureOf(HDC dc) {
    if (!active()) return nullptr;
    State& s = state();
    const auto it = s.dcs.find(dc);
    if (it == s.dcs.end() || field<HDC>(it->second, crisp_exe::kDc) != dc) return nullptr;
    return picture(it->second);
}

Rect all(const Picture& p) { return Rect{0, 0, p.twin->width(), p.twin->height()}; }

using TextOutWFn = BOOL(WINAPI*)(HDC, int, int, LPCWSTR, int);
using FillRgnFn = BOOL(WINAPI*)(HDC, HRGN, HBRUSH);

}  // namespace

// The game's TextOutW on a window picture's DC (spec §4): the same letters K times larger, each where the game put
// its small one.
BOOL WINAPI hookTextOutW(HDC dc, int x, int y, LPCWSTR text, int count) {
    Guard guard;
    State& s = state();
    const auto real = reinterpret_cast<TextOutWFn>(s.textOutW);
    Picture* pic = text && count > 0 ? pictureOf(dc) : nullptr;
    if (!pic) return real(dc, x, y, text, count);
    LOGFONTW lf{};
    TEXTMETRICW tm{};
    SIZE extent{};
    std::vector<int> ends(static_cast<size_t>(count));  // the width of the first i + 1 letters in the game's font
    const BigFont* font = nullptr;
    if (GetTextAlign(dc) == 0 &&  // TA_TOP | TA_LEFT | TA_NOUPDATECP: the game never changes it
        GetObjectW(GetCurrentObject(dc, OBJ_FONT), sizeof lf, &lf) == int(sizeof lf) && lf.lfEscapement == 0 &&
        GetTextMetricsW(dc, &tm) && GetTextExtentExPointW(dc, text, count, 0, nullptr, ends.data(), &extent))
        font = bigFont(lf, s.K);
    bool letters = font != nullptr;
    for (int i = 0; letters && i < count; ++i) letters = glyph(*font, text[i]) != nullptr;  // every one, or none
    if (!letters) {  // like any other write: the small letters enlarged
        const BOOL ok = real(dc, x, y, text, count);
        GdiFlush();
        pic->twin->sync(pic->bits, all(*pic));
        return ok;
    }
    const Rect r = textRect(x, y, extent.cx, tm.tmHeight, tm.tmHeight);
    pic->twin->sync(pic->bits, r);  // what the twin missed there so far
    const BOOL ok = real(dc, x, y, text, count);
    GdiFlush();
    const uint32_t rgb = textColor(GetTextColor(dc));
    const int baseline = baselineRow(y, tm.tmAscent, s.K);
    for (int i = 0; i < count; ++i)
        if (const GlyphBitmap* g = glyph(*font, text[i]))
            drawGlyph(*pic->twin, *g, letterPen(x, i > 0 ? ends[size_t(i) - 1] : 0, s.K), baseline, rgb);
    pic->twin->absorb(pic->bits, r);  // the small letters stay out of the twin
    return ok;
}

// The game's FillRgn (DrawTriangle) on a window picture's DC: every pixel of the region takes its new value in the
// twin (spec §4).
BOOL WINAPI hookFillRgn(HDC dc, HRGN region, HBRUSH brush) {
    Guard guard;
    const BOOL ok = reinterpret_cast<FillRgnFn>(state().fillRgn)(dc, region, brush);
    Picture* pic = pictureOf(dc);  // after its GdiFlush the fill is in the pixels
    if (!pic) return ok;
    const DWORD size = GetRegionData(region, 0, nullptr);
    std::vector<uint8_t> data(size);
    if (size < sizeof(RGNDATAHEADER) || GetRegionData(region, size, reinterpret_cast<RGNDATA*>(data.data())) != size) {
        pic->twin->sync(pic->bits, all(*pic));
        return ok;
    }
    const RGNDATA* d = reinterpret_cast<const RGNDATA*>(data.data());
    const RECT* rects = reinterpret_cast<const RECT*>(d->Buffer);
    const DWORD fit = DWORD((size - sizeof(RGNDATAHEADER)) / sizeof(RECT));
    for (DWORD i = 0; i < std::min(d->rdh.nCount, fit); ++i) {
        const RECT& b = rects[i];
        pic->twin->overwrite(pic->bits, Rect{int(b.left), int(b.top), int(b.right), int(b.bottom)});
    }
    return ok;
}

void dropFonts() {
    if (g_glyphDc) DeleteDC(g_glyphDc);  // first: a font selected into a DC is not deleted
    g_glyphDc = nullptr;
    for (const auto& f : g_fonts)
        if (f.second.font) DeleteObject(f.second.font);
    g_fonts.clear();
}

}  // namespace uagfx::win::crisp
