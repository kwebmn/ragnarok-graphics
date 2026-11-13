#include "win/osk_window.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/osk.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/core/config/bindings.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"
#include "win/words.hpp"

namespace uagfx::win::oskwin {
namespace {

// uaRO.exe's: the chat window (its words' edit, its SendMsg), the chat room (its edit), the whisper windows (in the
// windows' list, their edits); the manager's control with the keys and its SetFocus; an edit's words, its longest
// (+ 0x54) and SetText (vtable + 0x98)
constexpr uint32_t kWindowMgr = 0x727ED8, kChatAt = kWindowMgr + 0x84, kChatClass = 0x697D78, kChatWords = 0x68;
constexpr uint32_t kRoomAt = kWindowMgr + 0x104, kRoomClass = 0x697E10, kRoomEdit = 0x94;
constexpr uint32_t kWindowsList = kWindowMgr + 0x38, kWhisperClass = 0x699BC8, kWhisperEdit = 0x84;
constexpr uint32_t kFocus = kWindowMgr + 0x6C, kSetFocus = 0x4CE510, kGetText = 0x443300, kLongest = 0x54;
constexpr uint32_t kSetTextSlot = 0x98, kSendMsgSlot = 0x94;
constexpr int kButtonMsg = 6, kEnter = 0x76;
constexpr ULONGLONG kBlinkMs = 530;
using GetTextFn = const char*(__attribute__((thiscall)) *)(uint32_t edit);
using SetTextFn = void(__attribute__((thiscall)) *)(uint32_t edit, const char* text);
using SetFocusFn = void(__attribute__((thiscall)) *)(uint32_t manager, uint32_t control);

struct State {
    uisheet::Sheet sheet;  // 3.0.18-beta.34: the keyboard drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    osk::Grid sheetGrid;
    bool sheetCaret = false;
    bool open = false, shift = false;
    osk::Lang lang = osk::Lang::Uk;
    osk::Focus focus{1, 0};
    uint32_t edit = 0, owner = 0;  // the line typed into, the window its Enter goes to
    Field field;                   // 3.0.18-beta.241: a driver window's own field (own: typed into it, not the game's)
    bool own = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    std::string line;  // UTF-8, as the game's line has it
    osk::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

uint32_t chatWindow() {
    const uint32_t w = read32(kChatAt);
    return w && read32(w) == kChatClass ? w : 0;
}

// The line with the keys among the ones the keyboard knows (a whisper card's, the room's, the chat's), its window;
// none - the chat's, given the keys
void pickLine(State& s) {
    const uint32_t focus = read32(kFocus);
    s.edit = s.owner = 0;
    if (focus) {
        const uint32_t room = read32(kRoomAt);
        if (room && read32(room) == kRoomClass && read32(room + kRoomEdit) == focus) {
            s.edit = focus;
            s.owner = room;
            return;
        }
        const uint32_t head = read32(kWindowsList);
        size_t seen = 0;
        for (uint32_t node = head ? read32(head) : 0; node && node != head && seen < 512; node = read32(node), ++seen) {
            const uint32_t w = read32(node + 8);
            if (w && read32(w) == kWhisperClass && read32(w + kWhisperEdit) == focus) {
                s.edit = focus;
                s.owner = w;
                return;
            }
        }
    }
    const uint32_t chat = chatWindow();
    if (!chat) return;
    s.edit = read32(chat + kChatWords);
    s.owner = chat;
    if (s.edit && s.edit != focus) reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, s.edit);
}

std::string gameText(uint32_t edit) {
    if (!edit) return "";
    const char* t = reinterpret_cast<GetTextFn>(uintptr_t(kGetText))(edit);
    return t ? std::string(t, strnlen(t, 256)) : std::string();
}

void setText(uint32_t edit, const std::string& text) {
    if (edit) reinterpret_cast<SetTextFn>(uintptr_t(read32(read32(edit) + kSetTextSlot)))(edit, text.c_str());
}

size_t letters(const std::string& utf8) {
    size_t n = 0;
    for (const char c : utf8) n += (static_cast<unsigned char>(c) & 0xC0) != 0x80;
    return n;
}

// The game's line with `utf8` after it, when it still takes it (its longest in the code page's bytes); a window's own
// field (3.0.18-beta.241) by its letters
void type(State& s, const std::string& utf8) {
    if (s.own) {
        const std::string now = s.field.read ? s.field.read() : std::string();
        if (s.field.write && letters(now) + letters(utf8) <= s.field.longest) s.field.write(now + utf8);
        s.key.clear();
        return;
    }
    if (!s.edit) return;
    const std::string now = gameText(s.edit), more = narrowed(utf8Wide(utf8), s.codePage);
    const uint32_t longest = read32(s.edit + kLongest);
    if (more.empty() || (longest > 0 && now.size() + more.size() > longest)) return;
    setText(s.edit, now + more);
    s.key.clear();
}

void erase(State& s) {
    if (s.own) {  // 3.0.18-beta.241: its last letter (a UTF-8 sequence)
        std::string now = s.field.read ? s.field.read() : std::string();
        while (!now.empty() && (static_cast<unsigned char>(now.back()) & 0xC0) == 0x80) now.pop_back();
        if (!now.empty()) now.pop_back();
        if (s.field.write) s.field.write(now);
        s.key.clear();
        return;
    }
    if (!s.edit) return;
    std::wstring w = widened(gameText(s.edit), s.codePage);
    if (w.empty()) return;
    w.pop_back();
    setText(s.edit, narrowed(w, s.codePage));
    s.key.clear();
}

void send(State& s) {
    if (s.own) {  // 3.0.18-beta.241: the window's Enter
        if (s.field.enter) s.field.enter();
        close();
        return;
    }
    if (!s.owner) return;
    if (s.told++ < 30) LOG_INFO("keyboard: the line sent (its window's Enter)");
    reinterpret_cast<SendMsgFn>(uintptr_t(read32(read32(s.owner) + kSendMsgSlot)))(s.owner, 0, kButtonMsg, kEnter, 0,
                                                                                   0);
    close();
}

const char* langName(osk::Lang l) {
    switch (l) {
        case osk::Lang::Uk: return "\xD0\xA3\xD0\x9A\xD0\xA0";  // УКР
        case osk::Lang::Ru: return "\xD0\xA0\xD0\xA3\xD0\xA1";  // РУС
        case osk::Lang::En: return "ENG";
        case osk::Lang::Symbols: return "!?#";
    }
    return "";
}

osk::Texts texts(const State& s) {
    osk::Texts t;
    t.space = say(settings::Text::OskSpace);
    t.send = say(settings::Text::OskSend);
    t.lang = langName(s.lang);
    std::string hint = say(settings::Text::OskHint);
    const std::string names[] = {uapad::buttonLabel(0), uapad::buttonLabel(2), uapad::buttonLabel(3),
                                 uapad::buttonLabel(4), uapad::buttonLabel(5), uapad::buttonLabel(6),
                                 uapad::buttonLabel(1),  uapad::buttonLabel(7), uapad::buttonLabel(13)};
    for (size_t i = 0; i < std::size(names); ++i) {
        const std::string key = "{" + std::to_string(i + 1) + "}";
        const size_t at = hint.find(key);
        if (at != std::string::npos) hint.replace(at, key.size(), names[i]);
    }
    t.hint = hint;
    return t;
}

void press(State& s) {
    const osk::Grid g = osk::rows(s.lang, s.shift);
    const osk::Key& k = osk::keyAt(g, s.focus);
    switch (k.special) {
        case osk::Special::Shift: s.shift = !s.shift; break;
        case osk::Special::Space: type(s, " "); break;
        case osk::Special::Back: erase(s); break;
        case osk::Special::Lang: s.lang = osk::next(s.lang, 1); break;
        case osk::Special::Send: send(s); return;
        case osk::Special::None:
            type(s, k.text);
            if (s.shift && s.lang != osk::Lang::Symbols) s.shift = false;  // a capital once, as phones do
            break;
    }
    s.key.clear();
}

}  // namespace

void open() {
    State& s = state();
    if (s.open) return;
    s.open = true;
    const settings::Lang ui = language();
    s.lang = ui == settings::Lang::Ru ? osk::Lang::Ru : ui == settings::Lang::En ? osk::Lang::En : osk::Lang::Uk;
    s.shift = false;
    s.focus = osk::Focus{1, 0};
    s.own = false;
    pickLine(s);
    s.key.clear();
    LOG_INFO("keyboard: open - typing into %s", !s.owner ? "nothing (no chat line)" :
                                                s.owner == chatWindow() ? "the chat's line" : "a card's line");
}

void openFor(const Field& f) {
    State& s = state();
    const settings::Lang ui = language();
    s.lang = ui == settings::Lang::Ru ? osk::Lang::Ru : ui == settings::Lang::En ? osk::Lang::En : osk::Lang::Uk;
    s.shift = false;
    s.focus = osk::Focus{1, 0};
    s.field = f;
    s.own = true;
    s.edit = s.owner = 0;
    s.key.clear();
    s.open = true;
    LOG_INFO("keyboard: open - typing into a window's own field");
}

bool isOpen() { return state().open; }

void close() {
    State& s = state();
    s.open = false;
    s.own = false;  // 3.0.18-beta.241: a window's own field let go
    s.field = Field{};
}

void command(Command c) {
    State& s = state();
    if (!s.open) return;
    const osk::Grid g = osk::rows(s.lang, s.shift);
    switch (c) {
        case Command::Left: s.focus = osk::move(g, s.focus, -1, 0); break;
        case Command::Right: s.focus = osk::move(g, s.focus, 1, 0); break;
        case Command::Up: s.focus = osk::move(g, s.focus, 0, -1); break;
        case Command::Down: s.focus = osk::move(g, s.focus, 0, 1); break;
        case Command::Press: press(s); return;
        case Command::Erase: erase(s); break;
        case Command::Space: type(s, " "); break;
        case Command::LangBack: s.lang = osk::next(s.lang, -1); break;
        case Command::LangOn: s.lang = osk::next(s.lang, 1); break;
        case Command::Shift: s.shift = !s.shift; break;
        case Command::Send: send(s); return;
        case Command::Close: close(); return;
        case Command::Reply:  // 3.0.18-beta.134 (not a window's own field)
            if (!s.own && s.edit && gameText(s.edit).rfind("/r ", 0) != 0) setText(s.edit, "/r " + gameText(s.edit));
            break;
    }
    s.key.clear();
}

static void frameInner(HWND game, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (!s.open || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) return;
    if (!inGameWorld()) {
        close();
        return;
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.line = s.own ? (s.field.read ? s.field.read() : std::string()) : utf8Of(widened(gameText(s.edit), codePage));
    const osk::Grid g = osk::rows(s.lang, s.shift);
    s.layout = osk::layout(g, targetWidth, targetHeight, s.scale);
    const bool caret = (GetTickCount64() / kBlinkMs) % 2 == 0;
    const std::string key = s.line + (caret ? "|" : "") + ';' + std::to_string(int(s.lang)) + (s.shift ? "S" : "") +
                            ';' + std::to_string(s.focus.row) + ',' + std::to_string(s.focus.col) + ';' +
                            std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + '#' +
                            std::to_string(uistyle::generation()) + (uaui::ready() ? "u" : "");
    if (key != s.key && uaui::ready()) {  // 3.0.18-beta.34: uaUI's keyboard
        s.key = key;
        ++s.sheetGen;
        s.sheetGrid = g;
        s.sheetCaret = caret;
    }
}

// 3.0.18-beta.34 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the keyboard in RmlUi's
// markup, line by line as core osk paints it - its glass, the line's field, the keys (the focused gold, «Надіслати»
// and a held Shift lit); the words where core osk places them
static void fillSheet(State& s, uisheet::Content& c) {
    const osk::Layout& L = s.layout;
    const osk::Grid& g = s.sheetGrid;
    const float sc = L.s;
    constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kBlack = 0x000000;
    c.w = float(L.w), c.h = float(L.h);
    c.rect(c.under, L.glass, 12 * sc, kCard, uistyle::window(), kWhite, 0.12f, 1.0f * sc);
    c.rect(c.under, L.line, 8 * sc, kBlack, 0.35f, kGold, 0.6f, 1.0f * sc);
    for (size_t r = 0; r < L.keys.size() && r < g.size(); ++r)
        for (size_t k = 0; k < L.keys[r].size() && k < g[r].size(); ++k) {
            const osk::Key& key = g[r][k];
            const bool on = int(r) == s.focus.row && int(k) == s.focus.col;
            const bool lit = key.special == osk::Special::Send || (key.special == osk::Special::Shift && s.shift);
            c.rect(c.under, L.keys[r][k], 8 * sc, on ? kGold : lit ? kGold : kWhite, on ? 0.34f : lit ? 0.16f : 0.06f,
                   on ? kGold : kWhite, on ? 0.9f : 0.14f, 1.0f * sc);
        }
    c.say(osk::words(L, g, s.focus, s.line, s.sheetCaret, texts(s)));
}

void frame(HWND game, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, codePage, k, targetWidth, targetHeight);
    State& s = state();
    s.sheet.top = true;  // 3.0.18-beta.69: over the game's own windows, as its picture was
    s.sheetShown = s.sheet.frame("keyboard", s.open && s.k > 0, true, s.layout.x, s.layout.y, s.sheetGen,
                                 [&](uisheet::Content& c) { fillSheet(s, c); });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    (void)record;  // the old edition (its picture) gone: uaUI draws it
    (void)renderer;
}

}  // namespace uagfx::win::oskwin
