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
    if (pic.width <= 0 || pic.height <= 0) return;
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = pic.width;
    bi.bmiHeader.biHeight = -pic.height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bmp && bits) {
        HGDIOBJ oldBmp = SelectObject(dc, bmp);
        std::memset(bits, 0, size_t(pic.width) * size_t(pic.height) * 4);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        for (const Words& w : words) {
            if (w.text.empty() || w.box.w <= 0) continue;
            HFONT font = CreateFontW(-std::max(8, int(std::lround(w.px))), 0, 0, 0, w.bold ? FW_SEMIBOLD : FW_NORMAL,
                                     FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            HGDIOBJ oldFont = SelectObject(dc, font);
            RECT r{LONG(std::lround(w.box.x)), LONG(std::lround(w.box.y)), LONG(std::lround(w.box.x + w.box.w)),
                   LONG(std::lround(w.box.y + w.box.h))};
            DrawTextW(dc, w.text.c_str(), int(w.text.size()), &r,
                      (w.wrap ? DT_WORDBREAK | DT_TOP : DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS) | DT_NOPREFIX |
                          w.align);
            SelectObject(dc, oldFont);
            DeleteObject(font);
        }
        GdiFlush();
        const auto* cover = static_cast<const uint32_t*>(bits);
        for (const Words& w : words) {
            const int x0 = std::max(0, int(w.box.x)), y0 = std::max(0, int(w.box.y));
            const int x1 = std::min(pic.width, int(std::ceil(w.box.x + w.box.w)));
            const int y1 = std::min(pic.height, int(std::ceil(w.box.y + w.box.h)));
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x) {
                    const size_t i = size_t(y) * size_t(pic.width) + size_t(x);
                    const uint32_t c = cover[i];
                    const uint32_t v = std::max({c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF});
                    if (v) panel::over(pic.px[i], w.rgb, float(v) / 255.0f);
                }
        }
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
