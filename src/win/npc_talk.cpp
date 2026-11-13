#include "win/npc_talk.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "core/window_stack.hpp"
#include "core/layout.hpp"
#include "core/npc_talk.hpp"
#include "core/pad_glyphs.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/client_npc.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/card_drag.hpp"
#include "win/chat_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_input.hpp"
#include "win/window_shadow.hpp"
#include "win/window_stack.hpp"
#include "win/uaui.hpp"
#include "win/ui_live.hpp"
#include "win/ui_sheet.hpp"
#include "win/words.hpp"

namespace uagfx::win::npcwin {
namespace {

namespace nt = uagfx::npctalk;

// uaRO.exe's NPC windows (the pad's client_npc has their buttons): the text window (0x10) - its shown flag +0x38,
// the button Enter presses +0x58 (0x98 Next, 0x87 Close, 0x75 none), +0x88 set when a Next was pressed (the next
// words begin a new page, its SendMsg 0x1A); the menu (0x11) - its rows' option numbers (an int vector +0x68 .. +0x6C),
// its list (+0x74) and the list's chosen row (+0x64); the number field (0x38)
constexpr uint32_t kSayWnd = 0x727F7C, kMenuWnd = 0x727F80, kNumberWnd = 0x728038;
constexpr uint32_t kSayClass = 0x698CF0, kMenuClass = 0x699080, kNumberClass = 0x6989F8;
constexpr uint32_t kShown = 0x38, kDefaultButton = 0x58, kNewPage = 0x88;
constexpr uint32_t kRowsBegin = 0x68, kRowsEnd = 0x6C, kList = 0x74, kListChosen = 0x64;
constexpr int kBtnNext = 0x98, kBtnClose = 0x87;

// 3.0.18-beta.88 (live windows, group 3): the NPC's talk a live window - its lines' colour runs, its options in two
// columns by flexbox, its button; RmlUi's clicks, the drag by its ground kept
const char* kLive = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw npw" data-style-background-color="ground">
<div class="head"><span class="title">{{name}}</span><span class="count">{{place}}</span></div>
<span class="x ctl" data-event-click="np_close()">&#x2715;</span>
<div class="talk"><div class="tline" data-for="l : lines"><span data-for="r : l.runs"
  data-style-color="r.colour">{{r.text}}</span></div></div>
<div class="opts"><div class="opt ctl" data-for="o, i : options" data-class-on="i == selected"
  data-event-click="np_choose(i)"><span
  class="num">{{i + 1}}</span><span class="otext"><span data-for="r : o.runs"
  data-style-color="r.colour">{{r.text}}</span></span><span class="last"
  data-if="o.last">{{last_time}}</span></div></div>
<div class="btns" data-if="button != ''"><div class="btn main ctl" data-event-click="np_button()">{{button}}</div></div>
<div class="foot"><span class="hint">{{keys}}</span><span class="hint">{{server}}</span></div>
<div id="ring" class="ring"></div>
</body></rml>)RML";

struct NpcRunView {
    Rml::String text, colour;
};
struct NpcLineView {
    std::vector<NpcRunView> runs;
};
struct NpcOptionView {
    std::vector<NpcRunView> runs;  // 3.0.18-beta.179 (R-P7RFV2): its colours as the game's list draws them
    bool last = false;
};
bool operator==(const NpcRunView& a, const NpcRunView& b) { return a.text == b.text && a.colour == b.colour; }

struct State {
    uilive::Window live;  // 3.0.18-beta.88
    std::vector<NpcLineView> lineViews;
    std::vector<NpcOptionView> optionViews;
    Rml::String name, place, buttonWord, keys, server, lastTime, ground = "#10141cf0";
    int selectedNo = -1;
    uisheet::Sheet sheet;  // 3.0.18-beta.31: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    WindowEdge edge;  // 3.0.11-beta.108: the top of two or more windows (win/window_stack)
    bool on = false, shown = false;
    // 3.0.18-beta.238 (the tester: «пробивається старе біле окошко, бо дальше змінюється на нове»): ours can show
    // (on, a game window, its scale and size known) - the game's text and menu windows held from their first draw
    bool canShow = false;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    unsigned codePage = 0;
    uint32_t npc = 0;
    std::vector<std::string> lines, options;  // as the server sent them (the game's code page)
    nt::Talk talk;
    nt::Show show;
    nt::Layout layout;
    CardDrag drag;  // 3.0.12-beta.2 (a player: «вікно розмови з НПЦ ... не дає можливості його редагувати»)
    int dragTold = 0;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
    // 3.0.11-beta.255 (the concept «Разговор с NPC с геймпада», stage 1): the option chosen last time, A's wait
    bool remember = true;
    std::string menuShown;               // the menu's key whose remembered option was put (once a menu)
    std::map<std::string, int> choices;  // the character's
    std::string choicesOf;               // whose they are (its name)
    std::string content;                 // the page's words, options and button: a change starts A's wait
    uint64_t changedAt = 0;
    int heldTold = 0;
};

State& state() {
    static State s;
    return s;
}

uint32_t window(uint32_t slot, uint32_t cls) {
    const uint32_t w = read32(slot);
    return w && read32(w) == cls ? w : 0;
}

void holdGame(bool hidden) {  // the game's text and menu windows out of sight while ours shows
    for (const auto& [slot, cls] : {std::pair{kSayWnd, kSayClass}, std::pair{kMenuWnd, kMenuClass}})
        if (const uint32_t w = window(slot, cls)) {
            const uint32_t want = hidden ? 0 : 1;
            if (read32(w + kShown) != want) write32(w + kShown, want);
        }
}

std::vector<std::string> tokens(const std::string& text) {  // as the game's strtok splits the menu: no empty ones
    std::vector<std::string> out;
    size_t from = 0;
    while (from <= text.size()) {
        size_t end = text.find(':', from);
        if (end == std::string::npos) end = text.size();
        if (end > from) out.push_back(text.substr(from, end - from));
        from = end + 1;
    }
    return out;
}

nt::Texts texts() {
    nt::Texts t;
    t.next = say(settings::Text::NpcNext);
    t.close = say(settings::Text::SkillsPadClose);
    t.keys = say(settings::Text::NpcKeys);
    t.server = say(settings::Text::NpcServer);
    t.choose = say(settings::Text::NpcChoose);
    t.ok = "OK";
    t.lastTime = say(settings::Text::NpcLastTime);  // 3.0.11-beta.255
    return t;
}

float measure(const std::string& utf8Text, float px, bool bold) {
    return float(wordsWidth(utf8Wide(utf8Text), px, bold));
}

void render(State& s) {
    ++s.sheetGen;
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    panel::Box b = s.layout.glass;
    float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    int lx = 0, ly = 0, lw = 0, lh = 0;  // 3.0.18-beta.88: the live talk's own box
    if (s.live.box(lx, ly, lw, lh)) x0 = float(lx), y0 = float(ly), b.w = float(lw), b.h = float(lh);
    const RECT r = clientBox(client, s.targetW, s.targetH, x0, y0, b.w, b.h);
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Npc, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Npc, nullptr, nullptr, false);
    s.shown = false;
}

// 3.0.11-beta.255: uaro-graphics\npc-choices-<the name's bytes in hex>.txt
std::string choicesPath(const std::string& name) {
    std::string hex;
    char b[3];
    for (unsigned char c : name) {
        std::snprintf(b, sizeof b, "%02x", c);
        hex += b;
    }
    return layout::join(gameDir(), "uaro-graphics\\npc-choices-" + hex + ".txt");
}

void loadChoices(State& s) {
    const std::string name = readOwnName();
    if (name == s.choicesOf) return;
    s.choicesOf = name;
    s.choices.clear();
    std::string text;
    if (!name.empty() && readTextFile(choicesPath(name), text)) s.choices = nt::readChoices(text);
}

void remember(State& s, int row) {
    if (!s.remember || row < 0 || s.talk.options.empty()) return;
    loadChoices(s);
    if (s.choicesOf.empty()) return;
    const std::string key = nt::menuKey(s.talk);
    if (const auto at = s.choices.find(key); at != s.choices.end() && at->second == row) return;
    if (s.choices.size() >= nt::kChoicesMost) s.choices.erase(s.choices.begin());
    s.choices[key] = row;
    const std::string path = choicesPath(s.choicesOf);
    if (!writeFileAtomic(path, nt::writeChoices(s.choices))) LOG_INFO("npc: %s cannot be written", path.c_str());
}

int chosenRow() {
    const uint32_t menu = window(kMenuWnd, kMenuClass);
    const uint32_t list = menu ? read32(menu + kList) : 0;
    return list ? int(read32(list + kListChosen)) : -1;
}

bool choose(State& s, int row, const char* by) {
    const uint32_t menu = window(kMenuWnd, kMenuClass);
    const uint32_t list = menu ? read32(menu + kList) : 0;
    if (!list || row < 0 || row >= int(s.talk.options.size())) return false;
    write32(list + kListChosen, uint32_t(row));
    remember(s, row);  // 3.0.11-beta.255
    const bool done = uapad::clientnpc::answer(uapad::DialogCommand::MenuOk);
    if (s.told++ < 20) LOG_INFO("npc: %s chose the option %d %s", by, row + 1, done ? "(the menu's own OK)" : "- not");
    return done;
}

bool press(State& s, uapad::DialogCommand c, const char* by) {
    if (c == uapad::DialogCommand::MenuOk) remember(s, chosenRow());  // 3.0.11-beta.255
    const bool done = uapad::clientnpc::answer(c);
    if (s.told++ < 20)
        LOG_INFO("npc: %s - %s %s", by,
                 c == uapad::DialogCommand::Next        ? "next"
                 : c == uapad::DialogCommand::Close     ? "close"
                 : c == uapad::DialogCommand::MenuOk    ? "the menu's OK"
                 : c == uapad::DialogCommand::MenuCancel ? "the menu's cancel"
                                                         : "up or down",
                 done ? "(the game's own button)" : "- not done");
    return done;
}

void closeOrCancel(State& s, const char* by) {
    press(s, window(kMenuWnd, kMenuClass) ? uapad::DialogCommand::MenuCancel : uapad::DialogCommand::Close, by);
}

}  // namespace

void packet(const packets::Event& e) {
    State& s = state();
    if (const auto* m = std::get_if<packets::NpcSay>(&e)) {
        const uint32_t sayWin = window(kSayWnd, kSayClass);
        if (!sayWin || m->npc != s.npc || read32(sayWin + kNewPage)) s.lines.clear();  // a new talk or page
        s.npc = m->npc;
        s.lines.push_back(m->text);
        if (s.lines.size() > 40) s.lines.erase(s.lines.begin());
    } else if (const auto* m = std::get_if<packets::NpcMenu>(&e)) {
        s.npc = m->npc;
        s.options = tokens(m->text);
    }
}

static void frameInner(HWND game, bool on, bool remember, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
    State& s = state();
    if (remember != s.remember) {  // 3.0.11-beta.255
        s.remember = remember;
        LOG_INFO("npc: [interface] npc_remember = %d", remember ? 1 : 0);
    }
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        if (!on) holdGame(false);
        LOG_INFO("npc: [interface] npc_talk = %d", on ? 1 : 0);
    }
    // 3.0.11-beta.195 (the tester: the Kafras' menus «у старому стилі»): a script's menu with no words before it is
    // the game's menu window alone - the talk shows it too, its options alone
    const uint32_t sayWin = inGameWorld() ? window(kSayWnd, kSayClass) : 0;
    const uint32_t menuWin = inGameWorld() ? window(kMenuWnd, kMenuClass) : 0;
    if (!sayWin) s.lines.clear();
    if (!sayWin && !menuWin) s.options.clear();
    if (!menuWin) s.menuShown.clear();
    if (!on || (!sayWin && !menuWin) || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        s.content.clear();
        return;
    }
    holdGame(true);
    const float scale = uistyle::readingScale(k);
    s.k = float(k);
    s.scale = scale;
    s.codePage = codePage;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    nt::Talk t;
    for (const std::string& line : s.lines) t.lines.push_back(utf8Of(widened(line, codePage)));
    if (const uint32_t menu = menuWin) {
        const uint32_t begin = read32(menu + kRowsBegin), end = read32(menu + kRowsEnd);
        for (uint32_t at = begin; begin && at < end && at < begin + 4 * 64; at += 4) {
            const int idx = int(read32(at));
            t.options.push_back(idx >= 0 && idx < int(s.options.size()) ? utf8Of(widened(s.options[size_t(idx)],
                                                                                         codePage))
                                                                         : "#" + std::to_string(idx + 1));
        }
        const uint32_t list = read32(menu + kList);
        t.selected = list ? int(read32(list + kListChosen)) : -1;
    } else {
        const int def = int(read32(sayWin + kDefaultButton));
        t.button = def == kBtnNext ? nt::Button::Next : def == kBtnClose ? nt::Button::Close : nt::Button::None;
    }
    const std::string map = readGameMap();
    t.place = map;
    s.talk = nt::named(t);
    // 3.0.11-beta.255: a new menu - the option chosen last time there put as the chosen one
    s.show.remembered = -1;
    if (menuWin && !s.talk.options.empty()) {
        const std::string mk = nt::menuKey(s.talk);
        loadChoices(s);
        const auto at = s.choices.find(mk);
        const int last = at != s.choices.end() && at->second < int(s.talk.options.size()) ? at->second : -1;
        if (s.remember && last >= 0) s.show.remembered = last;
        if (mk != s.menuShown) {
            s.menuShown = mk;
            const uint32_t list = read32(menuWin + kList);
            if (s.remember && last >= 0 && list) {
                write32(list + kListChosen, uint32_t(last));
                s.talk.selected = last;
                if (s.told++ < 20) LOG_INFO("npc: the option %d chosen last time put first at this menu", last + 1);
            }
        }
    }
    // 3.0.11-beta.255: a new page, menu or button - A taken after kGuardMs
    std::string content = std::to_string(int(s.talk.button)) + '|';
    for (const std::string& l : s.talk.lines) content += l + '\n';
    for (const std::string& o : s.talk.options) content += o + '|';
    const uint64_t now = GetTickCount64();
    if (content != s.content) {
        s.content = content;
        s.changedAt = now;
    }
    const uapad::runtime::Look pad = uapad::runtime::look();
    s.show.pad = pad.shown;
    s.show.sony = pad.sony;
    s.show.waitA = pad.shown ? nt::waiting(s.changedAt, now) : 0;
    const nt::Texts tx = texts();
    s.layout = nt::layout(targetWidth / 2, targetHeight - int(std::lround(200 * scale)), scale, s.talk, s.show, tx,
                          measure);
    s.drag.apply(s.layout.x, s.layout.y, s.layout.w, s.layout.h, targetWidth, targetHeight, k,
                 48 * scale);  // 3.0.12-beta.2: moved by its title or its words
    s.show.hoverOption = -1;
    s.show.hoverButton = s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy)) {
        const nt::Hit h = nt::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == nt::Part::Option) s.show.hoverOption = h.index;
        s.show.hoverButton = h.part == nt::Part::Button;
        s.show.hoverClose = h.part == nt::Part::Close;
    }
    std::string key = std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ';' + std::to_string(scale) +
                      ';' + std::to_string(s.show.hoverOption) + (s.show.hoverButton ? "b" : "") +
                      (s.show.hoverClose ? "x" : "") + (s.show.pad ? (s.show.sony ? "S" : "X") : "") + ';' +
                      std::to_string(int(s.show.waitA * 8)) + ';' + std::to_string(s.show.remembered) + ';' +
                      std::to_string(s.talk.selected) + ';' + std::to_string(int(s.talk.button)) + ';' + s.talk.name +
                      ';' + s.talk.place + ';';
    for (const std::string& l : s.talk.lines) key += l + '\n';
    for (const std::string& o : s.talk.options) key += o + '|';
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()) + (uaui::ready() ? "u" : "");
        drawn != s.key) {  // 3.0.18-beta.31: uaUI's or the picture
        s.key = drawn;
        render(s);
    }
    if (!s.shown && s.told < 20) {
        ++s.told;
        LOG_INFO("npc: the talk in the interface's window - %zu line(s), %zu option(s), the NPC %u",
                 s.talk.lines.size(), s.talk.options.size(), s.npc);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.88: the live talk's model and place
static void liveFrame(State& s) {
    s.live.ownDrag = true;  // 3.0.18-beta.92: its own drag
    s.live.make("npc", kLive, [&](Rml::DataModelConstructor& m) {
        if (auto v = m.RegisterStruct<NpcRunView>()) {
            v.RegisterMember("text", &NpcRunView::text);
            v.RegisterMember("colour", &NpcRunView::colour);
        }
        m.RegisterArray<std::vector<NpcRunView>>();
        if (auto v = m.RegisterStruct<NpcLineView>()) v.RegisterMember("runs", &NpcLineView::runs);
        if (auto v = m.RegisterStruct<NpcOptionView>()) {
            v.RegisterMember("runs", &NpcOptionView::runs);
            v.RegisterMember("last", &NpcOptionView::last);
        }
        m.RegisterArray<std::vector<NpcLineView>>();
        m.RegisterArray<std::vector<NpcOptionView>>();
        m.Bind("lines", &s.lineViews);
        m.Bind("options", &s.optionViews);
        m.Bind("name", &s.name);
        m.Bind("place", &s.place);
        m.Bind("button", &s.buttonWord);
        m.Bind("keys", &s.keys);
        m.Bind("server", &s.server);
        m.Bind("last_time", &s.lastTime);
        m.Bind("selected", &s.selectedNo);
        m.Bind("ground", &s.ground);
        m.BindEventCallback("np_choose", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (!a.empty()) choose(state(), a[0].Get<int>(), "a click");
        });
        m.BindEventCallback("np_button", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            State& st = state();
            press(st, st.talk.button == nt::Button::Next ? uapad::DialogCommand::Next : uapad::DialogCommand::Close,
                  "its button");
        });
        m.BindEventCallback("np_close", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            closeOrCancel(state(), "its close");
        });
    });
    if (!s.live.made()) return;
    const nt::Texts tx = texts();
    const nt::Talk& t = s.talk;
    const auto views = [](const std::vector<nt::Run>& runs) {
        std::vector<NpcRunView> out;
        for (const nt::Run& r : runs) {
            char col[16];
            std::snprintf(col, sizeof col, "#%06x", unsigned(uaui::themed(r.rgb) & 0xFFFFFF));
            out.push_back(NpcRunView{r.text, col});
        }
        return out;
    };
    std::vector<NpcLineView> lines;
    for (const std::string& l : t.lines) lines.push_back(NpcLineView{views(nt::colours(l, 0xEEF2F8))});
    std::vector<NpcOptionView> options;
    const std::vector<std::vector<nt::Run>> optionRuns = nt::optionRuns(t.options, 0xEEF2F8);
    for (size_t i = 0; i < optionRuns.size(); ++i)
        options.push_back(NpcOptionView{views(optionRuns[i]), int(i) == s.show.remembered});
    const Rml::String button = t.button == nt::Button::Next ? tx.next : t.button == nt::Button::Close ? tx.close : "";
    const Rml::String ground = uisheet::Sheet::ground(0x10141C, uistyle::window());
    const bool sameLines = lines.size() == s.lineViews.size() &&
                           std::equal(lines.begin(), lines.end(), s.lineViews.begin(),
                                      [](const NpcLineView& a, const NpcLineView& b) { return a.runs == b.runs; });
    if (!sameLines) s.lineViews = std::move(lines), s.live.doc.dirty("lines");
    const bool sameOptions = options.size() == s.optionViews.size() &&
                             std::equal(options.begin(), options.end(), s.optionViews.begin(),
                                        [](const NpcOptionView& a, const NpcOptionView& b) {
                                            return a.runs == b.runs && a.last == b.last;
                                        });
    if (!sameOptions) s.optionViews = std::move(options), s.live.doc.dirty("options");
    if (t.name != s.name || t.place != s.place || button != s.buttonWord || tx.keys != s.keys ||
        tx.server != s.server || tx.lastTime != s.lastTime || t.selected != s.selectedNo || ground != s.ground) {
        s.name = t.name, s.place = t.place, s.buttonWord = button, s.keys = tx.keys, s.server = tx.server;
        s.lastTime = tx.lastTime, s.selectedNo = t.selected, s.ground = ground;
        s.live.doc.dirtyAll();
    }
    s.live.frame(s.shown, s.layout.x, s.layout.y, int(winstack::Win::Npc), ".opt, .btn, .x");
}

void frame(HWND game, bool on, bool remember, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, remember, codePage, k, targetWidth, targetHeight);
    State& s = state();
    s.canShow = on && game && k > 0 && targetWidth > 0 && targetHeight > 0;  // 3.0.18-beta.238
    liveFrame(s);  // the old edition (the sheet) gone: the live window alone
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.live.shown) return;  // 3.0.18-beta.88
    s.live.draw(s.shadow, record, renderer, s.scale, s.k);
}

bool shown() { return state().shown; }

bool numberOpen() { return inGameWorld() && window(kNumberWnd, kNumberClass) != 0; }

bool padCommand(uapad::DialogCommand c) {
    using uapad::DialogCommand;
    State& s = state();
    if (!s.shown) return false;
    const uint64_t now = GetTickCount64();
    switch (c) {
        case DialogCommand::Next:
        case DialogCommand::MenuOk:
            if (nt::waiting(s.changedAt, now) > 0) {  // a new page or menu: A not taken yet
                if (s.heldTold++ < 10) LOG_INFO("npc: the controller's A held - the page or menu is new");
                return true;
            }
            if (c == DialogCommand::MenuOk) remember(s, chosenRow());
            return false;
        case DialogCommand::MenuUp:
        case DialogCommand::MenuDown:
        case DialogCommand::MenuLeft:
        case DialogCommand::MenuRight:
        case DialogCommand::MenuPageUp:
        case DialogCommand::MenuPageDown: {
            const uint32_t menu = window(kMenuWnd, kMenuClass);
            const uint32_t list = menu ? read32(menu + kList) : 0;
            const int count = int(s.talk.options.size());
            if (!list || count <= 0) return false;
            const nt::Move m = c == DialogCommand::MenuUp       ? nt::Move::Up
                               : c == DialogCommand::MenuDown   ? nt::Move::Down
                               : c == DialogCommand::MenuLeft   ? nt::Move::Left
                               : c == DialogCommand::MenuRight  ? nt::Move::Right
                               : c == DialogCommand::MenuPageUp ? nt::Move::PageUp
                                                                : nt::Move::PageDown;
            const int to = nt::moved(int(read32(list + kListChosen)), count, m);
            if (to >= 0) write32(list + kListChosen, uint32_t(to));
            s.key.clear();
            return true;
        }
        default: return false;
    }
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = mouseButton(m);
    if (!button && m != WM_MOUSEWHEEL && m != WM_MOUSEMOVE) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    if (s.live.shown) {  // 3.0.18-beta.88: RmlUi's - a press on its ground drags it
        if (m == WM_LBUTTONUP) s.drag.end();
        if (m == WM_MOUSEMOVE) return false;
        const int r = s.live.mouse(game, m, wp, lp);
        if (!r) return false;
        if (r == 2) s.drag.begin(tx, ty);
        result = 0;
        return true;
    }
    return false;  // the old edition gone: the live window alone
}

namespace {
// 3.0.13-beta.10: the NPC's picture - the windows' manager's +0x198 (made by FUN_004ab7e0, its class 0x69B048); the
// game mode's messages (vtable + 0x14, FUN_005707d0 in this build): 0x71 shows a picture, an empty one with type -1
// closes it (the packet 0x01B3 with 255, uaRO.exe 0x57B657)
constexpr uint32_t kCutinWnd = 0x727ED8 + 0x198, kCutinClass = 0x69B048;
constexpr uint32_t kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0, kCutinMsg = 0x71;
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);
bool g_cutinEsc = true;
int g_cutinTold = 0;
}  // namespace

void setCutinEsc(bool on) { g_cutinEsc = on; }

void cutinEsc(UINT m, WPARAM wp) {
    if (!g_cutinEsc || m != WM_KEYDOWN || wp != VK_ESCAPE) return;
    const uint32_t w = window(kCutinWnd, kCutinClass);
    if (!w || !read32(w + kShown)) return;
    const uint32_t mode = read32(kModeNow);
    if (!mode || read32(read32(mode) + kModeMsgSlot) != kModeMsg) return;
    static const char none[1] = {0};
    reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, int(kCutinMsg), int(reinterpret_cast<uintptr_t>(none)), -1,
                                                     0);
    if (g_cutinTold++ < 5) LOG_INFO("npc: Esc - the NPC's picture closed as the server closes it");
}

bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    (void)lp;
    if (m != WM_KEYDOWN || !s.shown || window(kNumberWnd, kNumberClass) || !chatwin::inputEmpty()) return false;
    const bool menu = window(kMenuWnd, kMenuClass) != 0;
    int digit = -1;
    if (wp >= '1' && wp <= '9') digit = int(wp - '1');
    if (wp >= VK_NUMPAD1 && wp <= VK_NUMPAD9) digit = int(wp - VK_NUMPAD1);
    if (digit >= 0) {
        if (!menu || digit >= int(s.talk.options.size())) return false;
        choose(s, digit, "a key");
        return true;
    }
    switch (wp) {
        case VK_RETURN:
            if (menu)
                press(s, uapad::DialogCommand::MenuOk, "Enter");
            else if (s.talk.button != nt::Button::None)
                press(s, s.talk.button == nt::Button::Next ? uapad::DialogCommand::Next : uapad::DialogCommand::Close,
                      "Enter");
            else
                return false;
            return true;
        case VK_ESCAPE: closeOrCancel(s, "Esc"); return true;
        case VK_UP:
        case VK_DOWN:
            if (!menu) return false;
            press(s, wp == VK_UP ? uapad::DialogCommand::MenuUp : uapad::DialogCommand::MenuDown, "an arrow");
            return true;
        default: return false;
    }
}

void beforeGameDraw() {
    State& s = state();
    // 3.0.18-beta.238: not only once ours is shown - the window the game made this frame (a Kafra's first words) was
    // drawn by it once before ours took its place at the present
    if (s.on && s.canShow && inGameWorld()) holdGame(true);
}


bool box(int& x, int& y, int& w, int& h) {  // 3.0.11-beta.109
    const State& s = state();
    if (!s.shown) return false;
    if (s.live.box(x, y, w, h)) return true;  // 3.0.18-beta.88
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

}  // namespace uagfx::win::npcwin
