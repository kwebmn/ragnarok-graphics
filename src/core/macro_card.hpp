#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::macrocard {

// 3.0.11-beta.169 (task 8 of the remaining windows: the chat's macros, Alt+M): the game's macro window (UI window
// 0x56, FUN_004534c0 / its create FUN_00452ec0) as a card in style A: ten rows «Alt+1» ... «Alt+0», each the game's
// own field (typed as before - a click gives it the keys; its words and caret shown as it has them), «Зберегти» the
// window's own Enter (its message 0 writes them), ✕ its close (6 / 0x87). The parts that need no Windows; the words
// by GDI (win/macro_card).
constexpr int kFields = 10, kSaveMsg = 0, kCloseButton = 0x87;
constexpr uint32_t kFirstField = 0x64;  // the window's first field (4 apart)
std::string keyName(int i);             // «Alt+1» ... «Alt+9», «Alt+0»

struct Texts {
    std::string title, hint, save;
};
struct Show {
    int focus = -1;  // the field with the keys
    bool caret = false;
    int hover = -1;  // a field
    bool hoverSave = false, hoverClose = false;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, hint, close, save;
    std::vector<panel::Box> keys, fields;
};
Layout layout(int targetW, int targetH, float scale);
enum class Part : uint8_t { None, Field, Save, Close };
struct Hit {
    Part part = Part::None;
    int index = -1;
};
Hit hitTest(const Layout& L, float x, float y);


enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const std::vector<std::string>& texts, const Show& sh, const Texts& t);

}  // namespace uagfx::macrocard
