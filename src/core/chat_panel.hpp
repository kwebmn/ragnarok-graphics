#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
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
    int item = 0, mob = 0;  // 3.0.11-beta.244: a thing's or a monster's name - a click opens it in «Довідник»
};
// 3.0.18-beta.245 («Довідник» › chat links): «[Name]» in the words - a thing's or a monster's name `resolve` knows
// ({item, mob}; {0, 0} none) becomes its link span (the brackets kept); the speaker's and the links' spans untouched
std::vector<Span> bracketLinks(const std::vector<Span>& in,
                               const std::function<std::pair<int, int>(const std::string& name)>& resolve);
struct Line {
    Kind kind = Kind::System;
    std::vector<Span> spans;
    std::string who;     // who said it ("" none), as the game wrote it (UTF-8)
    bool trade = false;  // one near selling or buying
    uint64_t ms = 0;
    int repeat = 1;      // 3.0.11-beta.243: the same line again right after: «×N»
    uint32_t serial = 0;  // 3.0.18-beta.170: its own number (WrapCache's key); 0 - none, never kept
};

struct Texts {
    std::string tabs[6];       // «Усі», «Група», «Гільдія», «Шепіт», «Торг», «Бій»
    std::string from, to;      // «від {1}», «до {1}»
    std::string send, type;    // the empty field's hint: «Enter — надіслати · Tab — канал», «Enter — писати …»
    std::string walk;          // 3.0.18-beta.208: the walk mode's «Ходьба · Enter — писати · ` — вимкнути»
    std::string newer;         // the scrolled history's way back down
    // 3.0.11-beta.240 (the chat concept, stage 1): the commands' hints over the input while «/» is typed - «/p — у
    // групу», «/g — у гільдію», «/w Ім'я — шепіт», «/r — відповісти {1}»
    std::string commands[4];
    std::string all;          // 3.0.11-beta.242: «Усі» on the whispers' row
    std::string menu[4];      // «Шепнути», «Блокувати шепіт», «Копіювати ім'я»; .244 «Копіювати рядок»
    // 3.0.11-beta.243 (the chat concept, stage 4): «Усі»'s filters - «Системні рядки», «Підібране й досвід»,
    // «Оголошення», «Торгівля»; the server's own lines shorter - «Автолут: до {1}», «Автолут вимкнено», «Показ досвіду
    // ввімкнено» / «вимкнено», «Показ зені ввімкнено» / «вимкнено», «Досвід: Base +{1} · Job +{2}»
    std::string filters[4];
    std::string autoloot, autolootOff, expOn, expOff, zenyOn, zenyOff, expGained, noSp;  // .243, 3.0.12-beta.3
    std::string search, searchHint;  // 3.0.11-beta.284: «Пошук», «Шукати в рядках · Esc — закрити»
    // 3.0.13-beta.3 (the owner's pick 24 of «50 идей»: «Свои цвета каналов чата»): the party's and the guild's
    // colours, one's own and the others' words in the public chat (0: the game's) - a GM's own colour kept
    uint32_t party = kParty, guild = kGuild, own = 0, others = 0;
};
// 3.0.13-beta.3 (the owner's pick 28 of «50 идей»: «Журнал чата в файл по дням»): [interface] chat_log - a line as
// the log keeps it, «[hh:mm:ss] words ×N»; "" a line the log skips (the fight's)
std::string logLine(const Line& l, int hour, int minute, int second);
// 3.0.13-beta.12 (the owner's pick 21 of «50 идей»: «Длиннее строка чата»): the server takes a line of 70 letters at
// most (uAthena CHATBOX_SIZE; a public one longer is dropped, a whisper cut) - a longer one sent as several, cut at
// spaces, each `most` bytes at most (a word longer than that cut where it must); the game's code page's bytes
constexpr size_t kChatPiece = 68, kChatLongest = 234;
std::vector<std::string> pieces(const std::string& text, size_t most);
// 3.0.13-beta.3: [interface] chat_party, chat_guild, chat_own, chat_others - a colour by its name; "default" (or
// unknown) `fallback`
uint32_t chatColour(const std::string& token, uint32_t fallback);
// 3.0.11-beta.243: «Усі»'s filters - each bit hides one kind there
constexpr int kHideSystem = 1, kHideLoot = 2, kHideAnnounce = 4, kHideTrade = 8;
// The server's own words made short (the autoloot, the shown experience and zeny, «Experience Gained …»); false: as
// they came. A gained experience's line becomes the loot's (its filter, «Бій»)
bool shortSystem(const std::string& text, const Texts& t, Line& out);
// «14:32» for a line `ago` ms old when the day's clock stands at `nowSec` seconds
std::string clock(uint64_t ago, int nowSec);
// 3.0.11-beta.240: the input's own commands - «/p words» the party, «/g words» the guild, «/w Name words» (a name with
// spaces in quotes) a whisper, «/r words» a whisper to the last who whispered; false none of them (the words as typed)
struct Command {
    int mode = 0;          // 0 near, 1 the party, 2 the guild
    std::string whisper;   // the name a whisper goes to; "" none
    std::string words;     // what is sent
};
bool command(const std::string& utf8, const std::string& replyTo, Command& out);
bool commandHints(const std::string& utf8);  // «/», «/p» ... typed and no space yet: the hints shown

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
// 3.0.11-beta.242: on «Шепіт» with one chosen - that one's whispers only
bool showsWith(Tab tab, const std::string& person, const Line& line, int hide = 0);
int unreadOn(const Line& line);  // the tab a new line counts on while another one is open; -1 none
// 3.0.11-beta.284 (the owner's feature 17): a line with these words (case folded, in its words or its speaker's)
bool matches(const Line& line, const std::string& foldedWords);

// The input's row as the game has it now (its edit's words and caret, its send mode, the whisper's name)
struct Input {
    std::string text;            // UTF-8
    size_t caret = std::string::npos;  // bytes into `text`; npos its end
    bool typing = true;          // the keys go to the chat (the battle mode: after Enter)
    bool battle = false;         // the game's battle mode
    bool walk = false;           // 3.0.18-beta.208: the walk mode (win/walk_mode) - typing after Enter
    int mode = 0;                // 0 near, 1 the party, 2 the guild
    std::string whisper;         // the name the words go to (UTF-8); "" none
    bool whisperFocus = false;   // the name being typed
    bool caretOn = true;         // the caret's blink
    std::string replyTo;         // 3.0.11-beta.240: the last who whispered (UTF-8); "" none
    bool searching = false;      // 3.0.11-beta.284: the field is the search's (Ctrl+F in it): `text` its words
};

using Measure = std::function<float(const std::string& utf8, float px, bool bold)>;
  // panel::wordsWidth

// A row of the lines' area: its pieces left to right (x from the area's left)
struct Piece {
    std::string text;
    uint32_t rgb = kText;
    bool bold = false, name = false;
    float x = 0, w = 0;
    int line = -1;  // which of the lines given
    int item = 0, mob = 0;  // 3.0.11-beta.244: as its span's
};
using Row = std::vector<Piece>;
// The lines as rows `width` wide at `px`, broken at spaces (a word wider than a row by its letters)
std::vector<Row> wrap(const std::vector<const Line*>& lines, float width, float px, const Measure& m);

// 3.0.18-beta.170 (R-CD8KCP: the chat's frame grew from 0.13 to 1.33 ms in three minutes of hunting - each kill,
// pick-up or experience line wrapped all its up to 200 lines again, every word measured): wrap's rows of each line
// kept while it stays the same (its number, its spans' count, its repeats), and the width, the size and the look
// (`generation`) do too - only a new or changed line measured; the lines no longer given let go
class WrapCache {
public:
    std::vector<Row> wrap(const std::vector<const Line*>& lines, float width, float px, const Measure& m,
                          int generation);

private:
    struct Kept {
        size_t spans = 0;
        int repeat = 0;
        std::vector<Row> rows;
    };
    std::unordered_map<uint32_t, Kept> kept_;
    float width_ = -1, px_ = -1;
    int generation_ = 0;
};

struct Show {
    Tab tab = Tab::All;
    int scroll = 0;  // rows up from the newest
    int unread[kTabs] = {};
    int hoverTab = -1, hoverLine = -1;  // the line whose name is pointed at
    bool hoverMode = false, hoverClear = false, hoverNewer = false;
    bool hoverGrip = false;  // 3.0.11-beta.241
    bool hoverBar = false;   // 3.0.11-beta.245
    // 3.0.11-beta.242 (the chat concept, stage 3: «шёпот по собеседникам»): on «Шепіт» the ones whispered with, the
    // latest first, their unread; the one shown ("" all); a name's menu (its line, where; -1 none) and the item pointed
    std::vector<std::string> people;
    std::vector<int> peopleUnread;
    std::string person;
    int hoverPerson = -1;
    int menuLine = -1, hoverItem = -1;
    bool menuNamed = true;  // 3.0.11-beta.244: a name's menu (its four), else a line's (copy it alone)
    int hoverLink = 0;      // 3.0.11-beta.244: the link pointed at - a thing's id, a monster's as -id; 0 none
    float menuX = 0, menuY = 0;
    // 3.0.11-beta.243: «Усі»'s filters (kHide*), their menu open, pointed at; the line pointed at (its time), the clock
    int hide = 0;
    bool filtersOpen = false, hoverFilters = false;
    bool hoverSearch = false;  // 3.0.18-beta.105
    int hoverFilter = -1, hoverRowLine = -1;
    uint64_t nowMs = 0;
    int clockSec = -1;  // the day's seconds now; < 0 no times
    std::vector<uint64_t> lineTimes;  // the rows' lines' ms (as their Line::ms), by the rows' line index
};

struct Layout {
    float s = 1;
    bool quiet = false;  // idle: dimmed to its last lines, no tabs, no input
    int x = 0, y = 0, w = 0, h = 0;  // the picture in the target
    int rows = 0;                     // the lines' rows shown
    float px = 12.5f, rowH = 18;      // the lines' words and rows
    panel::Box glass, tabsRow, lines, input, mode, clear, field, newer;
    panel::Box hints;  // 3.0.11-beta.240: the commands' hints over the rows (w 0: none)
    panel::Box grip;   // 3.0.11-beta.241: its top right corner - drags its size
    // 3.0.11-beta.242: the whispers' row (its «Усі» first, then each one), a name's menu and its items
    panel::Box peopleRow, menu;
    std::vector<panel::Box> people, items;
    panel::Box filtersButton, filtersMenu;  // 3.0.11-beta.243
    panel::Box searchButton;  // 3.0.18-beta.105: the tabs' row's magnifier, left of the gear (every tab)
    panel::Box bar;  // 3.0.11-beta.245 (the owner: «в чате нету скрола»): the history's scroll bar's track
    std::vector<panel::Box> filters;
    std::vector<panel::Box> tabs;
};
// Its bottom left at (left, bottom) in the target; 3.0.11-beta.241: `width` at scale 1 (kLeast..kMost, by its corner)
constexpr int kWidth = 520, kLeastWidth = 360, kMostWidth = 900, kLeastRows = 4, kMostRows = 20;
Layout layout(int left, int bottom, float scale, int rows, bool quiet, const Show& sh, const Input& in, const Texts& t,
              const Measure& m, int width = kWidth);
// 3.0.11-beta.241 (the chat concept, stage 2: «размер мышью»): its corner dragged to (x, y) in the target - the width
// (scale 1) and the rows it gives, its bottom left staying
void sizeTo(const Layout& L, float x, float y, int& width, int& rows);

enum class Part : uint8_t {
    None, Tab, Name, Lines, Mode, Clear, Field, Newer, Body, Grip, Person, Item, Filters, Filter, Link, Bar,
    Search  // 3.0.18-beta.105: the magnifier - the search opened or closed
};
// 3.0.11-beta.245: the scroll bar - its thumb for `total` rows (w 0: all of them shown), the scroll (rows up from the
// newest) the cursor's y on its track gives (the thumb's middle there)
panel::Box barThumb(const Layout& L, size_t total, int scroll);
int scrollAt(const Layout& L, size_t total, float y);
// 3.0.11-beta.244: a menu's item - by its place, the action it does (0 whisper, 1 block, 2 copy the name, 3 the line)
int menuAction(const Show& sh, int index);
struct Hit {
    Part part = Part::None;
    int index = -1;  // a tab's; a name's line (of the rows' lines)
};
// The rows' first shown and how many (the newest at the bottom, `scroll` rows up)
void shown(const Layout& L, const std::vector<Row>& rows, int scroll, size_t& first, size_t& count);
Hit hitTest(const Layout& L, const std::vector<Row>& rows, const Show& sh, float x, float y);


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

// 3.0.18-beta.32 (uaUI's chat): what paint() decides beyond the layout - each tab's unread badge (w 0 none), the
// channel chip's colour and its chevron, the caret (the name's or the words'), the scroll's thumb, the underlines of
// the name or the link pointed at
struct Underline {
    panel::Box box;
    uint32_t rgb = 0;
};
struct View {
    std::vector<panel::Box> badges;
    uint32_t chipRgb = 0, caretRgb = 0;
    panel::Box chevron, caret, thumb;
    std::vector<Underline> underlines;
};
View view(const Layout& L, const std::vector<Row>& rows, const Input& in, const Show& sh, const Texts& t,
          const Measure& m);

}  // namespace uagfx::chatpanel
