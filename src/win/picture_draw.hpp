#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/frame.hpp"
#include "core/window_panel.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win {

// 3.0.4-beta.6: the driver's own pictures in the game's frame (3.0.3-beta.8's windows' panel, its tip and menu; the
// buff timers) - words by GDI into a picture, the picture as one quad over the game's interface.
struct Words {
    panel::Box box;
    std::wstring text;
    uint32_t rgb = panel::kText;
    float px = 12;
    bool bold = false;
    UINT align = DT_LEFT;
    bool wrap = false;  // 3.0.11-beta.16: lines broken at words in the box, from its top (a quest's steps)
};

// The words, white on black by GDI into a picture of its size: each one's coverage in its own colour over the
// picture's shapes (the words' boxes do not overlap)
void drawWords(panel::Picture& pic, const std::vector<Words>& words);
// 3.0.11-beta.25: the same over `area` of the picture only (the words there, the rest of the picture kept)
void drawWords(panel::Picture& pic, const std::vector<Words>& words, const Rect& area);
// A picture as it is into the frame: its texture made for its size, uploaded when it changed, one quad with its top
// left at (x, y) in the target's pixels over the game's interface (3.0.11-beta.46: `opacity` under 1 - the whole
// picture fainter, the interface's motion)
void drawPicture(FrameRecord& record, gpu::Renderer& renderer, const panel::Picture& pic, uint32_t& texture, int& texW,
                 int& texH, bool& upload, int x, int y, float k, float opacity = 1.0f);
// 3.0.11-beta.50: a picture stretched over (x, y, w, h) of the target, sampled smoothly (the low HP edge)
void drawStretched(FrameRecord& record, gpu::Renderer& renderer, const panel::Picture& pic, uint32_t& texture,
                   int& texW, int& texH, bool& upload, int x, int y, int w, int h, float k, float opacity = 1.0f);
// 3.0.11-beta.54 (R-PVZH5M: «графика чегото притормаживает когда бьешь мобов» - at 4K with the render scale 200 % the
// character block alone was 1488 x 1200 pixels, painted again at each HP change): the new interface's parts paint at
// the screen's own resolution - with a whole render scale r (200 %) their k and places are the target's / r and each
// texel of theirs shows as r x r target pixels (the target's extra pixels are the world's supersampling): r² fewer
// pixels to paint and to send. The stretch set each frame before the parts' frames (1: none); drawPart as drawPicture,
// drawPartStretched as drawStretched, at it
void setPartStretch(int stretch);
int partStretch();
void drawPart(FrameRecord& record, gpu::Renderer& renderer, const panel::Picture& pic, uint32_t& texture, int& texW,
              int& texH, bool& upload, int x, int y, float k, float opacity = 1.0f);
void drawPartStretched(FrameRecord& record, gpu::Renderer& renderer, const panel::Picture& pic, uint32_t& texture,
                       int& texW, int& texH, bool& upload, int x, int y, int w, int h, float k, float opacity = 1.0f);
// 3.0.11-beta.49: a black shade `alpha` over (x, y, w, h) - one untextured quad (the world dimmed under the windows);
// 3.0.11-beta.54 a part's: its pixels, at the parts' stretch
void drawShade(FrameRecord& record, int x, int y, int w, int h, float k, float alpha);
// 3.0.11-beta.95: a quad of one colour (ARGB, its alpha its own) over (x, y, w, h) of a part's pixels (the monsters'
// HP bars)
void drawFill(FrameRecord& record, float x, float y, float w, float h, float k, uint32_t argb);
// The words' width at a size (the target's pixels), as drawWords draws them
int wordsWidth(const std::wstring& text, float px, bool bold);
std::wstring utf8Wide(const std::string& s);

}  // namespace uagfx::win
