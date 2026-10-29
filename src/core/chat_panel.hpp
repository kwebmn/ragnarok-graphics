#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::chatpanel {

// 3.0.11-beta.41 (stage 8 of the new interface; the owner: «делай чат»): the chat redrawn at the bottom left, style A
// (the concept's chat with tabs, v23): one window with its tabs «Усі», «Група», «Гільдія», «Шепіт», «Торг», «Бій» (the
// battle's log one of them), each channel its colour, the newest line at the bottom, the input's row under them with
// its channel. The game's own lines come as the game pushes them into its chat (win/chat_window tells where each came
// from); the battle's from the packets. Idle, it dims to its last lines. The parts that need no Windows; the words by
// GDI (win/chat_window).

enum class Kind : uint8_t { Public, Party, Guild, Whisper, Announce, System, Battle, Loot };
// Where a line of the game's came from (the caller of its push): one near, the character's own near, a whisper, a
// whisper's answer (the character's own sent ones among them), an announcement, the party's, the guild's, the rest
enum class Source : uint8_t { Public, PublicOwn, Whisper, WhisperResult, Announce, Party, Guild, Other };

constexpr uint32_t kText = 0xEEF2F8, kMuted = 0x9AA3B5, kFaint = 0x7E889A, kParty = 0x7FD3FF, kGuild = 0x9FE07C;
constexpr uint32_t kWhisper = 0xFFAEE0, kSystem = 0xF3C653, kRed = 0xFF7D6E, kBaseExp = 0xA8B4FF, kJobExp = 0x7FE0D0;

struct Span {
    std::string text;  // UTF-8
    uint32_t rgb = kText;
    bool bold = false;
    bool name = false;  // the one who said it: a click whispers to them
};
struct Line {
    Kind kind = Kind::System;
    std::vector<Span> spans;
    std::string who;     // who said it ("" none), as the game wrote it (UTF-8)
    bool trade = false;  // one near selling or buying
    uint64_t ms = 0;
};

struct Texts {
    std::string tabs[6];       // «Усі», «Група», «Гільдія», «Шепіт», «Торг», «Бій»
    std::string from, to;      // «від {1}», «до {1}»
    std::string send, type;    // the empty field's hint: «Enter — надіслати · Tab — канал», «Enter — писати …»
    std::string newer;         // the scrolled history's way back down
};

uint32_t gameRgb(uint32_t colorRef);  // the game's 0x00BBGGRR as 0xRRGGBB
// A line the game pushed into its chat (UTF-8), from where it came, in the game's colour
Line fromGame(Source source, const std::string& text, uint32_t colorRef, const Texts& t, uint64_t ms);
// One near selling or buying: «S>», «B>», «WTS», «WTB», «продам», «куплю», «обмін» ...
bool tradeLike(const std::string& utf8);

enum class Tab : uint8_t { All, Party, Guild, Whisper, Trade, Battle };
constexpr int kTabs = 6;
// What a tab shows: «Усі» all but the battle's hits (its kills and pick-ups too), «Торг» one near selling or buying,
// «Бій» the battle's hits, kills, experience and pick-ups
bool shows(Tab tab, const Line& line);
int unreadOn(const Line& line);  // the tab a new line counts on while another one is open; -1 none

// The input's row as the game has it now (its edit's words and caret, its send mode, the whisper's name)
struct Input {
    std::string text;            // UTF-8
    size_t caret = std::string::npos;  // bytes into `text`; npos its end
    bool typing = true;          // the keys go to the chat (the battle mode: after Enter)
    bool battle = false;         // the game's battle mode
    int mode = 0;                // 0 near, 1 the party, 2 the guild
    std::string whisper;         // the name the words go to (UTF-8); "" none
    bool whisperFocus = false;   // the name being typed
    bool caretOn = true;         // the caret's blink
};

using Measure = std::function<float(const std::string& utf8, float px, bool bold)>;
float estimate(const std::string& utf8, float px, bool bold);  // panel::wordsWidth

// A row of the lines' area: its pieces left to right (x from the area's left)
struct Piece {
    std::string text;
    uint32_t rgb = kText;
    bool bold = false, name = false;
    float x = 0, w = 0;
    int line = -1;  // which of the lines given
};
using Row = std::vector<Piece>;
// The lines as rows `width` wide at `px`, broken at spaces (a word wider than a row by its letters)
std::vector<Row> wrap(const std::vector<const Line*>& lines, float width, float px, const Measure& m);

struct Show {
    Tab tab = Tab::All;
    int scroll = 0;  // rows up from the newest
    int unread[kTabs] = {};
    int hoverTab = -1, hoverLine = -1;  // the line whose name is pointed at
    bool hoverMode = false, hoverClear = false, hoverNewer = false;
};

struct Layout {
    float s = 1;
    bool quiet = false;  // idle: dimmed to its last lines, no tabs, no input
    int x = 0, y = 0, w = 0, h = 0;  // the picture in the target
    int rows = 0;                     // the lines' rows shown
    float px = 12.5f, rowH = 18;      // the lines' words and rows
    panel::Box glass, tabsRow, lines, input, mode, clear, field, newer;
    std::vector<panel::Box> tabs;
};
// Its bottom left at (left, bottom) in the target
Layout layout(int left, int bottom, float scale, int rows, bool quiet, const Show& sh, const Input& in, const Texts& t,
              const Measure& m);

enum class Part : uint8_t { None, Tab, Name, Lines, Mode, Clear, Field, Newer, Body };
struct Hit {
    Part part = Part::None;
    int index = -1;  // a tab's; a name's line (of the rows' lines)
};
// The rows' first shown and how many (the newest at the bottom, `scroll` rows up)
void shown(const Layout& L, const std::vector<Row>& rows, int scroll, size_t& first, size_t& count);
Hit hitTest(const Layout& L, const std::vector<Row>& rows, const Show& sh, float x, float y);

void paint(panel::Picture& out, const Layout& L, const std::vector<Row>& rows, const Input& in, const Show& sh,
           const Texts& t, const Measure& m);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = kText;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const std::vector<Row>& rows, const Input& in, const Show& sh,
                         const Texts& t, const Measure& m);
// The field's words as shown (their tail when wider than it, «…» before) and the caret's x from the field's left
std::string fieldText(const Layout& L, const Input& in, const Measure& m, float& caretX);

std::string grouped(long long n);  // 6 140, 1 288

}  // namespace uagfx::chatpanel
