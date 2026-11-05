#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::partysetup {

// 3.0.11-beta.158 (task 6 of the remaining windows: the party's settings, Alt+P): the game's «Config Party» window
// (UI window 0x35, FUN_00485650) as a card in style A - three rows of two choices (the experience: each takes or
// shared; the things picked up and their sharing: each takes or shared), each choice its own radio button's
// (message 6 / 0x95 from that button: the game checks the leader), OK and cancel its own buttons. The parts that need
// no Windows; the words by GDI (win/party_setup).
constexpr int kChooseMsg = 0x95, kOkButton = 0x76, kCancelButton = 0x77;
struct GameRow {
    uint32_t buttons;  // the window's two radio buttons' pointers (choice 0 at +0, 1 at +4)
    uint32_t value;    // the chosen one (0 or 1)
};
constexpr GameRow kRows[3] = {{0x60, 0x68}, {0x6C, 0x74}, {0x78, 0x80}};

struct Texts {
    std::string title, ok, cancel, leaderOnly;
    std::string thingsFixed;  // 3.0.11-beta.200: «Речі діляться так, як задано при створенні групи»
    std::string rows[3];
    std::string choices[3][2];
};
struct Show {
    int values[3] = {0, 0, 0};
    bool present[3] = {true, true, true};  // a row whose buttons the game made
    bool canChange = true;                 // the leader (or a party being made)
    // 3.0.11-beta.200 (the tester: «судячи з шари луту справді не вносить зміни»): in a party already - the game sends
    // the experience's choice alone (the packet 0x0102), uAthena keeps the things' as the party was made
    bool thingsFixed = false;
    int hoverRow = -1, hoverChoice = -1;
    int hoverPart = 0;  // its Part
    int focus = -1;     // the controller's row
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, note, ok, cancel;
    panel::Box labels[3];
    panel::Box choices[3][2];
};
// .270: `rows` shown (1: a party already - the things' rows left out, the tester: «можливо прибрати з налаштувань
// дійсної паті, і залишити тільки при створенні»)
Layout layout(int targetW, int targetH, float scale, int rows = 3);
enum class Part : uint8_t { None, Choice, Ok, Cancel };
struct Hit {
    Part part = Part::None;
    int row = -1, choice = -1;
};
Hit hitTest(const Layout& L, float x, float y);
bool changeable(const Show& sh, int row);  // 3.0.11-beta.200: a row's choices pressed: the leader's, not the things'

void paint(panel::Picture& out, const Layout& L, const Show& sh);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Show& sh, const Texts& t);

}  // namespace uagfx::partysetup
