#include "win/picture_draw.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/d3d7.hpp"
#include "core/effect_exe.hpp"
#include "core/glint.hpp"
#include "core/states.hpp"
#include "win/code_page.hpp"
#include "win/gpu/renderer.hpp"
#include "win/ui_cost.hpp"

namespace uagfx::win {

namespace {

// 3.0.11-beta.57 (R-T942DZ: at 4K the minimap's picture took 15.6 ms each step, the character block's 13-19 ms -
// their shapes 2-3 of it): the rest was the words by GDI - the screen's DC, a DIB of the whole picture and every word
// drawn again each time. Now each word's coverage is kept by what makes it (its text, size and weight, how it sits in
// its box, the box's size): GDI draws it once, in a DIB kept for that, and the pictures after take it as it is
struct Coverage {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // its lit part in its box (x1 <= x0: none)
    std::vector<uint8_t> cover;          // the lit part's pixels, each its brightest channel (white on black)
    size_t bytes = 0;                    // what keeping it takes
    unsigned long long used = 0;
};

struct WordsGdi {
    HDC dc = nullptr;
    HBITMAP dib = nullptr;
    uint32_t* bits = nullptr;
    int w = 0, h = 0;
    std::vector<std::pair<int, HFONT>> fonts;  // the height, negative for a bold one
    std::unordered_map<std::wstring, Coverage> kept;
    size_t keptBytes = 0;
    unsigned long long clock = 0;  // a drawWords call each
};

WordsGdi& wordsGdi() {
    thread_local WordsGdi g;
    return g;
}

constexpr size_t kKeptBytes = size_t(4) << 20;  // past that the words unused longest go, down to 3/4 of it

// 3.0.11-beta.25 (R-77TCM7): a font for each size and weight, not for each of the words; 3.0.11-beta.54 (R-PVZH5M):
// kept from one picture to the next
HFONT fontOf(WordsGdi& g, int height, bool bold) {
    const int key = bold ? -height : height;
    for (const auto& [k, f] : g.fonts)
        if (k == key) return f;
    if (g.fonts.size() >= 32) {  // a few sizes in use at a time: past that all go (the stock font in the DC first)
        SelectObject(g.dc, GetStockObject(SYSTEM_FONT));
        for (const auto& f : g.fonts) DeleteObject(f.second);
        g.fonts.clear();
    }
    HFONT font = CreateFontW(-height, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    if (font) g.fonts.emplace_back(key, font);
    return font;
}

// The DC and its DIB, w x h at least: made bigger only when a word needs more
bool room(WordsGdi& g, int w, int h) {
    if (!g.dc) {
        g.dc = CreateCompatibleDC(nullptr);
        if (!g.dc) return false;
        SetBkMode(g.dc, TRANSPARENT);
        SetTextColor(g.dc, RGB(255, 255, 255));
    }
    if (g.bits && w <= g.w && h <= g.h) return true;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = std::max(w, g.w);
    bi.bmiHeader.biHeight = -std::max(h, g.h);
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(g.dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits) {
        if (dib) DeleteObject(dib);
        return false;
    }
    SelectObject(g.dc, dib);  // the one before out of the DC: it can go
    if (g.dib) DeleteObject(g.dib);
    g.dib = dib;
    g.bits = static_cast<uint32_t*>(bits);
    g.w = std::max(w, g.w);
    g.h = std::max(h, g.h);
    return true;
}

// A word drawn by GDI in its box (w x h, at the DIB's corner): its coverage, cut to its lit part
bool drawnAnew(WordsGdi& g, const Words& word, int height, int w, int h, UINT format, Coverage& out) {
    if (!room(g, w, h)) return false;
    const HFONT font = fontOf(g, height, word.bold);
    if (!font) return false;
    for (int y = 0; y < h; ++y) std::memset(g.bits + size_t(y) * size_t(g.w), 0, size_t(w) * 4);
    SelectObject(g.dc, font);
    RECT r{0, 0, w, h};
    DrawTextW(g.dc, word.text.c_str(), int(word.text.size()), &r, format);
    GdiFlush();
    int x0 = w, y0 = h, x1 = 0, y1 = 0;
    for (int y = 0; y < h; ++y) {
        const uint32_t* row = g.bits + size_t(y) * size_t(g.w);
        for (int x = 0; x < w; ++x)
            if (row[x] & 0xFFFFFF) {
                x0 = std::min(x0, x);
                x1 = std::max(x1, x + 1);
                y0 = std::min(y0, y);
                y1 = std::max(y1, y + 1);
            }
    }
    out = Coverage{};
    if (x1 > x0) {
        out.x0 = x0;
        out.y0 = y0;
        out.x1 = x1;
        out.y1 = y1;
        out.cover.resize(size_t(x1 - x0) * size_t(y1 - y0));
        uint8_t* to = out.cover.data();
        for (int y = y0; y < y1; ++y) {
            const uint32_t* row = g.bits + size_t(y) * size_t(g.w);
            for (int x = x0; x < x1; ++x) {
                const uint32_t c = row[x];
                *to++ = uint8_t(std::max({c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF}));
            }
        }
    }
    return true;
}

// What makes a word's coverage: its text, then a fixed tail - its size and weight, its format, its box's size
std::wstring keyOf(const Words& word, int height, UINT format, int w, int h) {
    std::wstring key = word.text;
    key.push_back(L'\x1F');
    for (const unsigned v : {unsigned(height) | (word.bold ? 0x8000u : 0u), unsigned(format & 0xFFFF),
                             unsigned(format >> 16), unsigned(w), unsigned(h)})
        key.push_back(wchar_t(v & 0xFFFF));
    return key;
}

void forgetOld(WordsGdi& g) {
    std::vector<std::pair<unsigned long long, const std::wstring*>> ages;
    ages.reserve(g.kept.size());
    for (const auto& [key, c] : g.kept) ages.emplace_back(c.used, &key);
    std::sort(ages.begin(), ages.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& [used, key] : ages) {
        if (g.keptBytes <= kKeptBytes / 4 * 3 || used == g.clock) break;  // never this picture's own
        const auto it = g.kept.find(*key);
        g.keptBytes -= it->second.bytes;
        g.kept.erase(it);
    }
}

}  // namespace

void drawWords(panel::Picture& pic, const std::vector<Words>& words) {
    drawWords(pic, words, Rect{0, 0, pic.width, pic.height});
}

void drawWords(panel::Picture& pic, const std::vector<Words>& words, const Rect& area) {
    const Rect a{std::max(0, area.left), std::max(0, area.top), std::min(pic.width, area.right),
                 std::min(pic.height, area.bottom)};
    if (a.empty() || pic.px.size() != size_t(pic.width) * size_t(pic.height)) return;
    uicost::Words cost;  // 3.0.11-beta.57: the words' share of a part's painting
    WordsGdi& g = wordsGdi();
    ++g.clock;
    int fresh = 0, again = 0;
    for (const Words& word : words) {
        if (word.text.empty() || word.box.w <= 0) continue;
        const panel::Box& b = word.box;
        const int left = int(std::lround(b.x)), top = int(std::lround(b.y));
        const int w = int(std::lround(b.x + b.w)) - left, h = int(std::lround(b.y + b.h)) - top;
        if (w <= 0 || h <= 0 || left + w <= a.left || top + h <= a.top || left >= a.right || top >= a.bottom)
            continue;  // nothing to show, or not over the area
        const int height = std::max(8, int(std::lround(word.px)));
        const UINT format = (word.wrap ? DT_WORDBREAK | DT_TOP : DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS) |
                            DT_NOPREFIX | word.align;
        std::wstring key = keyOf(word, height, format, w, h);
        auto it = g.kept.find(key);
        if (it == g.kept.end()) {
            Coverage c;
            if (!drawnAnew(g, word, height, w, h, format, c)) continue;
            c.bytes = c.cover.size() + key.size() * sizeof(wchar_t) + 64;
            g.keptBytes += c.bytes;
            it = g.kept.emplace(std::move(key), std::move(c)).first;
            ++fresh;
        } else {
            ++again;
        }
        Coverage& c = it->second;
        c.used = g.clock;
        const int lit = c.x1 - c.x0;  // each coverage over the picture in the word's colour, inside the area
        for (int y = std::max(a.top, top + c.y0); y < std::min(a.bottom, top + c.y1); ++y) {
            const uint8_t* from = c.cover.data() + size_t(y - top - c.y0) * size_t(lit);
            uint32_t* row = pic.px.data() + size_t(y) * size_t(pic.width);
            for (int x = std::max(a.left, left + c.x0); x < std::min(a.right, left + c.x1); ++x)
                if (const uint8_t v = from[x - left - c.x0]) panel::over(row[x], word.rgb, float(v) / 255.0f);
        }
    }
    if (g.keptBytes > kKeptBytes) forgetOld(g);
    cost.counted(fresh, again, g.keptBytes);
}

std::wstring utf8Wide(const std::string& s) { return widened(s, CP_UTF8); }

namespace {

// A picture's quad at (x, y, w, h) of the target: its texture made for its size and uploaded when it changed;
// `smooth` - sampled between its texels (stretched), else a texel a target pixel
void drawQuad(FrameRecord& record, gpu::Renderer& renderer, const panel::Picture& pic, uint32_t& texture, int& texW,
              int& texH, bool& upload, int x, int y, int w, int h, bool smooth, float k, float opacity,
              int stretch = 1) {
    if (pic.width <= 0 || pic.height <= 0 || opacity <= 0 || w <= 0 || h <= 0) return;
    if (!texture || texW != pic.width || texH != pic.height) {
        if (texture) renderer.releaseTexture(texture);
        texture = renderer.createTexture(pic.width, pic.height, 1);
        texW = pic.width;
        texH = pic.height;
        upload = true;
        if (!texture) return;
    }
    if (upload) {
        record.upload(texture, 0, Rect{0, 0, texW, texH}, pic.px.data());
        upload = false;
    }
    // Premultiplied, no depth
    DrawSetup s;
    s.ui = true;
    s.blend = true;
    s.srcBlend = d3d7::BLEND_ONE;
    s.dstBlend = d3d7::BLEND_INVSRCALPHA;
    const bool faint = opacity < 1;  // the texture times the vertices' colour: premultiplied, all four channels
    s.stage[0].colorOp = s.stage[0].alphaOp = faint ? d3d7::TOP_MODULATE : d3d7::TOP_SELECTARG1;
    s.stage[0].colorArg1 = s.stage[0].alphaArg1 = d3d7::TA_TEXTURE;
    s.stage[0].colorArg2 = s.stage[0].alphaArg2 = d3d7::TA_DIFFUSE;
    s.stage[0].addressU = s.stage[0].addressV = d3d7::TADDRESS_CLAMP;
    s.stage[0].magFilter = smooth ? d3d7::TFG_LINEAR : d3d7::TFG_POINT;
    s.stage[0].minFilter = smooth ? d3d7::TFN_LINEAR : d3d7::TFN_POINT;
    s.stage[1].colorOp = s.stage[1].alphaOp = d3d7::TOP_DISABLE;
    DrawCmd cmd;
    cmd.state = record.addState(s);
    cmd.texture[0] = texture;
    cmd.effect = uint8_t(effect_exe::Kind::Panel);  // sampled as it is (draw.cpp, renderer11.cpp)
    cmd.primitive = d3d7::PT_TRIANGLELIST;
    cmd.fvf = d3d7::FVF_TL1;
    // a game x lands at x * k + 0.5 in the target (shaders.cpp draw_vs): the quad's edges on the target's pixel edges -
    // 3.0.11-beta.54: (x, y, w, h) and k a part's, its target pixel edges at x * stretch
    const float half = 0.5f / float(stretch);
    const float x0 = (float(x) - half) / k, y0 = (float(y) - half) / k;
    const float x1 = (float(x + w) - half) / k, y1 = (float(y + h) - half) / k;
    const uint32_t a = faint ? uint32_t(std::lround(std::clamp(opacity, 0.0f, 1.0f) * 255)) : 255u;
    const uint32_t white = a << 24 | a << 16 | a << 8 | a;
    const glint::Vertex v[6] = {{x0, y0, 0, 1, white, 0, 0, 0}, {x1, y0, 0, 1, white, 0, 1, 0},
                                {x0, y1, 0, 1, white, 0, 0, 1}, {x1, y0, 0, 1, white, 0, 1, 0},
                                {x1, y1, 0, 1, white, 0, 1, 1}, {x0, y1, 0, 1, white, 0, 0, 1}};
    record.draw(cmd, v, 6, nullptr, 0);
}

}  // namespace

void drawPicture(FrameRecord& record, gpu::Renderer& renderer, const panel::Picture& pic, uint32_t& texture, int& texW,
                 int& texH, bool& upload, int x, int y, float k, float opacity) {
    drawQuad(record, renderer, pic, texture, texW, texH, upload, x, y, pic.width, pic.height, false, k, opacity);
}

void drawStretched(FrameRecord& record, gpu::Renderer& renderer, const panel::Picture& pic, uint32_t& texture,
                   int& texW, int& texH, bool& upload, int x, int y, int w, int h, float k, float opacity) {
    drawQuad(record, renderer, pic, texture, texW, texH, upload, x, y, w, h, true, k, opacity);
}

namespace {
int g_partStretch = 1;  // 3.0.11-beta.54: the game's thread's
}  // namespace

void setPartStretch(int stretch) { g_partStretch = std::clamp(stretch, 1, 4); }
int partStretch() { return g_partStretch; }

void drawPart(FrameRecord& record, gpu::Renderer& renderer, const panel::Picture& pic, uint32_t& texture, int& texW,
              int& texH, bool& upload, int x, int y, float k, float opacity) {
    drawQuad(record, renderer, pic, texture, texW, texH, upload, x, y, pic.width, pic.height, false, k, opacity,
             g_partStretch);
}

void drawPartStretched(FrameRecord& record, gpu::Renderer& renderer, const panel::Picture& pic, uint32_t& texture,
                       int& texW, int& texH, bool& upload, int x, int y, int w, int h, float k, float opacity) {
    drawQuad(record, renderer, pic, texture, texW, texH, upload, x, y, w, h, true, k, opacity, g_partStretch);
}

void drawShade(FrameRecord& record, int x, int y, int w, int h, float k, float alpha) {
    if (w <= 0 || h <= 0 || alpha <= 0 || k <= 0) return;
    const float half = 0.5f / float(g_partStretch);  // 3.0.11-beta.54: a part's - in its pixels
    DrawSetup s;  // the vertices' colour alone, premultiplied
    s.ui = true;
    s.blend = true;
    s.srcBlend = d3d7::BLEND_ONE;
    s.dstBlend = d3d7::BLEND_INVSRCALPHA;
    s.stage[0].colorOp = s.stage[0].alphaOp = d3d7::TOP_SELECTARG1;
    s.stage[0].colorArg1 = s.stage[0].alphaArg1 = d3d7::TA_DIFFUSE;
    s.stage[1].colorOp = s.stage[1].alphaOp = d3d7::TOP_DISABLE;
    DrawCmd cmd;
    cmd.state = record.addState(s);
    cmd.effect = uint8_t(effect_exe::Kind::Panel);  // at the target's pixels, as the pictures
    cmd.primitive = d3d7::PT_TRIANGLELIST;
    cmd.fvf = d3d7::FVF_TL1;
    const float x0 = (float(x) - half) / k, y0 = (float(y) - half) / k;
    const float x1 = (float(x + w) - half) / k, y1 = (float(y + h) - half) / k;
    const uint32_t c = uint32_t(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255)) << 24;
    const glint::Vertex v[6] = {{x0, y0, 0, 1, c, 0, 0, 0}, {x1, y0, 0, 1, c, 0, 1, 0}, {x0, y1, 0, 1, c, 0, 0, 1},
                                {x1, y0, 0, 1, c, 0, 1, 0}, {x1, y1, 0, 1, c, 0, 1, 1}, {x0, y1, 0, 1, c, 0, 0, 1}};
    record.draw(cmd, v, 6, nullptr, 0);
}

int wordsWidth(const std::wstring& text, float px, bool bold) {
    if (text.empty()) return 0;
    // 3.0.11-beta.28: the DC and each size's font kept (the search's marks measure their words on every paint)
    struct Kept {
        int height;
        bool bold;
        HFONT font;
    };
    thread_local HDC dc = CreateCompatibleDC(nullptr);
    thread_local std::vector<Kept> fonts;
    const int height = std::max(8, int(std::lround(px)));
    HFONT font = nullptr;
    for (const Kept& k : fonts)
        if (k.height == height && k.bold == bold) font = k.font;
    if (!font) {
        font = CreateFontW(-height, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        if (!font) return 0;
        if (fonts.size() >= 32) {  // never one selected: each measure puts the stock font back
            DeleteObject(fonts.front().font);
            fonts.erase(fonts.begin());
        }
        fonts.push_back(Kept{height, bold, font});
    }
    if (!dc) return 0;
    SelectObject(dc, font);
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), int(text.size()), &size);
    SelectObject(dc, GetStockObject(SYSTEM_FONT));
    return int(size.cx);
}

}  // namespace uagfx::win
