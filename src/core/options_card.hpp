#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::optionscard {

// 3.0.11-beta.162 (task 8 of the remaining windows: the game's options, Alt+O): the game's options window (UI window
// 0xD, FUN_00473dd0) as a card in style A - the music and the sounds (a switch and a volume of 0-127 each), the
// cursor's snap to a target for the attack, the skills and the things on the ground, the game's windows opaque; each
// row the window's own control (a checkbox: message 6 / 0x93 from it with its new state; a slider: message 8 from it
// by a step). The parts that need no Windows; the words by GDI (win/options_card).
constexpr uint32_t kToggleAt[6] = {0x7C, 0x78, 0x80, 0x84, 0x88, 0x8C};  // the window's checkboxes' pointers
constexpr uint32_t kSliderAt[2] = {0x68, 0x6C};                          // its sliders' (the music's, the sounds')
constexpr int kCheckMsg = 0x93, kStepMsg = 8, kCloseButton = 0x87, kVolumeMost = 127;

struct Values {
    bool on[6] = {false, false, false, false, false, false};  // music, sounds, attack / skill / item snap, opaque
    int volume[2] = {0, 0};                                   // the music's, the sounds' (0-127)
};
int volumeAt(const panel::Box& slider, float x);  // a slider's place to its volume
// 3.0.11-beta.200 (the tester: «непрозорі вікна - тогл не працює ... не повертає прозорі»): uaRO.exe only keeps its
// «opaque» (m_isOpaque, the session's + 0x814: the registry's, its checkbox's) - nothing draws by it; the interface's
// windows whole by it, else at «Прозорість вікон»
constexpr uint32_t kOpaqueFlag = 0x771148 + 0x814;
inline int windowOpacity(int configured, bool opaque) { return opaque ? 100 : configured; }

struct Texts {
    std::string title, snapHint;
    std::string toggles[6];
};
struct Show {
    int hoverToggle = -1, hoverSlider = -1;
    bool hoverClose = false;
    int focus = -1;  // the controller's row: 0-1 the volumes, 2-5 the other switches
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, close, hint;
    panel::Box labels[6], toggles[6];  // a row's words and its switch
    panel::Box sliders[2], shares[2];  // the volumes' bars and their per cent
};
Layout layout(int targetW, int targetH, float scale);
enum class Part : uint8_t { None, Toggle, Slider, Close };
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
std::vector<Words> words(const Layout& L, const Values& v, const Show& sh, const Texts& t);

}  // namespace uagfx::optionscard
