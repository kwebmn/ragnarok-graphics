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
};

// The words, white on black by GDI into a picture of its size: each one's coverage in its own colour over the
// picture's shapes (the words' boxes do not overlap)
void drawWords(panel::Picture& pic, const std::vector<Words>& words);
// A picture as it is into the frame: its texture made for its size, uploaded when it changed, one quad with its top
// left at (x, y) in the target's pixels over the game's interface
void drawPicture(FrameRecord& record, gpu::Renderer& renderer, const panel::Picture& pic, uint32_t& texture, int& texW,
                 int& texH, bool& upload, int x, int y, float k);
// The words' width at a size (the target's pixels), as drawWords draws them
int wordsWidth(const std::wstring& text, float px, bool bold);
std::wstring utf8Wide(const std::string& s);

}  // namespace uagfx::win
