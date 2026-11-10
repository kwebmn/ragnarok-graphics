#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::chatwheel {

// 3.0.11-beta.214 (the owner: «посмотри, как сделать удобно начинать общение с геймпада»; answers 1 «придумай», 2
// «давай», 4 «можно», 5 «надо настраивать и поставь те по умолчанию которые по умолчанию в игре»): the chat wheel -
// the Talk button (D-pad ↓) held opens it at the screen's middle (a tap still talks to an NPC); the right stick points
// at a sector, letting the button go sends it: eight phrases (the player's own, [pad] phrase1-8, else the interface's)
// to everyone, the party or the guild, or ten emotions (the player's, [pad] emotes, else the game's own Alt+1-0). LB
// and RB the page, LT and RT the channel, Y the Windows voice typing into the chat's line. The parts that need no
// Windows; the words by GDI (win/chat_wheel).
enum class Page : uint8_t { Phrases, Emotes };
enum class Channel : uint8_t { Near, Party, Guild };
constexpr int kPhrases = 8, kEmotes = 10, kChannels = 3;

// The right stick (x right, y up, -1..1) at a sector of `count`: the first at the top, then clockwise; -1 within the
// dead middle
int sectorAt(float x, float y, int count, float dead = 0.5f);
std::vector<std::string> phraseList(const std::array<std::string, kPhrases>& configured,
                                    const std::array<std::string, kPhrases>& defaults);
// The configured emotions' commands (by spaces, «/» each, ten at most); none - `defaults`
std::vector<std::string> emoteList(const std::string& configured, const std::vector<std::string>& defaults);

struct Texts {
    std::string pages[2], channels[kChannels];
    std::string keyboard, voice, hint;  // with their buttons' names; the hint under the wheel
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    float cx = 0, cy = 0, inner = 0, outer = 0;  // the ring in the picture
    panel::Box pages[2], channels[kChannels], keyboard, voice, hint;
    std::vector<panel::Box> labels;  // a sector each
};
Layout layout(int count, int targetW, int targetH, float scale);


enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Center;
    bool wrap = false;
};
std::vector<Words> words(const Layout& L, const std::vector<std::string>& items, int hover, Page page,
                         Channel channel, const Texts& t);

}  // namespace uagfx::chatwheel
