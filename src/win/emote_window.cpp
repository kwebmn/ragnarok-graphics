#include "win/emote_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/window_stack.hpp"
#include "core/emote_card.hpp"
#include "core/settings/strings.hpp"
#include "core/sprite_file.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/game_files.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/updates_hook.hpp"
#include "win/window_input.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"
#include "win/words.hpp"

namespace uagfx::win::emotewin {
namespace {

namespace ec = uagfx::emotecard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the emotion list (where the manager keeps it, its class's vtable, its SendMsg; its shown flag, the
// emotion it uses, its list of commands - the first, past the last) and its use (thiscall: x, y - a y past its title
// uses + 0x60's); the chat window (where, its class, its SendMsg: 6 / 0x76 its Enter) and the macros' window
constexpr uint32_t kWindowMgr = 0x727ED8, kEmoteAt = kWindowMgr + 0x1AC, kEmoteClass = 0x69C320;
constexpr uint32_t kEmoteSend = 0x4521D0, kEmoteUse = 0x452540, kSendMsgSlot = 0x94, kUseSlot = 0x68;
constexpr uint32_t kShown = 0x38, kChosen = 0x60, kFirst = 0x6C, kPast = 0x70;
constexpr uint32_t kChatAt = kWindowMgr + 0x84, kChatClass = 0x697D78, kChatSend = 0x45EF90;
constexpr uint32_t kMacrosAt = kWindowMgr + 0x1A8;
constexpr int kButtonMsg = 6, kCloseButton = 0x87, kChatEnter = 0x76, kPastTitle = 0xD9;
// The pictures: 이팩트\emotion.spr by 이팩트\emotion_index3.act (CP949), an action each in the list's order
const std::string kEffects = "data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\";
using UseFn = void(__attribute__((thiscall)) *)(uint32_t self, int x, int y);

// 3.0.18-beta.73 (the big six, F: input through the library): the window a live document - its cells laid out by
// flexbox, pointed at by RCSS, clicked and double clicked by RmlUi's events; the part keeps the drag and the stack
const char* kLive = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw ew" data-style-background-color="ground">
<div class="head"><span class="title">{{title}}</span></div>
<span class="x ctl" data-event-click="shut()">&#x2715;</span>
<div class="grid"><div class="cell ctl" data-for="e, i : emotes" data-event-click="pick(i)"
  data-event-dblclick="send(i)"><img data-if="e.icon != ''" data-attr-src="e.icon"/><span>{{e.cmd}}</span></div></div>
<div class="hint">{{hint}}</div>
<div id="ring" class="ring"></div>
</body></rml>)RML";

struct EmoteView {
    Rml::String icon, cmd;
};

struct State {
    uaui::Doc live;  // 3.0.18-beta.73
    bool liveShown = false;
    std::vector<EmoteView> views;
    std::vector<std::string> iconPrev;
    Rml::String title, hint, ground = "#10141cf0";
    uisheet::NavSet nav;
    uisheet::Sheet sheet;  // 3.0.18-beta.32: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool on = false, shown = false, hid = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    std::vector<std::string> commands;
    std::vector<panel::Picture> icons;
    int iconSide = 0;
    ec::Show show;
    ec::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
    // 3.0.11-beta.260 (the tester: «вікно емоцій не перетягується»): moved by a press outside its emotions - its shift
    // from the middle (the target's pixels, kept while the game runs), the press's point against it while dragged
    float offX = 0, offY = 0, grabX = 0, grabY = 0;
    bool dragging = false;
};

State& state() {
    static State s;
    return s;
}

uint32_t gameEmotes() {
    const uint32_t w = read32(kEmoteAt);
    return w && read32(w) == kEmoteClass && read32(kEmoteClass + kSendMsgSlot) == kEmoteSend &&
                   read32(kEmoteClass + kUseSlot) == kEmoteUse
               ? w
               : 0;
}

bool readFile(const std::string& entry, std::string& out) {
    if (readGameFile(entry, out)) return true;  // as the game finds it (its zips too)
    static std::unique_ptr<gamefiles::Reader> files;
    if (!files) files = std::make_unique<gamefiles::Reader>(gameDir());
    std::vector<uint8_t> bytes;
    if (!files->read(entry, bytes, nullptr)) return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
}

// The list's pictures at `side`, read once a size (the card's and the chat wheel's)
const std::vector<panel::Picture>& picturesAt(int side) {
    static std::map<int, std::vector<panel::Picture>> made;
    const auto it = made.find(side);
    if (it != made.end()) return it->second;
    std::vector<panel::Picture>& out = made[side];
    out.assign(size_t(ec::kEmotions), panel::Picture{});
    std::string spr, act;
    spritefile::Sprite sp;
    spritefile::Act ac;
    if (!readFile(kEffects + "emotion.spr", spr) || !readFile(kEffects + "emotion_index3.act", act) ||
        !spritefile::readSpr(spr, sp) || !spritefile::readAct(act, ac)) {
        LOG_INFO("emotions: the list's pictures not read (emotion.spr, emotion_index3.act) - the commands alone");
        return out;
    }
    int count = 0;
    for (int i = 0; i < ec::kEmotions; ++i) {
        spritefile::Image im;
        if (!spritefile::frameImage(sp, ac, size_t(i), 0, im)) continue;
        const spritefile::Image fit = spritefile::fitted(im, side, side);
        panel::Picture& p = out[size_t(i)];
        p.width = fit.w;
        p.height = fit.h;
        p.px = fit.argb;
        ++count;
    }
    LOG_INFO("emotions: %d of the list's %d pictures at %d px (emotion.spr by emotion_index3.act)", count,
             ec::kEmotions, side);
    return out;
}

void loadIcons(State& s, int side) {
    if (side == s.iconSide && !s.icons.empty()) return;
    s.iconSide = side;
    s.icons = picturesAt(side);
}

void readCommands(State& s, uint32_t window) {
    s.commands.clear();
    const uint32_t first = read32(window + kFirst), past = read32(window + kPast);
    if (!first || past <= first || (past - first) / 4 > 64) return;
    for (uint32_t at = first; at < past; at += 4) {
        char buf[33] = {};
        const uint32_t words = read32(at);
        if (!words || !readGame(words, buf, 32)) buf[0] = 0;
        s.commands.push_back(utf8Of(widened(buf, s.codePage)));
    }
}

ec::Texts texts() {
    ec::Texts t;
    t.title = say(settings::Text::EmoteTitle);
    t.hint = say(settings::Text::EmoteHint);
    return t;
}

void render(State& s) {
    ++s.sheetGen;
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    panel::Box b = s.layout.glass;
    float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    int lx = 0, ly = 0, lw = 0, lh = 0;  // 3.0.18-beta.73: the live window's own box
    if (s.liveShown && s.live.box(lx, ly, lw, lh)) x0 = float(lx), y0 = float(ly), b.w = float(lw), b.h = float(lh);
    const RECT r = clientBox(client, s.targetW, s.targetH, x0, y0, b.w, b.h);
    uapad::gamewindow::setBlocked(Blocker::Emotes, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Emotes, nullptr, nullptr, false);
    s.shown = false;
}

// The emotion `i` as the game's list uses it; `send`: the chat's line sent after it, as Enter does
void use(State& s, int i, bool send) {
    const uint32_t w = gameEmotes();
    if (!w || i < 0 || i >= int(s.commands.size())) return;
    write32(w + kChosen, uint32_t(i));
    reinterpret_cast<UseFn>(uintptr_t(kEmoteUse))(w, 0, kPastTitle);
    const char* what = s.commands[size_t(i)].c_str();
    if (!send) {
        if (s.told++ < 30) LOG_INFO("emotions: %s into the chat's line (a click, as the game's list)", what);
        return;
    }
    if (read32(kMacrosAt)) {  // the game's list gave it to the macros' field - no chat line to send
        if (s.told++ < 30) LOG_INFO("emotions: %s into the macros' field (that window open) - not sent", what);
        return;
    }
    const uint32_t chat = read32(kChatAt);
    if (!chat || read32(chat) != kChatClass || read32(kChatClass + kSendMsgSlot) != kChatSend) {
        if (s.told++ < 30) LOG_INFO("emotions: %s - no chat window to send it from", what);
        return;
    }
    reinterpret_cast<SendMsgFn>(uintptr_t(kChatSend))(chat, 0, kButtonMsg, kChatEnter, 0, 0);
    if (s.told++ < 30) LOG_INFO("emotions: %s sent at once (a double click: the chat's Enter after it)", what);
}

}  // namespace

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("emotions: [interface] emotes = %d", on ? 1 : 0);
    }
    const uint32_t window = gameEmotes();
    if (window) {  // the game's window hidden while ours shows; back when ours is off
        if (on && read32(window + kShown) && write32(window + kShown, 0)) s.hid = true;
        else if (!on && s.hid && write32(window + kShown, 1)) s.hid = false;
    } else {
        s.hid = false;
    }
    if (!on || !window || !inGameWorld() || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) LOG_INFO("emotions: the game's emotion list (Alt+L) - the new card shows it");
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    readCommands(s, window);
    loadIcons(s, int(std::lround(ec::kIconSide * s.scale)));
    s.layout = ec::layout(targetWidth, targetHeight, s.scale);
    int cx = 0, cy = 0;
    const bool cursor = readGameCursor(cx, cy);
    if (s.dragging) {  // .260: it follows the cursor while the button is held
        const int button = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
        if (!cursor || GetAsyncKeyState(button) >= 0) {
            s.dragging = false;
        } else {
            s.offX = float(cx) * float(k) - s.grabX;
            s.offY = float(cy) * float(k) - s.grabY;
        }
    }
    const float baseX = float(s.layout.x), baseY = float(s.layout.y);  // inside the screen, a hand of it at least
    const float hand = 48 * s.scale;
    s.offX = std::clamp(s.offX, hand - baseX - float(s.layout.w), float(targetWidth) - hand - baseX);
    s.offY = std::clamp(s.offY, -baseY, float(targetHeight) - hand - baseY);
    s.layout.x = int(std::lround(baseX + s.offX));
    s.layout.y = int(std::lround(baseY + s.offY));
    s.show = ec::Show{};
    if (cursor) {
        const float lx = float(cx) * float(k) - float(s.layout.x), ly = float(cy) * float(k) - float(s.layout.y);
        s.show.hover = ec::cellAt(s.layout, lx, ly);
        s.show.hoverClose = ec::closeAt(s.layout, lx, ly);
    }
    // .260: not its place (it is dragged), its size
    std::string key = std::to_string(s.show.hover) + (s.show.hoverClose ? "x" : "") + ';' +
                      std::to_string(s.layout.w) + ';' + std::to_string(s.iconSide) + '#' +
                      std::to_string(uistyle::generation());
    for (const std::string& c : s.commands) key += ';' + c;
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.32
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.32 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the emotions in RmlUi's
// markup, line by line as core emotecard paints it - its glass, ✕, the cells (pointed at gold) with their pictures;
// the words where core emotecard places them
static void fillSheet(State& s, uisheet::Content& c) {
    const ec::Layout& L = s.layout;
    const ec::Show& sh = s.show;
    const float sc = L.s;
    constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B;
    auto& u = c.under;
    c.w = float(L.w), c.h = float(L.h);
    c.glass = uisheet::Box{L.glass.x, L.glass.y, L.glass.w, L.glass.h, 12 * sc, 1.0f * sc, 1, "glass"};
    c.ground = uisheet::Sheet::ground(kCard, uistyle::window());
    c.rect(u, L.close, 6 * sc, kWhite, sh.hoverClose ? 0.14f : 0.0f, kWhite, sh.hoverClose ? 0.3f : 0.0f, 1.0f * sc);
    for (size_t i = 0; i < L.cells.size(); ++i) {
        const bool hot = int(i) == sh.hover;
        c.rect(u, L.cells[i], 8 * sc, hot ? kGold : kWhite, hot ? 0.20f : 0.04f, hot ? kGold : kWhite,
               hot ? 0.8f : 0.08f, 1.0f * sc);
        if (i >= s.icons.size() || s.icons[i].width <= 0 || s.icons[i].height <= 0) continue;
        const panel::Picture& p = s.icons[i];
        const panel::Box& b = L.icons[i];
        c.icons.push_back(uisheet::Icon{std::round(b.x + (b.w - float(p.width)) / 2),
                                        std::round(b.y + (b.h - float(p.height)) / 2), float(p.width),
                                        float(p.height), 1, s.sheet.icon(&p)});
    }
    c.say(ec::words(L, s.commands, sh, texts()));
}

// 3.0.18-beta.73: the live window's model, place and pad targets
static void liveFrame(State& s) {
    if (!s.live.made()) {
        s.live.window = true;
        s.live.make("emotes", kLive, [&](Rml::DataModelConstructor& m) {
            if (auto v = m.RegisterStruct<EmoteView>()) {
                v.RegisterMember("icon", &EmoteView::icon);
                v.RegisterMember("cmd", &EmoteView::cmd);
            }
            m.RegisterArray<std::vector<EmoteView>>();
            m.Bind("emotes", &s.views);
            m.Bind("title", &s.title);
            m.Bind("hint", &s.hint);
            m.Bind("ground", &s.ground);
            m.BindEventCallback("pick", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
                if (!a.empty()) use(state(), a[0].Get<int>(), false);
            });
            m.BindEventCallback("send", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
                if (!a.empty()) use(state(), a[0].Get<int>(), true);
            });
            m.BindEventCallback("shut", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { close(); });
        });
        s.nav.shown = [] { return state().liveShown; };
        s.nav.ring = [](const panel::Box* b) {
            if (Rml::Element* r = state().live.byId("ring")) {
                r->SetProperty("display", b ? "block" : "none");
                if (b) {
                    const float d = 3 * uaui::dp();
                    r->SetProperty("left", std::to_string(b->x - d) + "px");
                    r->SetProperty("top", std::to_string(b->y - d) + "px");
                    r->SetProperty("width", std::to_string(b->w + 2 * d) + "px");
                    r->SetProperty("height", std::to_string(b->h + 2 * d) + "px");
                }
            }
        };
    }
    if (!s.live.made()) return;
    const ec::Texts t = texts();
    std::vector<EmoteView> views;
    s.iconPrev.resize(s.commands.size());
    for (size_t i = 0; i < s.commands.size(); ++i) {
        EmoteView v;
        v.cmd = s.commands[i];
        if (i < s.icons.size() && s.icons[i].width > 0)
            v.icon = uaui::picture(&s.icons[i], s.iconSide, s.iconPrev[i]);
        views.push_back(v);
    }
    const Rml::String ground = uisheet::Sheet::ground(0x10141C, uistyle::window());
    const bool same = views.size() == s.views.size() &&
                      std::equal(views.begin(), views.end(), s.views.begin(), [](const EmoteView& a, const EmoteView& b) {
                          return a.icon == b.icon && a.cmd == b.cmd;
                      });
    if (!same) s.views = std::move(views), s.live.dirty("emotes");
    if (t.title != s.title || t.hint != s.hint || ground != s.ground) {
        s.title = t.title, s.hint = t.hint, s.ground = ground;
        s.live.dirtyAll();
    }
    s.live.place(s.layout.x, s.layout.y);
    s.live.show(s.shown);
    s.liveShown = s.shown;
    if (s.liveShown) s.nav.set(int(winstack::Win::Emotes), s.live.boxesOf(".cell, .x"), s.layout.x, s.layout.y);
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    s.liveShown = false;
    if (uaui::ready()) {  // 3.0.18-beta.73: the live window in place of the sheet
        s.sheet.frame("emote", false, true, 0, 0, 0, [](uisheet::Content&) {});
        s.sheetShown = false;
        liveFrame(s);
        return;
    }
    s.live.show(false);
    s.sheetShown = s.sheet.frame("emote", s.shown, true, s.layout.x, s.layout.y, s.sheetGen,
                                 [&](uisheet::Content& c) { fillSheet(s, c); });
    if (s.sheetShown) {  // 3.0.18-beta.60: the pad walks its boxes (A clicks one)
        std::vector<panel::Box> targets;
        for (const auto& part : std::initializer_list<std::vector<panel::Box>>{s.layout.cells})
            targets.insert(targets.end(), part.begin(), part.end());
        s.sheet.nav(int(winstack::Win::Emotes), std::move(targets));
    }
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    s.sheet.raise();  // 3.0.18-beta.43: at its place in the windows' stack
    if (s.liveShown) {  // 3.0.18-beta.73: the live window at its place in the stack, its shadow the driver's
        s.live.raise(10 + uaui::windowRank());
        s.live.mark(uaui::windowMarked());
        int x = 0, y = 0, w = 0, h = 0;
        if (s.live.box(x, y, w, h))
            s.shadow.draw(record, renderer, x, y, w, h, 12 * uaui::dp(), s.scale, s.k);
        return;
    }
    if (!s.shown || s.k <= 0 || ((s.picture.width <= 0 || s.picture.height <= 0) && !s.sheetShown)) return;
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;
    // 3.0.18-beta.43: the shadow the driver's, under uaUI's window too
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h),
                  12 * s.scale, s.scale, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    (void)wp;
    if (!s.shown) return false;
    if (s.dragging && m == WM_LBUTTONUP) {  // .260: the drag over, wherever the button comes up
        s.dragging = false;
        result = 0;
        return true;
    }
    const bool button = mouseButton(m);
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    if (s.liveShown) {  // 3.0.18-beta.73: RmlUi's - a press off its cells and ✕ drags it
        bool control = false;
        if (!s.live.pointer(game, m, wp, lp, control)) return false;
        result = 0;
        if (m == WM_LBUTTONDOWN && !control) {
            s.dragging = true;
            s.grabX = tx - s.offX;
            s.grabY = ty - s.offY;
        }
        return true;
    }
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (!s.layout.glass.contains(lx, ly)) return false;
    result = 0;
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    if (ec::closeAt(s.layout, lx, ly)) {
        close();
        return true;
    }
    const int i = ec::cellAt(s.layout, lx, ly);
    if (i >= 0) {
        use(s, i, m == WM_LBUTTONDBLCLK);
    } else if (m == WM_LBUTTONDOWN) {  // .260: outside the emotions - the card dragged
        s.dragging = true;
        s.grabX = tx - s.offX;
        s.grabY = ty - s.offY;
    }
    return true;
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    const uint32_t w = gameEmotes();
    if (!s.shown || !w) return false;
    if (s.told++ < 30) LOG_INFO("emotions: closed (the window's 0x87)");
    reinterpret_cast<SendMsgFn>(uintptr_t(kEmoteSend))(w, 0, kButtonMsg, kCloseButton, 0, 0);
    s.key.clear();
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (!s.on) return;
    const uint32_t w = gameEmotes();
    if (w && read32(w + kShown) && write32(w + kShown, 0)) s.hid = true;
}

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.shown) return false;
    if (s.liveShown && s.live.box(x, y, w, h)) return true;  // 3.0.18-beta.73
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

const std::vector<panel::Picture>& emotionPictures(int side) { return picturesAt(side); }

int emotionNumber(const std::string& command) {
    using MsgStringFn = const char*(__cdecl*)(int line);
    static std::vector<std::string> list;
    if (list.empty()) {
        const auto add = [](int from, int to) {
            for (int i = from; i <= to; ++i) {
                const char* t = reinterpret_cast<MsgStringFn>(uintptr_t(0x504FB0))(i);
                list.push_back(t ? t : "");
            }
        };
        add(0x220, 0x23D);
        add(0x304, 0x30F);
        add(0x3B1, 0x3B7);
    }
    const auto it = std::find(list.begin(), list.end(), command);
    return it == list.end() ? -1 : int(it - list.begin());
}

}  // namespace uagfx::win::emotewin
