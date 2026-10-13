#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "core/twin_text.hpp"
#include "win/context.hpp"
#include "win/crisp/state.hpp"
#include "win/log.hpp"

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

// The text the twin's font is fitted with (0.5.1): the letters of uaRO's chat and windows - Latin, Cyrillic, digits.
const wchar_t kSample[] =
    L"The quick brown fox jumps over the lazy dog 0123456789 "
    L"\u0421\u044a\u0435\u0448\u044c \u0436\u0435 \u0435\u0449\u0451 \u044d\u0442\u0438\u0445 "
    L"\u043c\u044f\u0433\u043a\u0438\u0445 \u0444\u0440\u0430\u043d\u0446\u0443\u0437\u0441\u043a\u0438\u0445 "
    L"\u0431\u0443\u043b\u043e\u043a, \u0434\u0430 \u0432\u044b\u043f\u0435\u0439 \u0447\u0430\u044e";
constexpr int kSampleLength = int(std::size(kSample)) - 1;

HDC glyphDc() {
    if (!g_glyphDc) g_glyphDc = CreateCompatibleDC(nullptr);
    return g_glyphDc;
}

// The sample's width in lf at height h; 0 when GDI cannot tell.
int sampleWidth(LOGFONTW lf, LONG height) {
    lf.lfHeight = height;
    const HFONT font = CreateFontIndirectW(&lf);
    const HDC dc = glyphDc();
    if (!font || !dc) {
        if (font) DeleteObject(font);
        return 0;
    }
    const HGDIOBJ old = SelectObject(dc, font);
    SIZE size{};
    const BOOL ok = GetTextExtentPoint32W(dc, kSample, kSampleLength, &size);
    SelectObject(dc, old);
    DeleteObject(font);
    return ok ? int(size.cx) : 0;
}

// The game's font as the twin draws it (spec §4, 0.5.1): K times larger, or as much smaller as it takes for its
// sample to be no wider than the game's times K - the game's hinted small letters are narrower than K times larger
// ones, and the game places every letter by them. Measured once per font, with the game's DC, which holds the game's
// font. Null when GDI cannot make it.
const BigFont* bigFont(HDC dc, const LOGFONTW& lf, double K) {
    const FontKey key{lf.lfFaceName, lf.lfHeight, lf.lfWidth, lf.lfWeight, lf.lfItalic, lf.lfCharSet};
    auto it = g_fonts.find(key);
    if (it == g_fonts.end()) {
        LOGFONTW big = lf;
        big.lfWidth = LONG(std::lround(lf.lfWidth * K));
        big.lfQuality = ANTIALIASED_QUALITY;
        const LONG sign = lf.lfHeight < 0 ? -1 : 1;
        const int start = int(std::lround(std::labs(lf.lfHeight) * K));
        SIZE game{};
        const double target = GetTextExtentPoint32W(dc, kSample, kSampleLength, &game) ? game.cx * K : 0.0;
        const int height = fitFontHeight(start, int(std::lround(start * 0.8)), target,
                                         [&big, sign](int h) { return sampleWidth(big, sign * h); });
        big.lfHeight = sign * height;
        big.lfWidth = start > 0 ? LONG(std::lround(lf.lfWidth * K * height / start)) : 0;
        // The entry first: should it throw (no memory), no font is made and left behind (0.5.2 review 2c).
        it = g_fonts.emplace(key, BigFont{}).first;
        it->second = BigFont{CreateFontIndirectW(&big), g_nextFont++};
        LOG_INFO("crisp text: the game's font of height %ld drawn at %d (k %.3f: sample %d px, the game's x k %.0f)",
                 static_cast<long>(lf.lfHeight), static_cast<int>(sign * height), K, sampleWidth(big, big.lfHeight),
                 target);
    }
    return it->second.font ? &it->second : nullptr;
}

// A letter K times larger: from the cache, else from GDI; null when GDI cannot draw it (not a TrueType font).
const GlyphBitmap* glyph(const BigFont& f, wchar_t ch) {
    State& s = state();
    if (const GlyphBitmap* g = s.glyphs.find(f.id, ch)) return g;
    if (!glyphDc() || !SelectObject(g_glyphDc, f.font)) return nullptr;
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

// TextOutW on a picture's DC (spec §4): the twin's part around the game's call, which call() makes.
template <typename Call>
void textOut(Picture& pic, HDC dc, int x, int y, LPCWSTR text, int count, Call&& call) {
    State& s = state();
    LOGFONTW lf{};
    TEXTMETRICW tm{};
    SIZE extent{};
    std::vector<int> ends(static_cast<size_t>(count));  // the width of the first i + 1 letters in the game's font
    const BigFont* font = nullptr;
    if (GetTextAlign(dc) == 0 &&  // TA_TOP | TA_LEFT | TA_NOUPDATECP: the game never changes it
        GetObjectW(GetCurrentObject(dc, OBJ_FONT), sizeof lf, &lf) == int(sizeof lf) && lf.lfEscapement == 0 &&
        GetTextMetricsW(dc, &tm) && GetTextExtentExPointW(dc, text, count, 0, nullptr, ends.data(), &extent))
        font = bigFont(dc, lf, s.K);
    bool letters = font != nullptr;
    for (int i = 0; letters && i < count; ++i) letters = glyph(*font, text[i]) != nullptr;  // every one, or none
    if (!letters) {  // like any other write: the small letters enlarged
        call();
        GdiFlush();
        pic.twin->sync(pic.bits, all(pic));
        return;
    }
    const Rect r = textRect(x, y, extent.cx, tm.tmHeight, tm.tmHeight);
    pic.twin->sync(pic.bits, r);  // what the twin missed there so far
    call();
    GdiFlush();
    const uint32_t rgb = textColor(GetTextColor(dc));
    const int baseline = baselineRow(y, tm.tmAscent, s.K);
    for (int i = 0; i < count; ++i)
        if (const GlyphBitmap* g = glyph(*font, text[i]))
            drawGlyph(*pic.twin, *g, letterPen(x, i > 0 ? ends[size_t(i) - 1] : 0, s.K), baseline, rgb);
    pic.twin->absorb(pic.bits, r);  // the small letters stay out of the twin
}

// FillRgn on a picture's DC: every pixel of the region takes its new value in the twin.
void fillRegion(Picture& pic, HRGN region) {
    const DWORD size = GetRegionData(region, 0, nullptr);
    std::vector<uint8_t> data(size);
    if (size < sizeof(RGNDATAHEADER) || GetRegionData(region, size, reinterpret_cast<RGNDATA*>(data.data())) != size) {
        pic.twin->sync(pic.bits, all(pic));
        return;
    }
    const RGNDATA* d = reinterpret_cast<const RGNDATA*>(data.data());
    const RECT* rects = reinterpret_cast<const RECT*>(d->Buffer);
    const DWORD fit = DWORD((size - sizeof(RGNDATAHEADER)) / sizeof(RECT));
    for (DWORD i = 0; i < std::min(d->rdh.nCount, fit); ++i) {
        const RECT& b = rects[i];
        pic.twin->overwrite(pic.bits, Rect{int(b.left), int(b.top), int(b.right), int(b.bottom)});
    }
}

}  // namespace

// The game's TextOutW on a window picture's DC (spec §4): the same letters K times larger, each where the game put
// its small one.
BOOL WINAPI hookTextOutW(HDC dc, int x, int y, LPCWSTR text, int count) {
    Guard guard;
    const auto real = reinterpret_cast<TextOutWFn>(state().textOutW);
    BOOL ok = FALSE;
    bool called = false;
    const auto call = [&] {
        ok = real(dc, x, y, text, count);
        called = true;
    };
    Picture* pic = nullptr;
    guarded(  // the driver's part; the game's call is made once, whatever fails (0.5.0 review M3)
        [&] {
            pic = text && count > 0 ? pictureOf(dc) : nullptr;
            if (pic) textOut(*pic, dc, x, y, text, count, call);
        },
        [&] {
            if (pic) lost(pic->self);
        });
    if (!called) call();
    return ok;
}

// The game's FillRgn (DrawTriangle) on a window picture's DC: every pixel of the region takes its new value in the
// twin (spec §4).
BOOL WINAPI hookFillRgn(HDC dc, HRGN region, HBRUSH brush) {
    Guard guard;
    const BOOL ok = reinterpret_cast<FillRgnFn>(state().fillRgn)(dc, region, brush);
    Picture* pic = nullptr;
    guarded(
        [&] {
            pic = pictureOf(dc);  // after its GdiFlush the fill is in the pixels
            if (pic) fillRegion(*pic, region);
        },
        [&] {
            if (pic) lost(pic->self);
        });
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
