#include "win/chat_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "core/chat_panel.hpp"
#include "core/hotbar_panel.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/hud_edit.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/target_card.hpp"
#include "win/ui_cost.hpp"

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
const int kHeights[] = {7, 12, 4};  // F10 goes round them

struct Raw {
    std::string text;
    uint32_t colour = 0;
    cp::Source source = cp::Source::Other;
    ULONGLONG ms = 0;
};

struct State {
    bool on = false, shown = false, moved = false, forced = false, oldTold = false, folded = false;
    bool inputTold = false, swallowTab = false;
    unsigned codePage = 0;
    std::vector<Raw> pending;
    std::deque<cp::Line> lines, battle;
    uint64_t version = 0;
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
    ULONGLONG killAt = 0;
    std::map<uint16_t, std::string> skillNames;
    int linesTold = 0, clicksTold = 0;
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
    }
    return reinterpret_cast<PushFn>(g_push)(manager, msg, text, colour, more);
}

std::string say(settings::Text t) { return settings::tr(startup().lang, t); }

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
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
    t.type = say(settings::Text::ChatHintType);
    t.newer = "\xE2\x86\x93";  // ↓
    return t;
}

float measure(const std::string& utf8Text, float px, bool bold) {
    return float(wordsWidth(utf8Wide(utf8Text), px, bold));
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

void add(State& s, cp::Line line) {
    const ULONGLONG now = GetTickCount64();
    const int u = cp::unreadOn(line);
    if (u >= 0 && u != int(s.show.tab)) ++s.show.unread[u];
    if (cp::shows(s.show.tab, line)) s.lastLine = now;
    std::deque<cp::Line>& to = line.kind == cp::Kind::Battle ? s.battle : s.lines;
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
        add(s, cp::fromGame(r.source, utf8(s, r.text), r.colour, t, r.ms));
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

uint32_t wordsEdit() {
    const uint32_t chat = gameChat(), e = chat ? read32(chat + kWordsEdit) : 0;
    return e && read32(e) == kWordsClass ? e : 0;
}

uint32_t nameEdit() {
    const uint32_t chat = gameChat(), e = chat ? read32(chat + kNameEdit) : 0;
    return e && read32(e) == kNameClass ? e : 0;
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
    }
    if (const uint32_t name = nameEdit()) {
        in.whisper = utf8(s, editText(name));
        in.whisperFocus = read32(kFocus) == name;
    }
    const uint32_t mode = read32(kMode);
    in.mode = mode <= 2 ? int(mode) : 0;
    in.battle = read32(kBattle) != 0;
    in.typing = !in.battle || read32(kTyping) != 0;
    in.caretOn = (GetTickCount64() / kBlinkMs) % 2 == 0;
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
void named(cp::Line& l, const std::string& form, const std::string& name, uint32_t nameRgb) {
    const size_t at = form.find("{1}");
    const std::string before = at == std::string::npos ? "" : form.substr(0, at);
    const std::string after = at == std::string::npos ? " " + form : form.substr(at + 3);
    if (!before.empty()) l.spans.push_back(cp::Span{before, cp::kMuted, false, false});
    l.spans.push_back(cp::Span{name, nameRgb, true, false});
    if (!after.empty()) l.spans.push_back(cp::Span{after, cp::kMuted, false, false});
}

void killed(State& s, uint32_t id, uint32_t me) {
    cp::Line l;
    l.kind = cp::Kind::Loot;
    l.ms = GetTickCount64();
    named(l, say(settings::Text::ChatKilled), actorName(s, id, me), cp::kText);
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
    named(l, say(settings::Text::ChatPicked), name, cp::kText);
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

std::vector<Words> gdiWords(const std::vector<cp::Words>& in) {
    std::vector<Words> out;
    for (const cp::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == cp::Align::Right    ? DT_RIGHT
                                 : w.align == cp::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("chat", s.layout.w, s.layout.h);  // 3.0.11-beta.55
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;  // 3.0.11-beta.57: in the memory it had
    const cp::Texts t = texts();
    cp::paint(s.picture, s.layout, s.rows, s.input, s.show, t, measure);
    drawWords(s.picture, gdiWords(cp::words(s.layout, s.rows, s.input, s.show, t, measure)));
    if (s.layout.quiet) panel::fade(s.picture, 0.72f);
    s.upload = true;
}

// The tab's lines as rows, made again when a line came or changed, the tab or the width changed
void makeRows(State& s) {
    const std::string key = std::to_string(s.version) + ';' + std::to_string(int(s.show.tab)) + ';' +
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
            if (cp::shows(s.show.tab, l)) s.rowLines.push_back(&l);
    }
    if (s.rowLines.size() > 200) s.rowLines.erase(s.rowLines.begin(), s.rowLines.end() - 200);
    s.rows = cp::wrap(s.rowLines, s.layout.lines.w, s.layout.px, measure);
}

void block(const State& s, HWND game) {  // the mouse's clicks over it are its own, not the world's
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
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

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    take(s);  // the lines kept with the chat off too: on, it shows them
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
    const ULONGLONG now = GetTickCount64();
    int cx = 0, cy = 0;
    cp::Hit hit;
    if (readGameCursor(cx, cy) && s.layout.w > 0)
        hit = cp::hitTest(s.layout, s.rows, s.show, float(cx) * float(k) - float(s.layout.x),
                          float(cy) * float(k) - float(s.layout.y));
    const bool over = hit.part != cp::Part::None;
    const bool busy = !s.input.text.empty() || s.input.whisperFocus || (s.input.battle && s.input.typing);
    if (s.folded && !busy) {  // Alt+F10: out of sight until one types
        hide(s);
        return;
    }
    const bool quiet = !busy && !over && s.show.scroll == 0 && now - s.lastLine > kQuietMs;
    s.show.hoverTab = hit.part == cp::Part::Tab ? hit.index : -1;
    s.show.hoverLine = hit.part == cp::Part::Name ? hit.index : -1;
    s.show.hoverMode = hit.part == cp::Part::Mode;
    s.show.hoverClear = hit.part == cp::Part::Clear;
    s.show.hoverNewer = hit.part == cp::Part::Newer;
    s.layout = cp::layout(int(std::lround(10 * scale)), targetHeight - int(std::lround(10 * scale)), scale,
                          kHeights[s.height % 3], quiet, s.show, s.input, texts(), measure);
    hudedit::place(hudlayout::Part::Chat, float(k), targetWidth, targetHeight, s.layout.w, s.layout.h, s.layout.x,
                   s.layout.y);  // 3.0.11-beta.75: where the player put it
    makeRows(s);
    s.show.scroll = std::min(s.show.scroll, mostScroll(s));
    const cp::Input& in = s.input;
    const bool caret = !quiet && in.caretOn && (in.whisperFocus || (in.typing && !in.text.empty()));
    std::string key = std::to_string(s.version) + ';' + s.rowsKey + ';' + std::to_string(s.layout.w) + 'x' +
                      std::to_string(s.layout.h) + ';' + std::to_string(scale) + ';' + (quiet ? "q" : "") +
                      std::to_string(s.show.scroll) + ';' + std::to_string(s.show.hoverTab) + ',' +
                      std::to_string(s.show.hoverLine) + (s.show.hoverMode ? "m" : "") +
                      (s.show.hoverClear ? "c" : "") + (s.show.hoverNewer ? "n" : "") + ';';
    for (int u : s.show.unread) key += std::to_string(u) + ',';
    key += ';' + in.text + '|' + std::to_string(in.caret) + '|' + in.whisper + '|' + std::to_string(in.mode) +
           (in.whisperFocus ? "f" : "") + (in.battle ? "b" : "") + (in.typing ? "t" : "") + (caret ? "|" : "");
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()); drawn != s.key) {  // its grounds
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
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

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
    int cursor = -1, cx = 0, cy = 0;  // under the game's own cursor, as the minimap
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;  // 3.0.11-beta.43: where the game's cursor stands, as the hover
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty)) return false;
    const cp::Hit h = cp::hitTest(s.layout, s.rows, s.show, tx - float(s.layout.x), ty - float(s.layout.y));
    if (h.part == cp::Part::None) return false;
    result = 0;
    if (m == WM_MOUSEWHEEL) {  // three rows a notch, up into the history
        const int by = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 3 : -3;
        s.show.scroll = std::clamp(s.show.scroll + by, 0, mostScroll(s));
        s.key.clear();
        return true;
    }
    if (m != WM_LBUTTONDOWN) return true;
    const char* did = "";
    switch (h.part) {
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

bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    (void)lp;
    if (m == WM_CHAR && wp == '\t' && s.swallowTab) {  // the Tab the channel took: not into the game's field
        s.swallowTab = false;
        return true;
    }
    if (!s.on || !inGameWorld()) return false;
    const bool down = m == WM_KEYDOWN || m == WM_SYSKEYDOWN;
    if (!down) return false;
    if (wp == VK_F10) {  // the game's own chat out of sight: F10 the new one's height, Alt+F10 away until one types
        if (GetKeyState(VK_MENU) < 0)
            s.folded = !s.folded;
        else {
            s.height = (s.height + 1) % 3;
            s.folded = false;
        }
        s.key.clear();
        LOG_INFO("chat: %s", s.folded ? "folded away (Alt+F10)" : "its height changed (F10)");
        return true;
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

}  // namespace uagfx::win::chatwin
