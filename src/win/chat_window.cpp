#include "win/walk_mode.hpp"
#include "win/ui_field.hpp"
#include "win/chat_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "core/chat_panel.hpp"
#include "core/ui_icons.hpp"
#include "core/settings/search.hpp"
#include "core/hotbar_panel.hpp"
#include "core/layout.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/game_memory.hpp"
#include "win/hud_edit.hpp"
#include "win/log.hpp"
#include "win/part_marks.hpp"
#include "win/minimap_proto.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/reference_window.hpp"
#include "win/target_card.hpp"
#include "win/ui_cost.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"
#include "win/window_input.hpp"
#include "win/words.hpp"

namespace uagfx::win::chatwin {
namespace {

namespace cp = uagfx::chatpanel;

// uaRO.exe's: the windows' manager's message (FUN_004db310 - ECX the manager, the message, the words, their colour
// 0x00BBGGRR (0 white), one more; ret 0x10): 1 and 0x14 a line into the chat; its first bytes, whole instructions
constexpr uint32_t kPush = 0x4DB310;
const uint8_t kPushCode[] = {0x55, 0x8B, 0xEC, 0x6A, 0xFF};  // push ebp; mov ebp, esp; push -1
using PushFn = int(__attribute__((thiscall)) *)(uint32_t manager, int msg, const char* text, int colour, int more);

// Its callers by where they return to: the packets' handlers (the chat near 0x008d FUN_0057e730; the character's own
// 0x008e in the packets' loop; whispers 0x0097 FUN_0057fc00, their answers 0x0098 FUN_005803d0 - «( To NAME ) : »
// among them; announcements 0x009a FUN_0057f430 and 0x01c3 FUN_00596720; the party's 0x0109 FUN_00588cf0, the
// guild's 0x017f FUN_00596640); the rest the game's own lines
struct Caller {
    uint32_t from, to;
    cp::Source source;
};
const Caller kCallers[] = {
    {0x57E730, 0x57E8A0, cp::Source::Public},  {0x579C7C, 0x579C7D, cp::Source::PublicOwn},
    {0x57FC00, 0x5803D0, cp::Source::Whisper}, {0x5803D0, 0x580CA0, cp::Source::WhisperResult},
    {0x57F430, 0x57FC00, cp::Source::Announce}, {0x596720, 0x596AA0, cp::Source::Announce},
    {0x588CF0, 0x588DE0, cp::Source::Party},   {0x596640, 0x596720, cp::Source::Guild}};

// The windows' manager: its chat window (UI window 1, its class's vtable; its place the manager keeps at +4, +8 and
// whether the player shows it at +0x10), the control with the keys (+0x6C, FUN_004ce510 gives them); a window's shown
// flag and place, the chat window's Move (vtable + 0x18, FUN_0045e880); its edits - the words (+0x68, 70 letters) and
// the whisper's name (+0x6C): GetText FUN_00443300 (while it has the keys the IME's words into it first), the caret
// after it at +0x4C (the IME's +0x48), SetText at the name's vtable + 0x98 (FUN_00446210); the send mode DAT_00772d60
// (0 near, 1 the party, 2 the guild), the battle mode and its typing
constexpr uint32_t kWindowMgr = 0x727ED8, kChatAt = kWindowMgr + 0x84, kChatClass = 0x697D78;
constexpr uint32_t kSavedX = kWindowMgr + 4, kSavedY = kWindowMgr + 8, kShownChoice = kWindowMgr + 0x10;
constexpr uint32_t kFocus = kWindowMgr + 0x6C, kSetFocus = 0x4CE510;
constexpr uint32_t kShown = 0x38, kRect = 0x14, kMoveSlot = 0x18, kMove = 0x45E880;
constexpr uint32_t kWordsEdit = 0x68, kNameEdit = 0x6C, kWordsClass = 0x696438, kNameClass = 0x696394;
constexpr uint32_t kGetText = 0x443300, kCaretAt = 0x4C, kSetTextSlot = 0x98, kSetText = 0x446210;
// 3.0.13-beta.12: the words' field's own SetText (its class 0x696438's vtable + 0x98) - not the name's 0x446210: the
// checks for that one on the words' field never passed (the «/p» line and the menu's «/ex» left the field as it was)
constexpr uint32_t kSetTextWords = 0x4431C0;
constexpr uint32_t kMode = 0x772D60, kBattle = 0x772D54, kTyping = 0x772D58;
// The character's account, the session's skills (a list: a node's next at +0, its SKILL_INFO at +8 - its id +4, its
// name +0x1C, FUN_00642860)
constexpr uint32_t kOwnAccount = 0x771F7C, kSkills = 0x771148 + 0x2D8;
constexpr int kAway = -4000;  // the game's chat window's x while the new one shows
using GetTextFn = const char*(__attribute__((thiscall)) *)(uint32_t edit);
using SetTextFn = void(__attribute__((thiscall)) *)(uint32_t edit, const char* text);
using SetFocusFn = void(__attribute__((thiscall)) *)(uint32_t manager, uint32_t control);
using MoveFn = void(__attribute__((thiscall)) *)(uint32_t window, int x, int y);

constexpr size_t kMostLines = 400, kMostBattle = 300, kMostPending = 512, kMostNodes = 512;
constexpr ULONGLONG kQuietMs = 12000, kBlinkMs = 530, kKillExpMs = 2500, kHitKeepMs = 20000;
constexpr ULONGLONG kServerExpMs = 120000;  // 3.0.12-beta.3
const int kHeights[] = {7, 12, 4};  // F10 goes round them

struct Raw {
    std::string text;
    uint32_t colour = 0;
    cp::Source source = cp::Source::Other;
    ULONGLONG ms = 0;
};

struct State {
    uisheet::Sheet sheet;  // 3.0.18-beta.32: the chat drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    bool on = false, shown = false, moved = false, forced = false, oldTold = false, folded = false;
    bool inputTold = false, swallowTab = false;
    bool widthTold = false;  // 3.0.18-beta.216 (R-JPMWSD): the input's two widths told once
    unsigned swallowChar = 0;
    int pasteTold = 0;  // 3.0.18-beta.84: Ctrl+V/C/X's control character, not into the game's field
    bool searching = false;  // 3.0.11-beta.284 (the owner's feature 17): Ctrl+F in the field - its words the search's
    std::string search;      // UTF-8
    WPARAM skipChar = 0;     // the character a key the search took makes next
    unsigned codePage = 0;
    std::vector<Raw> pending;
    std::deque<cp::Line> lines, battle;
    uint64_t version = 0;
    uint32_t serials = 0;     // 3.0.18-beta.170: each line's own number (the wrap cache's key)
    cp::WrapCache wrapCache;  // 3.0.18-beta.170 (R-CD8KCP): the lines' rows kept - a new or changed one alone wrapped
    ULONGLONG lastLine = 0;
    cp::Show show;
    int height = 0;
    cp::Input input;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    cp::Layout layout;
    std::vector<const cp::Line*> rowLines;  // valid while `version` is the rows' own (a line out of the store after)
    uint64_t rowsVersion = 0;
    std::vector<cp::Row> rows;
    std::string rowsKey, key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    std::map<uint32_t, ULONGLONG> hitAt;  // the monsters the character hit, when
    uint32_t exp[3] = {0, 0, 0};
    bool expKnown[3] = {false, false, false};
    cp::Line* killLine = nullptr;
    // 3.0.12-beta.3 (a player's screenshot: «Досвід: Base +19 950 · Job +25 110» and then «Base EXP +19 950», «Job
    // EXP +25 110»): the server tells the experience itself (@showexp) - when it last did; ours not added meanwhile
    ULONGLONG serverExpAt = 0;
    bool log = false;  // 3.0.13-beta.3: [interface] chat_log
    bool longLines = true;  // 3.0.13-beta.12: [interface] long_chat
    int longTold = 0;
    int logTold = 0;
    std::string coloursKey;  // 3.0.13-beta.3: [interface] chat_* colours
    uint32_t party = cp::kParty, guild = cp::kGuild, own = 0, others = 0;
    int serverExpTold = 0;
    ULONGLONG killAt = 0;
    std::map<uint16_t, std::string> skillNames;
    int linesTold = 0, clicksTold = 0;
    // 3.0.11-beta.240 (the chat concept, stage 1): the last who whispered (/r), a command's channel and whisper to put
    // back once its words are sent, how long the chat stays lit when idle ([interface] chat_fade; 0 always)
    std::string replyTo;
    bool restoreOn = false;
    int restoreMode = 0;
    std::string restoreName;
    ULONGLONG restoreAt = 0;
    ULONGLONG fadeMs = 12000;
    int commandsTold = 0;
    // 3.0.11-beta.241 (the chat concept, stage 2): its size by its corner - the width at scale 1, the rows (0: F10's),
    // being dragged, read once
    int width = cp::kWidth, customRows = 0;
    bool sizing = false, sizeRead = false;
    std::map<std::string, int> personUnread;  // 3.0.11-beta.242: each one's whispers not read yet
    bool barDrag = false;  // 3.0.11-beta.245: the scroll bar's thumb dragged
};

State& state() {
    static State s;
    return s;
}

uintptr_t g_push = 0;

int __attribute__((thiscall)) pushed(uint32_t manager, int msg, const char* text, int colour, int more) {
    if ((msg == 1 || msg == 0x14) && text) {
        const uint32_t from = uint32_t(reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
        cp::Source source = cp::Source::Other;
        for (const Caller& c : kCallers)
            if (from >= c.from && from < c.to) source = c.source;
        std::vector<Raw>& p = state().pending;
        if (p.size() >= kMostPending) p.erase(p.begin());
        p.push_back(Raw{std::string(text, strnlen(text, 512)), uint32_t(colour), source, GetTickCount64()});
        // 3.0.11-beta.213: @mobsearch's answers for the minimap (whatever the new chat's switch)
        minimapproto::heard(utf8Of(widened(p.back().text, state().codePage)), p.back().ms);
    }
    return reinterpret_cast<PushFn>(g_push)(manager, msg, text, colour, more);
}

// 3.0.13-beta.12: an edit's SetText as its own class has it (the words' 0x4431C0, the name's 0x446210); 0 another
uint32_t setTextOf(uint32_t edit) {
    const uint32_t f = edit ? read32(read32(edit) + kSetTextSlot) : 0;
    return f == kSetText || f == kSetTextWords ? f : 0;
}

std::string utf8(const State& s, const std::string& game) { return utf8Of(widened(game, s.codePage)); }

cp::Texts texts() {
    cp::Texts t;
    const settings::Text tabs[cp::kTabs] = {settings::Text::ChatTabAll,     settings::Text::ChatTabParty,
                                            settings::Text::ChatTabGuild,   settings::Text::ChatTabWhisper,
                                            settings::Text::ChatTabTrade,   settings::Text::ChatTabBattle};
    for (int i = 0; i < cp::kTabs; ++i) t.tabs[i] = say(tabs[i]);
    t.from = say(settings::Text::ChatFrom);
    t.to = say(settings::Text::ChatTo);
    t.send = say(settings::Text::ChatHintSend);
    t.walk = say(settings::Text::ChatHintWalk);  // 3.0.18-beta.208
    t.type = say(settings::Text::ChatHintType);
    t.newer = "\xE2\x86\x93";  // ↓
    t.commands[0] = say(settings::Text::ChatCmdParty);  // 3.0.11-beta.240
    t.commands[1] = say(settings::Text::ChatCmdGuild);
    t.commands[2] = say(settings::Text::ChatCmdWhisper);
    t.commands[3] = say(settings::Text::ChatCmdReply);
    t.all = say(settings::Text::ChatTabAll);  // 3.0.11-beta.242
    t.menu[0] = say(settings::Text::ChatMenuWhisper);
    t.menu[1] = say(settings::Text::ChatMenuBlock);
    t.menu[2] = say(settings::Text::ChatMenuCopy);
    t.menu[3] = say(settings::Text::ChatMenuCopyLine);  // 3.0.11-beta.244
    t.filters[0] = say(settings::Text::ChatFilterSystem);  // 3.0.11-beta.243
    t.filters[1] = say(settings::Text::ChatFilterLoot);
    t.filters[2] = say(settings::Text::ChatFilterAnnounce);
    t.filters[3] = say(settings::Text::ChatFilterTrade);
    t.autoloot = say(settings::Text::ChatSysAutoloot);
    t.autolootOff = say(settings::Text::ChatSysAutolootOff);
    t.expOn = say(settings::Text::ChatSysExpOn);
    t.expOff = say(settings::Text::ChatSysExpOff);
    t.zenyOn = say(settings::Text::ChatSysZenyOn);
    t.zenyOff = say(settings::Text::ChatSysZenyOff);
    t.expGained = say(settings::Text::ChatSysExpGained);
    t.noSp = say(settings::Text::ChatSysNoSp);  // 3.0.12-beta.3
    t.search = say(settings::Text::ChatSearch);  // 3.0.11-beta.284
    t.searchHint = say(settings::Text::ChatSearchHint);
    return t;
}

// 3.0.18-beta.216 (R-JPMWSD «при переміщенні курсора він стає на букву, а не між буквами»): the words measured as
// uaUI draws them - its faces letter by letter at RmlUi's whole-pixel size; GDI's whole line came out other than the
// letters drawn and the caret stood in a letter. GDI's only while uaUI does not draw
float measure(const std::string& utf8Text, float px, bool bold) {
    const float drawn = uaui::textWidth(utf8Text, px, bold);
    return drawn >= 0 ? drawn : float(wordsWidth(utf8Wide(utf8Text), px, bold));
}

const char* sourceName(cp::Source s) {
    switch (s) {
        case cp::Source::Public: return "one near";
        case cp::Source::PublicOwn: return "the character near";
        case cp::Source::Whisper: return "a whisper";
        case cp::Source::WhisperResult: return "a whisper's answer";
        case cp::Source::Announce: return "an announcement";
        case cp::Source::Party: return "the party";
        case cp::Source::Guild: return "the guild";
        case cp::Source::Other: return "the game";
    }
    return "?";
}

// 3.0.13-beta.3: [interface] chat_log - the line appended to today's file (several game windows append to it too)
void logged(State& s, const cp::Line& line) {
    if (!s.log) return;
    SYSTEMTIME t{};
    GetLocalTime(&t);
    const std::string text = cp::logLine(line, t.wHour, t.wMinute, t.wSecond);
    if (text.empty()) return;
    const std::string folder = layout::join(gameDir(), layout::kChatLogs);
    CreateDirectoryA(folder.c_str(), nullptr);
    char day[16];
    std::snprintf(day, sizeof day, "%04d-%02d-%02d.txt", t.wYear, t.wMonth, t.wDay);
    const std::string file = folder + "\\" + day;
    FILE* f = std::fopen(file.c_str(), "ab");
    if (!f) {
        if (s.logTold++ < 3) LOG_INFO("chat: [interface] chat_log - %s could not be opened", file.c_str());
        return;
    }
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
}

void add(State& s, cp::Line line) {
    logged(s, line);  // 3.0.13-beta.3
    const ULONGLONG now = GetTickCount64();
    // 3.0.11-beta.243 (the chat concept, stage 4): the same line again within a minute - «×N» on the one before
    if (line.kind != cp::Kind::Battle && !s.lines.empty()) {
        cp::Line& last = s.lines.back();
        const auto words = [](const cp::Line& l) {
            std::string w;
            for (const cp::Span& sp : l.spans) w += sp.text;
            return w;
        };
        if (last.kind == line.kind && last.who == line.who && line.ms >= last.ms && line.ms - last.ms < 60000 &&
            words(last) == words(line)) {
            ++last.repeat;
            last.ms = line.ms;
            ++s.version;
            return;
        }
    }
    const int u = cp::unreadOn(line);
    if (u >= 0 && u != int(s.show.tab)) ++s.show.unread[u];
    if (cp::shows(s.show.tab, line)) s.lastLine = now;
    std::deque<cp::Line>& to = line.kind == cp::Kind::Battle ? s.battle : s.lines;
    line.serial = ++s.serials;
    to.push_back(std::move(line));
    if (to.size() > (&to == &s.battle ? kMostBattle : kMostLines)) {
        if (&to.front() == s.killLine) s.killLine = nullptr;
        to.pop_front();
    }
    ++s.version;
}

// The lines the game pushed since: into the chat's (the words not logged - whispers are private)
void take(State& s) {
    if (s.pending.empty()) return;
    const cp::Texts t = texts();
    for (const Raw& r : s.pending) {
        if (s.linesTold < 12) {
            ++s.linesTold;
            LOG_INFO("chat: a line from %s (%zu bytes, colour %06X)", sourceName(r.source), r.text.size(), r.colour);
        }
        const std::string words = utf8(s, r.text);
        if (words.find("Experience Gained Base:") != std::string::npos ||
            words.rfind("Gained exp is now shown", 0) == 0) {  // 3.0.12-beta.3
            if (!s.serverExpAt && s.serverExpTold++ < 3)
                LOG_INFO("chat: the server tells the experience - the chat's own lines of it held");
            s.serverExpAt = GetTickCount64();
        } else if (words.rfind("Gained exp will not be shown", 0) == 0) {
            s.serverExpAt = 0;
        }
        cp::Texts tc = t;  // 3.0.13-beta.3: the player's colours
        tc.party = s.party;
        tc.guild = s.guild;
        tc.own = s.own;
        tc.others = s.others;
        cp::Line line = cp::fromGame(r.source, words, r.colour, tc, r.ms);
        if (r.source == cp::Source::Whisper && !line.who.empty()) s.replyTo = line.who;  // 3.0.11-beta.240: /r
        if (line.kind == cp::Kind::Whisper && !line.who.empty()) {  // 3.0.11-beta.242: the ones whispered with
            std::vector<std::string>& p = s.show.people;
            p.erase(std::remove(p.begin(), p.end(), line.who), p.end());
            p.insert(p.begin(), line.who);
            if (p.size() > 12) p.pop_back();
            const bool reading = s.show.tab == cp::Tab::Whisper && (s.show.person.empty() || s.show.person == line.who);
            if (r.source == cp::Source::Whisper && !reading) ++s.personUnread[line.who];
        }
        add(s, std::move(line));
    }
    s.pending.clear();
}

uint32_t gameChat() {
    const uint32_t w = read32(kChatAt);
    return w && read32(w) == kChatClass ? w : 0;
}

bool moveWindow(uint32_t window, int x, int y) {
    if (read32(read32(window) + kMoveSlot) != kMove) return false;
    reinterpret_cast<MoveFn>(uintptr_t(kMove))(window, x, y);
    return true;
}

// The game's chat window out of sight while the new one shows (shown all the same: its edit takes the IME's words as
// it draws); back where the game keeps it after
void holdOldChat(State& s, bool on) {
    const uint32_t chat = gameChat();
    if (!chat) {
        if (on && !s.oldTold) {
            s.oldTold = true;
            LOG_INFO("chat: the game's chat window is not where this build has it - it stays as it is");
        }
        return;
    }
    int32_t r[4] = {0, 0, 0, 0};
    if (!readGame(chat + kRect, r, sizeof r)) return;
    if (on) {
        if (r[0] != kAway && moveWindow(chat, kAway, r[1])) {
            if (!s.moved)
                LOG_INFO("chat: the game's chat window (%d, %d) moved off the screen - its input works on for the new "
                         "chat", r[0], r[1]);
            s.moved = true;
        }
        if (!read32(chat + kShown) && write32(chat + kShown, 1)) s.forced = true;
    } else if (s.moved) {
        moveWindow(chat, int(read32(kSavedX)), int(read32(kSavedY)));
        if (s.forced) write32(chat + kShown, read32(kShownChoice) ? 1 : 0);
        s.moved = s.forced = false;
        LOG_INFO("chat: the game's chat window back at its place");
    }
}

std::string editText(uint32_t edit) {
    const char* t = reinterpret_cast<GetTextFn>(uintptr_t(kGetText))(edit);
    return t ? std::string(t, strnlen(t, 256)) : std::string();
}

// 3.0.13-beta.12 (pick 21 of «50 идей»): [interface] long_chat - the game's field takes kChatLongest letters (its
// edit's most, +0x54, 70 the game's own); Enter on a longer line sends all its pieces but the last through the chat
// window's own Enter (its message 6 with the field's 0x76, vtable + 0x94, FUN_0045ef90: the field's words read,
// the field emptied, sent by the channel or the whisper set) - the last left in the field for the game's own Enter
constexpr uint32_t kEditMost = 0x54, kWindowMsgSlot = 0x94, kWindowMsg = 0x45EF90, kEnterMsg = 6, kWordsId = 0x76;
using WindowMsgFn = int(__attribute__((thiscall)) *)(uint32_t window, uint32_t from, int msg, int a, int b, int c);

uint32_t wordsEdit();

void sendLong(State& s, uint32_t words) {
    const std::string text = editText(words);
    if (text.size() <= cp::kChatPiece || text[0] == '/') return;  // the game's own line, or a command
    const uint32_t chat = gameChat();
    if (!chat || read32(read32(chat) + kWindowMsgSlot) != kWindowMsg || !setTextOf(words))
        return;
    const std::vector<std::string> parts = cp::pieces(text, cp::kChatPiece);
    if (parts.size() < 2) return;
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        reinterpret_cast<SetTextFn>(uintptr_t(setTextOf(words)))(words, parts[i].c_str());
        reinterpret_cast<WindowMsgFn>(uintptr_t(kWindowMsg))(chat, words, kEnterMsg, kWordsId, 0, 0);
    }
    reinterpret_cast<SetTextFn>(uintptr_t(setTextOf(words)))(words, parts.back().c_str());
    if (s.longTold++ < 10) LOG_INFO("chat: a long line (%zu bytes) sent as %zu", text.size(), parts.size());
}

uint32_t wordsEdit() {
    const uint32_t chat = gameChat(), e = chat ? read32(chat + kWordsEdit) : 0;
    return e && read32(e) == kWordsClass ? e : 0;
}

uint32_t nameEdit() {
    const uint32_t chat = gameChat(), e = chat ? read32(chat + kNameEdit) : 0;
    return e && read32(e) == kNameClass ? e : 0;
}

}  // namespace

bool inputEmpty() {
    const uint32_t words = wordsEdit();
    return !words || editText(words).empty();
}

bool focusInChat() {
    const uint32_t focus = read32(kFocus);
    return focus == 0 || focus == wordsEdit() || focus == nameEdit();
}

void clearLine() {
    const uint32_t words = wordsEdit();
    if (const uint32_t set = words ? setTextOf(words) : 0) reinterpret_cast<SetTextFn>(uintptr_t(set))(words, "");
}

bool gameBattle() { return read32(kBattle) != 0; }

namespace {

// 3.0.11-beta.241: the size by the corner - «width 640», «rows 10» (rows 0: F10's)
void readSize(State& s) {
    s.sizeRead = true;
    std::string text;
    if (!readTextFile(layout::join(gameDir(), layout::kChatState), text)) return;
    int w = 0, r = 0;
    const size_t wAt = text.find("width ");
    if (wAt != std::string::npos && std::sscanf(text.c_str() + wAt, "width %d", &w) == 1)
        s.width = std::clamp(w, cp::kLeastWidth, cp::kMostWidth);
    if (const size_t at = text.find("rows "); at != std::string::npos && std::sscanf(text.c_str() + at, "rows %d", &r))
        s.customRows = r == 0 ? 0 : std::clamp(r, cp::kLeastRows, cp::kMostRows);
    int hide = 0;  // 3.0.11-beta.243: «Усі»'s filters
    const size_t hAt = text.find("hide ");
    if (hAt != std::string::npos && std::sscanf(text.c_str() + hAt, "hide %d", &hide) == 1) s.show.hide = hide & 15;
}

void keepSize(const State& s) {
    char text[64];
    std::snprintf(text, sizeof text, "width %d\nrows %d\nhide %d\n", s.width, s.customRows, s.show.hide);
    const std::string path = layout::join(gameDir(), layout::kChatState);
    if (!writeFileAtomic(path, std::string(text))) LOG_INFO("chat: %s cannot be written", path.c_str());
}

void readInput(State& s) {
    cp::Input in;
    if (const uint32_t words = wordsEdit()) {
        const std::string raw = editText(words);
        in.text = utf8(s, raw);
        const uint32_t caret = read32(words + kCaretAt);
        if (caret <= raw.size()) in.caret = utf8(s, raw.substr(0, caret)).size();
        if (!raw.empty() && !s.inputTold) {
            s.inputTold = true;
            LOG_INFO("chat: the game's input read - %zu bytes, its caret at %u (+0x48 %u)", raw.size(), caret,
                     read32(words + 0x48));
        }
        const float px = 12 * s.layout.s, drawn = raw.size() >= 6 ? uaui::textWidth(in.text, px, false) : -1;
        if (drawn >= 0 && !s.widthTold) {  // 3.0.18-beta.216 (R-JPMWSD): what the caret stood off by (no words told)
            s.widthTold = true;
            LOG_INFO("chat: the input's %zu bytes %.0f px wide as uaUI draws them, %d px as GDI's whole line - the "
                     "caret by uaUI's",
                     raw.size(), double(drawn), wordsWidth(utf8Wide(in.text), px, false));
        }
    }
    if (const uint32_t name = nameEdit()) {
        in.whisper = utf8(s, editText(name));
        in.whisperFocus = read32(kFocus) == name;
    }
    const uint32_t mode = read32(kMode);
    in.mode = mode <= 2 ? int(mode) : 0;
    in.battle = read32(kBattle) != 0;
    in.typing = !in.battle || read32(kTyping) != 0;
    if (walkmodewin::on()) {  // 3.0.18-beta.208: the walk mode - typing after Enter, its own hint
        in.walk = true;
        in.typing = walkmodewin::typing();
    }
    in.caretOn = (GetTickCount64() / kBlinkMs) % 2 == 0;
    in.replyTo = s.replyTo;  // 3.0.11-beta.240
    s.input = in;
}

void giveKeys(uint32_t control) {
    if (control) reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, control);
}

// The whisper's name into the game's field (its own SetText, the field's words as the game's code page has them)
bool setName(const State& s, const std::string& utf8Name) {
    const uint32_t name = nameEdit();
    if (!name || read32(read32(name) + kSetTextSlot) != kSetText) return false;
    const std::string game = narrowed(utf8Wide(utf8Name), s.codePage);
    reinterpret_cast<SetTextFn>(uintptr_t(kSetText))(name, game.c_str());
    return true;
}

// The channel round: near, the party, the guild, a whisper (its name's field takes the keys); a whisper's name set
// - back near
void nextChannel(State& s) {
    const cp::Input& in = s.input;
    if (in.whisperFocus) {
        giveKeys(wordsEdit());
    } else if (!in.whisper.empty()) {
        setName(s, "");
        write32(kMode, 0);
        giveKeys(wordsEdit());
    } else if (in.mode < 2) {
        write32(kMode, uint32_t(in.mode + 1));
    } else {
        write32(kMode, 0);
        giveKeys(nameEdit());
    }
    s.key.clear();
}

std::string ownName(const State& s) { return utf8(s, readOwnName()); }

std::string actorName(const State& s, uint32_t id, uint32_t me) {
    if (id == me) return ownName(s);
    const int cls = targetpanel::actorClass(id);
    const auto index = reference::index();
    if (cls >= 0 && index) {
        const auto m = index->mobs.find(cls);
        if (m != index->mobs.end()) return m->second.name;
    }
    return "?";
}

// One of the character's skills by its id: its name's words (data\skillnametable.txt by its name in the session's
// list); "" none
std::string skillName(State& s, uint16_t id) {
    if (!id) return "";
    const auto known = s.skillNames.find(id);
    if (known != s.skillNames.end()) return known->second;
    std::string words;
    const uint32_t head = read32(kSkills);
    uint32_t node = head ? read32(head) : 0;
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        if (read32(node + 8 + 4) != id) continue;
        char buf[48] = {};
        const uint32_t at = read32(node + 8 + 0x1C);
        if (!at || !readGame(at, buf, sizeof buf - 1)) break;
        std::string aegis(buf, strnlen(buf, sizeof buf - 1));
        for (char& c : aegis)
            if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
        const auto& table = clienttables::skillNames();
        const auto w = table.find(aegis);
        words = w != table.end() ? hotbar::spaced(w->second) : aegis;
        break;
    }
    s.skillNames[id] = words;
    return words;
}

void battleHit(State& s, const packets::Hit& h, uint32_t me) {
    cp::Line l;
    l.kind = cp::Kind::Battle;
    l.ms = GetTickCount64();
    const bool mine = h.source == me;
    const std::string skill = mine ? skillName(s, h.skill) : "";
    if (!skill.empty())
        l.spans.push_back(cp::Span{skill, cp::kSystem, true, false});
    else
        l.spans.push_back(cp::Span{actorName(s, h.source, me), cp::kText, true, false});
    l.spans.push_back(cp::Span{" \xE2\x86\x92 ", cp::kMuted, false, false});  // →
    l.spans.push_back(cp::Span{actorName(s, h.target, me), cp::kText, true, false});
    if (h.damage > 0)
        l.spans.push_back(cp::Span{" " + cp::grouped(h.damage), mine ? cp::kSystem : cp::kRed, true, false});
    else
        l.spans.push_back(cp::Span{" " + say(settings::Text::ChatMiss), cp::kMuted, false, false});
    add(s, std::move(l));
}

// «{1} переможено»: the form's words round the name
void named(cp::Line& l, const std::string& form, const std::string& name, uint32_t nameRgb, int item = 0,
           int mob = 0) {
    const size_t at = form.find("{1}");
    const std::string before = at == std::string::npos ? "" : form.substr(0, at);
    const std::string after = at == std::string::npos ? " " + form : form.substr(at + 3);
    if (!before.empty()) l.spans.push_back(cp::Span{before, cp::kMuted, false, false});
    l.spans.push_back(cp::Span{name, nameRgb, true, false, item, mob});  // 3.0.11-beta.244: a link to «Довідник»
    if (!after.empty()) l.spans.push_back(cp::Span{after, cp::kMuted, false, false});
}

void killed(State& s, uint32_t id, uint32_t me) {
    cp::Line l;
    l.kind = cp::Kind::Loot;
    l.ms = GetTickCount64();
    const int cls = targetpanel::actorClass(id);  // 3.0.11-beta.244: a monster's - its page in «Довідник»
    named(l, say(settings::Text::ChatKilled), actorName(s, id, me), cp::kText, 0, cls >= 1001 && cls < 4000 ? cls : 0);
    add(s, std::move(l));
    s.killLine = &s.lines.back();
    s.killAt = GetTickCount64();
}

void picked(State& s, const packets::Pickup& p) {
    const auto& items = clienttables::itemNames();
    const auto it = items.find(std::to_string(p.item));
    const std::string name = it != items.end() ? utf8(s, hotbar::spaced(it->second)) : "#" + std::to_string(p.item);
    cp::Line l;
    l.kind = cp::Kind::Loot;
    l.ms = GetTickCount64();
    named(l, say(settings::Text::ChatPicked), name, cp::kText, p.item > 0 ? int(p.item) : 0);
    l.spans.push_back(cp::Span{" \xC3\x97" + std::to_string(p.amount), cp::kMuted, false, false});  // ×N
    add(s, std::move(l));
}

// The experience's growth: onto the kill just before it (· Base EXP +6 · Job EXP +2), else a line of its own
void experience(State& s, uint16_t type, uint32_t value) {
    if (!s.expKnown[type]) {
        s.expKnown[type] = true;
        s.exp[type] = value;
        return;
    }
    const long long grew = (long long)value - (long long)s.exp[type];
    s.exp[type] = value;
    if (grew <= 0) return;  // a level up starts it over
    if (s.serverExpAt && GetTickCount64() - s.serverExpAt < kServerExpMs) return;  // 3.0.12-beta.3: told by it
    const std::string words = (type == 1 ? "Base EXP +" : "Job EXP +") + cp::grouped(grew);
    const uint32_t rgb = type == 1 ? cp::kBaseExp : cp::kJobExp;
    if (s.killLine && GetTickCount64() - s.killAt < kKillExpMs) {
        s.killLine->spans.push_back(cp::Span{" \xC2\xB7 ", cp::kMuted, false, false});
        s.killLine->spans.push_back(cp::Span{words, rgb, true, false});
        ++s.version;
        return;
    }
    cp::Line l;
    l.kind = cp::Kind::Loot;
    l.ms = GetTickCount64();
    l.spans.push_back(cp::Span{words, rgb, true, false});
    add(s, std::move(l));
}

void render(State& s) {
    ++s.sheetGen;
}

// The tab's lines as rows, made again when a line came or changed, the tab or the width changed
void makeRows(State& s) {
    const std::string folded = s.searching ? settings::foldCase(s.search) : std::string();  // 3.0.11-beta.284
    const std::string key = std::to_string(s.version) + ';' + std::to_string(int(s.show.tab)) + s.show.person + 'h' +
                            std::to_string(s.show.hide) + ';' + folded + ';' +
                            std::to_string(int(s.layout.lines.w)) + ';' + std::to_string(s.layout.px);
    if (key == s.rowsKey) return;
    s.rowsKey = key;
    s.rowsVersion = s.version;
    s.rowLines.clear();
    if (s.show.tab == cp::Tab::Battle) {  // the battle's and the kills' and pick-ups' by their time
        size_t a = 0, b = 0;
        while (a < s.battle.size() || b < s.lines.size()) {
            if (b < s.lines.size() && !cp::shows(cp::Tab::Battle, s.lines[b])) {
                ++b;
                continue;
            }
            if (b >= s.lines.size() || (a < s.battle.size() && s.battle[a].ms <= s.lines[b].ms))
                s.rowLines.push_back(&s.battle[a++]);
            else
                s.rowLines.push_back(&s.lines[b++]);
        }
    } else {
        for (const cp::Line& l : s.lines)
            if (cp::showsWith(s.show.tab, s.show.person, l, s.show.hide)) s.rowLines.push_back(&l);  // .242, .243
    }
    if (!folded.empty())  // 3.0.11-beta.284: the search's - the lines with its words
        s.rowLines.erase(std::remove_if(s.rowLines.begin(), s.rowLines.end(),
                                        [&](const cp::Line* l) { return !cp::matches(*l, folded); }),
                         s.rowLines.end());
    if (s.rowLines.size() > 200) s.rowLines.erase(s.rowLines.begin(), s.rowLines.end() - 200);
    s.rows = s.wrapCache.wrap(s.rowLines, s.layout.lines.w, s.layout.px, measure, uistyle::generation());
}

void block(const State& s, HWND game) {  // the mouse's clicks over it are its own, not the world's
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r = clientBox(client, s.targetW, s.targetH, x0, y0, b.w, b.h);
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Chat, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Chat, nullptr, nullptr, false);
    s.shown = false;
}

int mostScroll(const State& s) { return std::max(0, int(s.rows.size()) - s.layout.rows); }

}  // namespace

void install() {
    static bool done = false;
    if (done) return;
    done = true;
    auto* at = reinterpret_cast<uint8_t*>(uintptr_t(kPush));
    uint8_t now[sizeof kPushCode] = {};
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), at, now, sizeof now, &got) || got != sizeof now ||
        std::memcmp(now, kPushCode, sizeof now) != 0) {
        LOG_INFO("chat: the game's lines are not where this build of uaRO.exe has them - the new chat stays empty");
        return;
    }
    auto* t = static_cast<uint8_t*>(VirtualAlloc(nullptr, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!t) return;
    std::memcpy(t, kPushCode, sizeof kPushCode);  // its first instructions, then on to the rest
    t[sizeof kPushCode] = 0xE9;
    const int32_t back = int32_t(uintptr_t(kPush) + sizeof kPushCode -
                                 (reinterpret_cast<uintptr_t>(t) + sizeof kPushCode + 5));
    std::memcpy(t + sizeof kPushCode + 1, &back, 4);
    FlushInstructionCache(GetCurrentProcess(), t, 16);
    g_push = reinterpret_cast<uintptr_t>(t);
    DWORD old = 0;
    if (!VirtualProtect(at, sizeof kPushCode, PAGE_EXECUTE_READWRITE, &old)) return;
    uint8_t jump[sizeof kPushCode];
    jump[0] = 0xE9;
    const int32_t to = int32_t(reinterpret_cast<uintptr_t>(&pushed) - (uintptr_t(kPush) + 5));
    std::memcpy(jump + 1, &to, 4);
    std::memcpy(at, jump, sizeof jump);
    DWORD unused = 0;
    VirtualProtect(at, sizeof kPushCode, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), at, sizeof kPushCode);
    LOG_INFO("chat: the game's lines read as it pushes them into its chat (uaRO.exe 0x%X)", kPush);
}

void packet(const packets::Event& e) {
    State& s = state();
    const uint32_t me = read32(kOwnAccount);
    if (!me) return;
    if (const auto* h = std::get_if<packets::Hit>(&e)) {
        if (h->source != me && h->target != me) return;
        if (h->source == me && h->target != me) s.hitAt[h->target] = GetTickCount64();
        battleHit(s, *h, me);
    } else if (const auto* v = std::get_if<packets::Vanish>(&e)) {
        const auto it = s.hitAt.find(v->id);
        if (it == s.hitAt.end()) return;
        const bool fresh = GetTickCount64() - it->second < kHitKeepMs;
        s.hitAt.erase(it);
        if (v->type == 1 && fresh) killed(s, v->id, me);
    } else if (const auto* p = std::get_if<packets::Pickup>(&e)) {
        if (p->ok && p->amount > 0) picked(s, *p);
    } else if (const auto* x = std::get_if<packets::StatusLong>(&e)) {
        if (x->type == 1 || x->type == 2) experience(s, x->type, x->value);
    }
}

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    take(s);  // the lines kept with the chat off too: on, it shows them
    partMark("chat take");  // 3.0.18-beta.171: its part split
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("chat: [interface] chat = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    if (world) holdOldChat(s, on);
    if (!on || !world || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    // 3.0.11-beta.67: [interface] text_size's too; 3.0.11-beta.75: its size in «Розміщення інтерфейсу»
    const float scale = uistyle::readingScale(k) * hudedit::sizeOf(hudlayout::Part::Chat);
    s.k = float(k);
    s.scale = scale;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    readInput(s);
    partMark("chat input");
    // 3.0.11-beta.240: a command's words sent (the field empty again) - its channel and whisper put back
    if (s.restoreOn && (s.input.text.empty() || GetTickCount64() - s.restoreAt > 3000)) {
        s.restoreOn = false;
        write32(kMode, uint32_t(s.restoreMode));
        setName(s, s.restoreName);
    }
    if (s.searching) {  // 3.0.11-beta.284: the field shows the search's words
        if (wordsEdit() == 0 || read32(kFocus) != wordsEdit()) {
            s.searching = false;
            s.search.clear();
            s.key.clear();
            LOG_INFO("chat: the search closed (the field left)");
        } else {
            s.input.text = s.search;
            s.input.caret = std::string::npos;
            s.input.whisper.clear();
            s.input.whisperFocus = false;
            s.input.searching = true;
        }
    }
    const ULONGLONG now = GetTickCount64();
    int cx = 0, cy = 0;
    cp::Hit hit;
    if (readHoverCursor(cx, cy) && s.layout.w > 0)
        hit = cp::hitTest(s.layout, s.rows, s.show, float(cx) * float(k) - float(s.layout.x),
                          float(cy) * float(k) - float(s.layout.y));
    const bool over = hit.part != cp::Part::None;
    partMark("chat hover");
    const bool busy = !s.input.text.empty() || s.input.whisperFocus || (s.input.battle && s.input.typing);
    if (s.folded && !busy) {  // Alt+F10: out of sight until one types
        hide(s);
        return;
    }
    // 3.0.11-beta.240: [interface] chat_fade - its ground fades after this long idle; 0 never
    const bool quiet = s.fadeMs > 0 && !busy && !over && s.show.scroll == 0 && now - s.lastLine > s.fadeMs;
    s.show.hoverTab = hit.part == cp::Part::Tab ? hit.index : -1;
    s.show.hoverLine = hit.part == cp::Part::Name ? hit.index : -1;
    s.show.hoverMode = hit.part == cp::Part::Mode;
    s.show.hoverClear = hit.part == cp::Part::Clear;
    s.show.hoverNewer = hit.part == cp::Part::Newer;
    s.show.hoverPerson = hit.part == cp::Part::Person ? hit.index : -1;  // 3.0.11-beta.242
    s.show.hoverItem = hit.part == cp::Part::Item ? hit.index : -1;
    s.show.hoverLink = hit.part == cp::Part::Link ? hit.index : 0;  // 3.0.11-beta.244
    s.show.hoverBar = hit.part == cp::Part::Bar || s.barDrag;      // 3.0.11-beta.245
    s.show.hoverFilters = hit.part == cp::Part::Filters;  // 3.0.11-beta.243
    s.show.hoverSearch = hit.part == cp::Part::Search;    // 3.0.18-beta.105
    s.show.hoverFilter = hit.part == cp::Part::Filter ? hit.index : -1;
    s.show.hoverRowLine = hit.part == cp::Part::Lines || hit.part == cp::Part::Name ? hit.index : -1;
    {
        SYSTEMTIME st{};
        GetLocalTime(&st);
        s.show.clockSec = st.wHour * 3600 + st.wMinute * 60 + st.wSecond;
        s.show.nowMs = GetTickCount64();
    }
    s.show.peopleUnread.clear();
    for (const std::string& p : s.show.people) s.show.peopleUnread.push_back(s.personUnread[p]);
    if (!s.sizeRead) readSize(s);
    s.show.hoverGrip = hit.part == cp::Part::Grip || s.sizing;
    s.layout = cp::layout(int(std::lround(10 * scale)), targetHeight - int(std::lround(10 * scale)), scale,
                          s.customRows > 0 ? s.customRows : kHeights[s.height % 3], quiet && !s.sizing, s.show,
                          s.input, texts(), measure, s.width);
    hudedit::place(hudlayout::Part::Chat, float(k), targetWidth, targetHeight, s.layout.w, s.layout.h, s.layout.x,
                   s.layout.y);  // 3.0.11-beta.75: where the player put it
    partMark("chat layout");
    makeRows(s);
    partMark("chat rows");
    s.show.lineTimes.clear();  // 3.0.11-beta.243: the rows' lines' times (the one pointed at shows its own)
    if (s.rowsVersion == s.version)
        for (const cp::Line* l : s.rowLines) s.show.lineTimes.push_back(l->ms);
    s.show.scroll = std::min(s.show.scroll, mostScroll(s));
    const cp::Input& in = s.input;
    const bool caret = !quiet && in.caretOn && (in.whisperFocus || (in.typing && !in.text.empty()));
    std::string key = std::to_string(s.version) + ';' + s.rowsKey + ';' + std::to_string(s.layout.w) + 'x' +
                      std::to_string(s.layout.h) + ';' + std::to_string(scale) + ';' + (quiet ? "q" : "") +
                      std::to_string(s.show.scroll) + ';' + std::to_string(s.show.hoverTab) + ',' +
                      std::to_string(s.show.hoverLine) + (s.show.hoverMode ? "m" : "") +
                      (s.show.hoverClear ? "c" : "") + (s.show.hoverNewer ? "n" : "") + ';';
    for (int u : s.show.unread) key += std::to_string(u) + ',';
    key += std::to_string(s.show.hoverRowLine) +
           (s.show.hoverRowLine >= 0 ? std::to_string(s.show.clockSec / 60) : std::string()) +
           (s.show.filtersOpen ? "F" : "") + (s.show.hoverFilters ? "f" : "") + (s.show.hoverSearch ? "q" : "") +
           std::to_string(s.show.hoverFilter) +
           ';';  // 3.0.11-beta.243
    // 3.0.11-beta.242: the whispers' row, the one chosen, a name's menu; 3.0.11-beta.241: the grip pointed at
    key += ';' + s.show.person + '|' + std::to_string(s.show.hoverPerson) + ',' + std::to_string(s.show.menuLine) +
           ',' + std::to_string(s.show.hoverItem) + (s.show.hoverGrip ? "g" : "") + (s.show.menuNamed ? "N" : "") +
           std::to_string(s.show.hoverLink) + (s.show.hoverBar ? "b" : "") + ';';
    for (size_t i = 0; i < s.show.people.size(); ++i)
        key += s.show.people[i] + ':' + std::to_string(s.show.peopleUnread[i]) + ',';
    key += ';' + in.text + '|' + std::to_string(in.caret) + '|' + in.whisper + '|' + std::to_string(in.mode) +
           (in.whisperFocus ? "f" : "") + (in.battle ? "b" : "") + (in.walk ? "w" : "") + (in.typing ? "t" : "") +
           (caret ? "|" : "");
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()) + (uaui::ready() ? "u" : "");
        drawn != s.key) {  // its grounds (3.0.18-beta.32: uaUI's or the picture)
        s.key = drawn;
        render(s);
        partMark("chat key, drawn anew");
    } else {
        partMark("chat key");
    }
    s.shown = true;
    block(s, game);
    partMark("chat block");
}

bool panelBox(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.shown) return false;
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

// 3.0.18-beta.32 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the chat in RmlUi's
// markup, line by line as core chatpanel paints it - its glass (fainter while quiet), the tabs (the shown one's gold
// line, the unread badges), the input, the channel's chip and chevron, ✕, the caret, «нові ↓», the hints' card, the
// whispers' people chips, the corner's grip, the scroll bar, the filters' gear and menu, a name's menu, the name or
// link pointed at underlined; the words where core chatpanel places them. A part of the screen (not a window)
static void fillSheet(State& s, uisheet::Content& c) {
    const cp::Layout& L = s.layout;
    const cp::Show& sh = s.show;
    const cp::Input& in = s.input;
    const cp::Texts t = texts();
    const float sc = L.s;
    constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kGold = 0xF3C653, kAccent = 0x5DB2FF;
    constexpr uint32_t kWhisper = cp::kWhisper, kMuted = cp::kMuted;
    const cp::View v = cp::view(L, s.rows, in, sh, t, measure);
    auto& u = c.under;
    const auto icon = [&](uiicons::Icon which, const panel::Box& b, uint32_t rgb, float alpha, float stroke) {
        const int px = std::max(4, int(std::ceil(b.w)));
        const std::string key = "u" + std::to_string(int(which)) + ':' + std::to_string(px) + ':' +
                                std::to_string(rgb);
        const std::string src = s.sheet.made(key, px, px, [&](panel::Picture& p) {
            uiicons::draw(p, which, 0, 0, float(px), rgb, alpha, stroke);
        });
        c.icons.push_back(uisheet::Icon{b.x, b.y, float(px), float(px), 1, src});
    };
    c.w = float(L.w), c.h = float(L.h);
    c.op = L.quiet ? 0.72f : 1.0f;
    c.rect(u, L.glass, 10 * sc, kCard, L.quiet ? uistyle::chat() * 0.45f : uistyle::chat(), kWhite,
           L.quiet ? 0.04f : 0.09f, 1.0f * sc);
    for (size_t i = 0; i < L.tabs.size(); ++i) {
        const panel::Box& b = L.tabs[i];
        const bool on = int(i) == int(sh.tab);
        if (on || int(i) == sh.hoverTab) c.rect(u, b, 7 * sc, kWhite, on ? 0.07f : 0.04f);
        if (on) c.rect(u, panel::Box{b.x + 9 * sc, b.y + b.h - 3 * sc, b.w - 18 * sc, 2 * sc}, 1 * sc, kGold, 0.95f);
        if (i < v.badges.size()) c.rect(u, v.badges[i], v.badges[i].h / 2, kAccent, 0.95f);
    }
    c.rect(u, panel::Box{0, L.tabsRow.h, L.glass.w, 1 * sc}, 0, kWhite, 0.06f);
    c.rect(u, L.input, 8 * sc, kBlack, 0.38f, kWhite, 0.10f, 1.0f * sc);
    c.rect(u, L.mode, 6 * sc, v.chipRgb, sh.hoverMode ? 0.28f : 0.16f);
    if (v.chevron.w > 0) icon(uiicons::Icon::Down, v.chevron, v.chipRgb, 0.9f, 2.4f);
    if (L.clear.w > 0 && sh.hoverClear) c.rect(u, L.clear, 5 * sc, kWhite, 0.14f);
    c.rect(u, v.caret, 0, v.caretRgb, 0.95f);
    if (L.newer.w > 0) c.rect(u, L.newer, 6 * sc, kAccent, sh.hoverNewer ? 0.45f : 0.28f);
    if (L.hints.w > 0) c.rect(u, L.hints, 8 * sc, kCard, 0.96f, kWhite, 0.14f, 1.0f * sc);
    for (size_t i = 0; i < L.people.size(); ++i) {
        const bool on = i == 0 ? sh.person.empty() : sh.person == sh.people[i - 1];
        c.rect(u, L.people[i], L.people[i].h / 2, on ? kWhisper : kWhite,
               on ? 0.22f : int(i) == sh.hoverPerson ? 0.10f : 0.05f, on ? kWhisper : kWhite, on ? 0.7f : 0.12f,
               1.0f * sc);
    }
    const float a = sh.hoverGrip ? 0.8f : L.quiet ? 0.15f : 0.35f;  // the corner's grip: two short slants
    for (int k = 0; k < 2; ++k) {
        const float d = (5 + 4 * float(k)) * sc;
        for (float t2 = 0; t2 < d; t2 += 1.0f * sc)
            c.rect(u, panel::Box{L.grip.x + L.grip.w - 4 * sc - d + t2, L.grip.y + 4 * sc + t2, 1.2f * sc, 1.2f * sc},
                   0, kWhite, a);
    }
    if (v.thumb.w > 0) {
        c.rect(u, L.bar, L.bar.w / 2, kWhite, 0.05f);
        c.rect(u, v.thumb, v.thumb.w / 2, kWhite, sh.hoverBar ? 0.45f : 0.25f);
    }
    if (L.filtersButton.w > 0) {
        const panel::Box& b = L.filtersButton;
        if (sh.hoverFilters || sh.filtersOpen) c.rect(u, b, 6 * sc, kWhite, 0.10f);
        const float side = 14 * sc;
        icon(uiicons::Icon::Gear, panel::Box{b.x + (b.w - side) / 2, b.y + (b.h - side) / 2, side, side},
             sh.hide ? kGold : kMuted, 1.0f, 2.0f);
    }
    if (L.searchButton.w > 0) {  // 3.0.18-beta.105: the search's magnifier (gold while it searches)
        const panel::Box& b = L.searchButton;
        if (sh.hoverSearch || s.searching) c.rect(u, b, 6 * sc, kWhite, s.searching ? 0.14f : 0.10f);
        const float side = 13 * sc;
        icon(uiicons::Icon::Search, panel::Box{b.x + (b.w - side) / 2, b.y + (b.h - side) / 2, side, side},
             s.searching ? kGold : kMuted, 1.0f, 2.0f);
    }
    auto& o = c.over;
    if (L.filtersMenu.w > 0) {
        c.rect(o, L.filtersMenu, 8 * sc, kCard, 0.97f, kWhite, 0.16f, 1.0f * sc);
        for (size_t i = 0; i < L.filters.size(); ++i) {
            const panel::Box& f = L.filters[i];
            if (int(i) == sh.hoverFilter) c.rect(o, f, 6 * sc, kWhite, 0.08f);
            const bool on = !(sh.hide & (1 << i));
            c.rect(o, panel::Box{f.x + 6 * sc, f.y + (f.h - 12 * sc) / 2, 12 * sc, 12 * sc}, 3 * sc,
                   on ? kGold : kWhite, on ? 0.9f : 0.05f, on ? kGold : kWhite, 0.6f, 1.0f * sc);
        }
    }
    if (L.menu.w > 0) {
        c.rect(o, L.menu, 8 * sc, kCard, 0.97f, kWhite, 0.16f, 1.0f * sc);
        if (sh.hoverItem >= 0 && sh.hoverItem < int(L.items.size()))
            c.rect(o, L.items[size_t(sh.hoverItem)], 6 * sc, kWhite, 0.10f);
    }
    for (const cp::Underline& x : v.underlines) c.rect(o, x.box, 0, x.rgb, 0.8f);
    c.say(cp::words(L, s.rows, in, sh, t, measure));
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    s.sheetShown = s.sheet.frame("chat", s.shown, false, s.layout.x, s.layout.y, s.sheetGen,
                                 [&](uisheet::Content& c) { fillSheet(s, c); });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    (void)record;  // the old edition (its picture) gone: uaUI draws it
    (void)renderer;
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = mouseButton(m);
    if (!button && m != WM_MOUSEWHEEL && !(m == WM_MOUSEMOVE && (s.sizing || s.barDrag))) return false;
    float tx = 0, ty = 0;  // 3.0.11-beta.43: where the game's cursor stands, as the hover
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty)) return false;
    if (s.barDrag) {  // 3.0.11-beta.245: the scroll bar's thumb dragged - the history follows
        s.show.scroll = std::clamp(cp::scrollAt(s.layout, s.rows.size(), ty - float(s.layout.y)), 0, mostScroll(s));
        if (m == WM_LBUTTONUP) s.barDrag = false;
        s.key.clear();
        result = 0;
        return true;
    }
    if (s.sizing) {  // 3.0.11-beta.241: its corner dragged - the width and the rows follow, kept when let go
        cp::sizeTo(s.layout, tx, ty, s.width, s.customRows);
        if (m == WM_LBUTTONUP) {
            s.sizing = false;
            keepSize(s);
            LOG_INFO("chat: its size by its corner - %d wide, %d rows", s.width, s.customRows);
        }
        s.key.clear();
        result = 0;
        return true;
    }
    const cp::Hit h = cp::hitTest(s.layout, s.rows, s.show, tx - float(s.layout.x), ty - float(s.layout.y));
    if (h.part == cp::Part::None) {
        if (m == WM_LBUTTONDOWN && s.show.menuLine >= 0) {  // 3.0.11-beta.242: a press away closes a name's menu
            s.show.menuLine = -1;
            s.key.clear();
        }
        return false;
    }
    result = 0;
    if (m == WM_MOUSEWHEEL) {  // three rows a notch, up into the history
        const int by = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 3 : -3;
        s.show.scroll = std::clamp(s.show.scroll + by, 0, mostScroll(s));
        s.key.clear();
        return true;
    }
    // 3.0.11-beta.242: a right click on a name - its menu; a press elsewhere closes it
    // 3.0.11-beta.244: on another part of a line - its own menu (copy it)
    if (m == WM_RBUTTONDOWN && (h.part == cp::Part::Name || h.part == cp::Part::Lines) && h.index >= 0) {
        s.show.menuNamed = h.part == cp::Part::Name;
        s.show.menuLine = h.index;
        s.show.menuX = tx - float(s.layout.x);
        s.show.menuY = ty - float(s.layout.y);
        s.key.clear();
        return true;
    }
    if (m != WM_LBUTTONDOWN) return true;
    const int menuLine = s.show.menuLine;
    s.show.menuLine = -1;
    if (h.part != cp::Part::Filter && h.part != cp::Part::Filters) s.show.filtersOpen = false;  // 3.0.11-beta.243
    const char* did = "";
    switch (h.part) {
        case cp::Part::Bar:  // 3.0.11-beta.245: the scroll bar - to where it was pressed, dragged on
            s.barDrag = true;
            s.show.scroll = std::clamp(cp::scrollAt(s.layout, s.rows.size(), ty - float(s.layout.y)), 0, mostScroll(s));
            did = "the scroll bar";
            break;
        case cp::Part::Link:  // 3.0.11-beta.244: a thing or a monster in «Довідник»
            if (h.index > 0)
                referencewin::openItem(h.index);
            else if (h.index < 0)
                referencewin::openMob(-h.index);
            did = "a link to «Довідник»";
            break;
        case cp::Part::Search:  // 3.0.18-beta.105: the magnifier - the search opened (its words typed) or closed
            s.searching = !s.searching;
            s.search.clear();
            s.key.clear();
            LOG_INFO("chat: the search %s (its magnifier)", s.searching ? "opened" : "closed");
            break;
        case cp::Part::Filters:  // 3.0.11-beta.243: «Усі»'s filters' menu
            s.show.filtersOpen = !s.show.filtersOpen;
            did = "the filters";
            break;
        case cp::Part::Filter:
            if (h.index >= 0 && h.index < 4) {
                s.show.hide ^= 1 << h.index;
                keepSize(s);
                did = "a filter";
            }
            break;
        case cp::Part::Person:  // 3.0.11-beta.242: one's whispers alone (0: all), the input to them
            s.show.person = h.index <= 0 || size_t(h.index - 1) >= s.show.people.size()
                                ? std::string()
                                : s.show.people[size_t(h.index - 1)];
            s.show.scroll = 0;
            if (!s.show.person.empty()) {
                s.personUnread[s.show.person] = 0;
                setName(s, s.show.person);
                giveKeys(wordsEdit());
            }
            did = s.show.person.empty() ? "all the whispers" : "one's whispers";
            break;
        case cp::Part::Item:  // 3.0.11-beta.242: a name's menu
            if (s.rowsVersion == s.version && menuLine >= 0 && size_t(menuLine) < s.rowLines.size() &&
                (cp::menuAction(s.show, h.index) == 3 || !s.rowLines[size_t(menuLine)]->who.empty())) {
                const std::string who = s.rowLines[size_t(menuLine)]->who;
                const int action = cp::menuAction(s.show, h.index);  // 3.0.11-beta.244
                if (action == 0) {  // whisper
                    setName(s, who);
                    giveKeys(wordsEdit());
                    did = "the menu - a whisper";
                } else if (action == 1) {  // the game's own «/ex Name» in the field, the player's Enter sends it
                    const uint32_t words = wordsEdit();
                    if (words && setTextOf(words)) {
                        setName(s, "");
                        const std::string game = narrowed(utf8Wide("/ex " + who), s.codePage);
                        reinterpret_cast<SetTextFn>(uintptr_t(setTextOf(words)))(words, game.c_str());
                        giveKeys(words);
                    }
                    did = "the menu - «/ex» put in the field";
                } else {  // the name (3.0.11-beta.244: or the whole line) to the clipboard
                    std::string text = who;
                    if (action == 3) {
                        text.clear();
                        for (const cp::Span& sp : s.rowLines[size_t(menuLine)]->spans) text += sp.text;
                    }
                    const std::wstring wide = utf8Wide(text);
                    if (OpenClipboard(game)) {
                        EmptyClipboard();
                        if (HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (wide.size() + 1) * sizeof(wchar_t))) {
                            if (void* p = GlobalLock(g)) {
                                std::memcpy(p, wide.c_str(), (wide.size() + 1) * sizeof(wchar_t));
                                GlobalUnlock(g);
                                if (!SetClipboardData(CF_UNICODETEXT, g)) GlobalFree(g);
                            } else {
                                GlobalFree(g);
                            }
                        }
                        CloseClipboard();
                    }
                    did = action == 3 ? "the menu - the line copied" : "the menu - the name copied";
                }
            }
            break;
        case cp::Part::Grip:  // 3.0.11-beta.241
            s.sizing = true;
            did = "its corner taken";
            break;
        case cp::Part::Tab:
            s.show.tab = cp::Tab(h.index);
            s.show.unread[h.index] = 0;
            s.show.scroll = 0;
            s.lastLine = GetTickCount64();
            did = "a tab opened";
            break;
        case cp::Part::Name:
            if (s.rowsVersion == s.version && h.index >= 0 && size_t(h.index) < s.rowLines.size() &&
                !s.rowLines[size_t(h.index)]->who.empty()) {
                const bool set = setName(s, s.rowLines[size_t(h.index)]->who);
                giveKeys(wordsEdit());
                did = set ? "a name clicked - the whisper goes to it" : "a name clicked - the game's field not found";
            }
            break;
        case cp::Part::Mode:
            if (!s.input.whisper.empty()) {
                giveKeys(nameEdit());
                did = "the whisper's name to change";
            } else {
                nextChannel(s);
                did = "the channel round";
            }
            break;
        case cp::Part::Clear:
            setName(s, "");
            giveKeys(wordsEdit());
            did = "the whisper's name cleared";
            break;
        case cp::Part::Field:
            giveKeys(wordsEdit());
            did = "the words' field given the keys";
            break;
        case cp::Part::Newer:
            s.show.scroll = 0;
            did = "back to the newest";
            break;
        default:
            break;
    }
    if (*did && s.clicksTold++ < 12) LOG_INFO("chat: %s", did);
    s.key.clear();
    return true;
}

// 3.0.11-beta.284 (the owner's feature 17): Ctrl+F in the chat's field - the search: the words typed (the game's
// field keeps its own), the tab's lines with them; Backspace, Esc or Enter (closes), Ctrl+F again closes. Ctrl+F
// elsewhere stays «Довідник»'s
bool searchKey(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    (void)lp;
    if ((m == WM_CHAR || m == WM_SYSCHAR) && s.skipChar && wp == s.skipChar) {
        s.skipChar = 0;
        return true;
    }
    if (!s.on || !s.shown || !inGameWorld()) return false;
    const uint32_t words = wordsEdit();
    // 3.0.16-beta.4 (a player: «щось у мене перестав довидник откриватись ... жму контр F» - the game keeps its chat
    // field focused nearly always, so Ctrl+F was the chat's search and «Довідник» never came): the chat's search only
    // with words typed in its field; else Ctrl+F is «Довідник»'s
    const bool inField = words && read32(kFocus) == words && !editText(words).empty();
    const bool ctrl = GetKeyState(VK_CONTROL) < 0, alt = GetKeyState(VK_MENU) < 0;
    const auto close = [&](const char* why) {
        s.searching = false;
        s.search.clear();
        s.key.clear();
        LOG_INFO("chat: the search closed (%s)", why);
    };
    if (m == WM_KEYDOWN && wp == 'F' && ctrl && !alt && (inField || s.searching)) {
        if (s.searching) {
            close("Ctrl+F");
        } else {
            s.searching = true;
            s.search.clear();
            s.key.clear();
            LOG_INFO("chat: the search opened (Ctrl+F in the field)");
        }
        s.skipChar = 6;
        return true;
    }
    if (!s.searching) return false;
    if (m == WM_KEYDOWN) {
        if (wp == VK_ESCAPE || wp == VK_RETURN) {
            close(wp == VK_ESCAPE ? "Esc" : "Enter");
            s.skipChar = wp == VK_ESCAPE ? 27 : 13;
            return true;
        }
        if (wp == VK_BACK) {
            while (!s.search.empty() && (uint8_t(s.search.back()) & 0xC0) == 0x80) s.search.pop_back();
            if (!s.search.empty()) s.search.pop_back();
            s.skipChar = 8;
            s.key.clear();
            return true;
        }
        return wp != VK_CONTROL && wp != VK_SHIFT && wp != VK_MENU && !ctrl;  // the game's keys held back
    }
    if (m == WM_CHAR && wp >= 32 && !ctrl) {
        const char c = char(wp);
        s.search += utf8(s, std::string(1, c));
        if (s.search.size() > 60) s.search.resize(60);
        s.key.clear();
        return true;
    }
    return false;
}

bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    (void)lp;
    if (m == WM_CHAR && wp == '\t' && s.swallowTab) {  // the Tab the channel took: not into the game's field
        s.swallowTab = false;
        return true;
    }
    if (m == WM_CHAR && s.swallowChar && wp == s.swallowChar) {
        s.swallowChar = 0;
        return true;
    }
    // 3.0.18-beta.84 (the owner's «свой ввод текста» for the chat's line): the game's field keeps the line (its
    // sending, its IME, the battle mode's guard) with the new interface's fields' clipboard - Ctrl+V pastes one line at
    // the end (in the game's code page, as much as the field takes), Ctrl+C copies the line, Ctrl+X cuts it
    if (m == WM_KEYDOWN && GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_MENU) >= 0 &&
        (wp == 'V' || wp == 'C' || wp == 'X') && s.on && inGameWorld()) {
        const uint32_t words = wordsEdit();
        if (words && read32(kFocus) == words && setTextOf(words)) {
            const std::string now = editText(words);
            if (wp == 'V') {
                const std::string paste =
                    narrowed(widened(textfield::oneLine(uifield::clipboardText()), CP_UTF8), s.codePage);
                const size_t most = std::max<size_t>(1, read32(words + kEditMost));
                std::string line = now + paste;
                if (line.size() > most) line.resize(most);
                reinterpret_cast<SetTextFn>(uintptr_t(setTextOf(words)))(words, line.c_str());
                if (s.pasteTold++ < 10)
                    LOG_INFO("chat: Ctrl+V - %zu letters pasted into the line", line.size() - now.size());
            } else if (!now.empty()) {
                uifield::setClipboard(utf8(s, now));
                if (wp == 'X') reinterpret_cast<SetTextFn>(uintptr_t(setTextOf(words)))(words, "");
            }
            s.swallowChar = wp == 'V' ? 0x16 : wp == 'C' ? 0x03 : 0x18;
            return true;
        }
    }
    if (!s.on || !inGameWorld()) return false;
    const bool down = m == WM_KEYDOWN || m == WM_SYSKEYDOWN;
    if (!down) return false;
    if (wp == VK_F10) {  // the game's own chat out of sight: F10 the new one's height, Alt+F10 away until one types
        if (GetKeyState(VK_MENU) < 0)
            s.folded = !s.folded;
        else {
            s.height = (s.height + 1) % 3;
            s.customRows = 0;  // 3.0.11-beta.241: F10's heights over the corner's rows
            keepSize(s);
            s.folded = false;
        }
        s.key.clear();
        LOG_INFO("chat: %s", s.folded ? "folded away (Alt+F10)" : "its height changed (F10)");
        return true;
    }
    // 3.0.11-beta.240 (the chat concept, stage 1): Enter on «/p», «/g», «/w Ім'я», «/r» - the words alone into the
    // game's field, its channel (or whisper's name) set for them; the game sends as its own, the channel put back after
    if (wp == VK_RETURN && m == WM_KEYDOWN && s.shown) {
        const uint32_t words = wordsEdit();
        if (words && read32(kFocus) == words) {
            cp::Command c;
            if (cp::command(utf8(s, editText(words)), s.replyTo, c) &&
                setTextOf(words)) {
                if (!s.restoreOn) {
                    s.restoreMode = s.input.mode;
                    s.restoreName = s.input.whisper;
                }
                const std::string game = narrowed(utf8Wide(c.words), s.codePage);
                reinterpret_cast<SetTextFn>(uintptr_t(setTextOf(words)))(words, game.c_str());
                if (!c.whisper.empty()) {
                    setName(s, c.whisper);
                } else {
                    setName(s, "");
                    write32(kMode, uint32_t(c.mode));
                }
                s.restoreOn = true;
                s.restoreAt = GetTickCount64();
                if (s.commandsTold++ < 10)
                    LOG_INFO("chat: a command - %s", !c.whisper.empty() ? "a whisper" : c.mode == 1 ? "the party"
                                                                                                   : "the guild");
            }
        }
        if (s.longLines && words && read32(kFocus) == words) sendLong(s, words);  // 3.0.13-beta.12
        return false;  // the game's Enter sends
    }
    if (wp == VK_TAB && m == WM_KEYDOWN && GetKeyState(VK_CONTROL) >= 0 && GetKeyState(VK_MENU) >= 0 && s.shown) {
        const uint32_t words = wordsEdit();
        if (!words || read32(kFocus) != words) return false;  // the name's field: the game's own Tab
        nextChannel(s);
        s.swallowTab = true;
        return true;
    }
    return false;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) holdOldChat(s, true);
}

void padChannel() {
    State& s = state();
    if (!s.shown) return;
    nextChannel(s);
    s.key.clear();
}

void padScroll(int by) {
    State& s = state();
    if (!s.shown || by == 0) return;
    s.show.scroll = std::clamp(s.show.scroll + by, 0, mostScroll(s));
    s.key.clear();
}

void setColours(const std::string& party, const std::string& guild, const std::string& own,
                const std::string& others) {  // 3.0.13-beta.3
    State& s = state();
    const std::string key = party + ',' + guild + ',' + own + ',' + others;
    if (key == s.coloursKey) return;
    s.coloursKey = key;
    s.party = cp::chatColour(party, cp::kParty);
    s.guild = cp::chatColour(guild, cp::kGuild);
    s.own = cp::chatColour(own, 0);
    s.others = cp::chatColour(others, 0);
    LOG_INFO("chat: the colours - party %s, guild %s, own %s, others %s", party.c_str(), guild.c_str(), own.c_str(),
             others.c_str());
}

void setLongLines(bool on) {  // 3.0.13-beta.12: the field's most each frame
    State& s = state();
    s.longLines = on;
    const uint32_t words = wordsEdit();
    if (!words) return;
    const uint32_t want = on ? uint32_t(cp::kChatLongest) : 70u, now = read32(words + kEditMost);
    if ((now == 70u || now == uint32_t(cp::kChatLongest)) && now != want) write32(words + kEditMost, want);
}

void setLog(bool on) {  // 3.0.13-beta.3
    State& s = state();
    if (on != s.log) LOG_INFO("chat: [interface] chat_log = %d (uaro-graphics\\chat-logs)", on ? 1 : 0);
    s.log = on;
}

void setFade(int seconds) {  // 3.0.11-beta.240
    State& s = state();
    const ULONGLONG ms = seconds > 0 ? ULONGLONG(seconds) * 1000 : 0;
    if (ms != s.fadeMs) {
        s.fadeMs = ms;
        LOG_INFO("chat: [interface] chat_fade = %d s", seconds);
    }
}

bool ownsPress(HWND game, LPARAM lp) {  // 3.0.11-beta.241: its corner - its size, not the part dragged
    const State& s = state();
    float tx = 0, ty = 0;
    if (!s.shown || !pointerInTarget(game, WM_LBUTTONDOWN, lp, s.k, s.targetW, s.targetH, tx, ty)) return false;
    const cp::Part p = cp::hitTest(s.layout, s.rows, s.show, tx - float(s.layout.x), ty - float(s.layout.y)).part;
    return p == cp::Part::Grip || p == cp::Part::Bar;  // 3.0.11-beta.245: its scroll bar too
}

bool whisperTo(const std::string& utf8Name) {
    State& s = state();
    if (utf8Name.empty() || !setName(s, utf8Name)) return false;
    giveKeys(wordsEdit());
    return true;
}

bool putIntoLine(const std::string& utf8) {
    State& s = state();
    const uint32_t words = wordsEdit();
    if (!words || utf8.empty() || !setTextOf(words)) return false;
    std::string line = editText(words);
    if (!line.empty() && line.back() != ' ') line += ' ';
    line += narrowed(utf8Wide(utf8), s.codePage);
    if (line.size() > std::max<size_t>(1, read32(words + kEditMost))) return false;
    reinterpret_cast<SetTextFn>(uintptr_t(setTextOf(words)))(words, line.c_str());
    giveKeys(words);
    LOG_INFO("chat: a name put into the line from «Довідник» (%zu bytes)", line.size());
    return true;
}

}  // namespace uagfx::win::chatwin
