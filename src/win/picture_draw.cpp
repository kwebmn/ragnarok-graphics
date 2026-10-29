#include "win/picture_draw.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/d3d7.hpp"
#include "core/effect_exe.hpp"
#include "core/glint.hpp"
#include "core/states.hpp"
#include "win/code_page.hpp"
#include "win/gpu/renderer.hpp"

namespace uagfx::win {

void drawWords(panel::Picture& pic, const std::vector<Words>& words) {
    drawWords(pic, words, Rect{0, 0, pic.width, pic.height});
}

void drawWords(panel::Picture& pic, const std::vector<Words>& words, const Rect& area) {
    const Rect a{std::max(0, area.left), std::max(0, area.top), std::min(pic.width, area.right),
                 std::min(pic.height, area.bottom)};
    if (a.empty() || pic.px.size() != size_t(pic.width) * size_t(pic.height)) return;
    const int w = a.width(), h = a.height();
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bmp && bits) {
        HGDIOBJ oldBmp = SelectObject(dc, bmp);
        HGDIOBJ oldFont = nullptr;
        std::memset(bits, 0, size_t(w) * size_t(h) * 4);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        // 3.0.11-beta.25 (R-77TCM7): a font for each size and weight, not for each of the words
        std::vector<std::pair<int, HFONT>> fonts;  // the height, negative for a bold one
        for (const Words& word : words) {
            if (word.text.empty() || word.box.w <= 0) continue;
            const panel::Box& b = word.box;
            RECT r{LONG(std::lround(b.x)) - a.left, LONG(std::lround(b.y)) - a.top,
                   LONG(std::lround(b.x + b.w)) - a.left, LONG(std::lround(b.y + b.h)) - a.top};
            if (r.right <= 0 || r.bottom <= 0 || r.left >= w || r.top >= h) continue;  // not over the area
            const int height = std::max(8, int(std::lround(word.px)));
            const int key = word.bold ? -height : height;
            HFONT font = nullptr;
            for (const auto& [k, f] : fonts)
                if (k == key) font = f;
            if (!font) {
                font = CreateFontW(-height, 0, 0, 0, word.bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                   DEFAULT_PITCH, L"Segoe UI");
                fonts.emplace_back(key, font);
            }
            const HGDIOBJ was = SelectObject(dc, font);
            if (!oldFont) oldFont = was;
            DrawTextW(dc, word.text.c_str(), int(word.text.size()), &r,
                      (word.wrap ? DT_WORDBREAK | DT_TOP : DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS) | DT_NOPREFIX |
                          word.align);
        }
        GdiFlush();
        const auto* cover = static_cast<const uint32_t*>(bits);
        for (const Words& word : words) {
            const int x0 = std::max(a.left, int(word.box.x)), y0 = std::max(a.top, int(word.box.y));
            const int x1 = std::min(a.right, int(std::ceil(word.box.x + word.box.w)));
            const int y1 = std::min(a.bottom, int(std::ceil(word.box.y + word.box.h)));
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x) {
                    const uint32_t c = cover[size_t(y - a.top) * size_t(w) + size_t(x - a.left)];
                    const uint32_t v = std::max({c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF});
                    if (v) panel::over(pic.px[size_t(y) * size_t(pic.width) + size_t(x)], word.rgb, float(v) / 255.0f);
                }
        }
        if (oldFont) SelectObject(dc, oldFont);
        for (const auto& [k, f] : fonts) DeleteObject(f);
        SelectObject(dc, oldBmp);
    }
    if (bmp) DeleteObject(bmp);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
}

std::wstring utf8Wide(const std::string& s) { return widened(s, CP_UTF8); }

void drawPicture(FrameRecord& record, gpu::Renderer& renderer, const panel::Picture& pic, uint32_t& texture, int& texW,
                 int& texH, bool& upload, int x, int y, float k) {
    if (pic.width <= 0 || pic.height <= 0) return;
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
    // Premultiplied, no depth, a texel a target pixel
    DrawSetup s;
    s.ui = true;
    s.blend = true;
    s.srcBlend = d3d7::BLEND_ONE;
    s.dstBlend = d3d7::BLEND_INVSRCALPHA;
    s.stage[0].colorOp = s.stage[0].alphaOp = d3d7::TOP_SELECTARG1;
    s.stage[0].colorArg1 = s.stage[0].alphaArg1 = d3d7::TA_TEXTURE;
    s.stage[0].addressU = s.stage[0].addressV = d3d7::TADDRESS_CLAMP;
    s.stage[0].magFilter = d3d7::TFG_POINT;
    s.stage[0].minFilter = d3d7::TFN_POINT;
    s.stage[1].colorOp = s.stage[1].alphaOp = d3d7::TOP_DISABLE;
    DrawCmd cmd;
    cmd.state = record.addState(s);
    cmd.texture[0] = texture;
    cmd.effect = uint8_t(effect_exe::Kind::Panel);  // sampled as it is (draw.cpp, renderer11.cpp)
    cmd.primitive = d3d7::PT_TRIANGLELIST;
    cmd.fvf = d3d7::FVF_TL1;
    // a game x lands at x * k + 0.5 in the target (shaders.cpp draw_vs): the quad's edges on the target's pixel edges
    const float x0 = (float(x) - 0.5f) / k, y0 = (float(y) - 0.5f) / k;
    const float x1 = (float(x + texW) - 0.5f) / k, y1 = (float(y + texH) - 0.5f) / k;
    const uint32_t white = 0xFFFFFFFFu;
    const glint::Vertex v[6] = {{x0, y0, 0, 1, white, 0, 0, 0}, {x1, y0, 0, 1, white, 0, 1, 0},
                                {x0, y1, 0, 1, white, 0, 0, 1}, {x1, y0, 0, 1, white, 0, 1, 0},
                                {x1, y1, 0, 1, white, 0, 1, 1}, {x0, y1, 0, 1, white, 0, 0, 1}};
    record.draw(cmd, v, 6, nullptr, 0);
}

int wordsWidth(const std::wstring& text, float px, bool bold) {
    if (text.empty()) return 0;
    HDC dc = CreateCompatibleDC(nullptr);
    HFONT font = CreateFontW(-std::max(8, int(std::lround(px))), 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE,
                             FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                             DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ old = SelectObject(dc, font);
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), int(text.size()), &size);
    SelectObject(dc, old);
    DeleteObject(font);
    DeleteDC(dc);
    return int(size.cx);
}

}  // namespace uagfx::win
