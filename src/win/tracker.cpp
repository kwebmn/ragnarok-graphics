#include "win/tracker.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/layout.hpp"
#include "core/reference_index.hpp"
#include "core/reference_pages.hpp"
#include "core/settings/strings.hpp"
#include "core/tracker_panel.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/buff_column.hpp"
#include "win/game_camera.hpp"
#include "win/hud_edit.hpp"
#include "win/log.hpp"
#include "win/minimap_proto.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/reference_window.hpp"
#include "win/uaui.hpp"
#include "win/ui_cost.hpp"
#include "win/window_input.hpp"
#include "win/words.hpp"

namespace uagfx::win::trackerwin {
namespace {

namespace tk = uagfx::tracker;

const std::string kPlotted = "#plot";  // 3.0.11-beta.71: the way set on the map - its entry's mark

struct State {
    bool on = false, shown = false, stateRead = false;
    bool folded = false, hidden = false;
    std::vector<std::string> quests;  // the pinned quests' slugs, the first pinned first
    std::vector<int> items;           // the pinned items to collect
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    std::vector<tk::Entry> entries;
    std::vector<std::string> routeOf;  // an entry's map (the way's target) - its card by a click
    tk::Layout layout;
    tk::Hit hover;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int clicksLogged = 0;
    bool uiShown = false;  // 3.0.18-beta.25: drawn by uaUI
    // 3.0.18-beta.145 (the tester did not see what it is for): shown by its icon with nothing pinned - what to
    // pin, for 8 s
    ULONGLONG emptyHintUntil = 0;
    std::string viewKey;
};

State& state() {
    static State s;
    return s;
}

void readState(State& s) {
    if (s.stateRead) return;
    s.stateRead = true;
    std::string text;
    if (!readTextFile(layout::join(gameDir(), layout::kTrackerState), text)) return;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("quest ", 0) == 0 && line.size() > 6 && s.quests.size() < 16) s.quests.push_back(line.substr(6));
        if (line.rfind("item ", 0) == 0 && s.items.size() < 16) s.items.push_back(std::atoi(line.c_str() + 5));
        if (line == "folded 1") s.folded = true;
        if (line == "hidden 1") s.hidden = true;
    }
}

void keepState(const State& s) {
    std::string text;
    for (const std::string& q : s.quests) text += "quest " + q + "\n";
    for (const int i : s.items) text += "item " + std::to_string(i) + "\n";
    text += std::string("folded ") + (s.folded ? "1" : "0") + "\nhidden " + (s.hidden ? "1" : "0") + "\n";
    const std::string path = layout::join(gameDir(), layout::kTrackerState);
    if (!writeFileAtomic(path, text)) LOG_INFO("tracker: %s cannot be written", path.c_str());
}

void block(const State& s, HWND game) {  // the mouse's clicks over it are its own
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const float x0 = float(s.layout.x), y0 = float(s.layout.y);
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + float(s.layout.w)) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + float(s.layout.h)) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Tracker, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Tracker, nullptr, nullptr, false);
    s.shown = false;
    s.hover = tk::Hit{};
    s.uiShown = false;
}

// Its entries: the pinned quests, the way the minimap leads, the pinned items
void build(State& s) {
    s.entries.clear();
    s.routeOf.clear();
    const settings::Lang l = language();
    const std::shared_ptr<const refindex::Index> ix = reference::index();
    std::vector<tk::Entry> pinned;
    if (ix) pinned = refpages::trackedEntries(*ix, l, s.quests, s.items);
    size_t at = 0;
    for (; at < pinned.size() && pinned[at].kind == tk::Kind::Quest; ++at) {
        s.entries.push_back(pinned[at]);
        s.routeOf.emplace_back();
    }
    int gx = 0, gy = 0;  // 3.0.11-beta.71: the way set on the map by clicks
    if (minimapproto::plotted(gx, gy)) {
        const std::string here = readGameMap();
        tk::Entry way{tk::Kind::Route, ix && !here.empty() ? refindex::mapTitle(*ix, here) : here,
                      say(settings::Text::TrackerRoute), {}};
        const auto lines = minimapproto::plotLines();
        if (!lines.empty()) way.lines.push_back(tk::Line{lines.front().first, lines.front().second, false});
        s.entries.push_back(std::move(way));
        s.routeOf.push_back(kPlotted);
    }
    std::string target;
    bool walk = true;
    if (minimapproto::following(target, walk)) {
        tk::Entry way{tk::Kind::Route, ix ? refindex::mapTitle(*ix, target) : target, say(settings::Text::TrackerRoute),
                      {}};
        for (const auto& [left, right] : minimapproto::routeLines()) way.lines.push_back(tk::Line{left, right, false});
        s.entries.push_back(std::move(way));
        s.routeOf.push_back(target);
    }
    for (; at < pinned.size(); ++at) {
        s.entries.push_back(pinned[at]);
        s.routeOf.emplace_back();
    }
}

// Its place: left of the buffs' column (its glass), its top in line; else under the new minimap at the right edge
void placeOf(const State& s, int& right, int& top) {
    int x = 0, y = 0;
    if (buffcolumn::glassBox(x, y)) {
        right = x - int(std::lround(10 * s.scale));
        top = y;
        return;
    }
    int mx = 0, my = 0, mw = 0, mh = 0;
    if (minimapproto::panelBox(mx, my, mw, mh)) {
        right = mx + mw;
        top = my + mh + int(std::lround(8 * s.scale));
        return;
    }
    right = s.targetW - int(std::lround(10 * s.scale));
    top = int(std::lround(150 * s.k));
}

// 3.0.18-beta.25 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 4 - by hand): the tracker in RmlUi's
// markup - its glass, the header's line, the entries' plates (the one pointed at lighter), ▾/✕ lit when pointed at,
// «ще N», the words where core tracker places them; drawn only (passive): its clicks and blocker stay this part's
const char* kDoc = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="trk" data-model="tracker" data-style-width="w + 'px'" data-style-height="h + 'px'">
<div class="glass" data-style-left="g.x + 'px'" data-style-top="g.y + 'px'" data-style-width="g.w + 'px'"
  data-style-height="g.h + 'px'" data-style-border-radius="g.r + 'px'" data-style-border-width="g.e + 'px'"
  data-style-background-color="ground"></div>
<div data-for="b : boxes" data-attr-class="b.cls" data-style-left="b.x + 'px'" data-style-top="b.y + 'px'"
  data-style-width="b.w + 'px'" data-style-height="b.h + 'px'" data-style-border-radius="b.r + 'px'"
  data-style-border-width="b.e + 'px'"></div>
<span data-for="t : words" data-attr-class="t.cls" data-style-left="t.x + 'px'" data-style-top="t.y + 'px'"
  data-style-width="t.w + 'px'" data-style-height="t.h + 'px'" data-style-line-height="t.h + 'px'"
  data-style-font-size="t.px + 'px'" data-style-color="t.colour">{{t.text}}</span>
</body></rml>)RML";

struct BoxV {
    float x = 0, y = 0, w = 0, h = 0, r = 0, e = 0;
    Rml::String cls;
};
struct WordV {
    float x = 0, y = 0, w = 0, h = 0, px = 12;
    Rml::String text, colour = "#eceff5", cls;
};

struct Ui {
    uaui::Doc doc;
    float w = 0, h = 0;
    BoxV g;
    Rml::String ground = "#00000000";  // 3.0.18-beta.65: never an empty style value
    std::vector<BoxV> boxes;
    std::vector<WordV> words;
    std::string key;
};

Ui& ui() {
    static Ui u;
    return u;
}

bool uiFrame(State& s) {
    Ui& u = ui();
    if (!uaui::ready() || !s.shown) {
        u.doc.show(false);
        return false;
    }
    if (!u.doc.made()) {
        u.doc.passive = true;
        if (!u.doc.make("tracker", kDoc, [&](Rml::DataModelConstructor& m) {
                if (auto b = m.RegisterStruct<BoxV>()) {
                    b.RegisterMember("x", &BoxV::x);
                    b.RegisterMember("y", &BoxV::y);
                    b.RegisterMember("w", &BoxV::w);
                    b.RegisterMember("h", &BoxV::h);
                    b.RegisterMember("r", &BoxV::r);
                    b.RegisterMember("e", &BoxV::e);
                    b.RegisterMember("cls", &BoxV::cls);
                }
                if (auto t = m.RegisterStruct<WordV>()) {
                    t.RegisterMember("x", &WordV::x);
                    t.RegisterMember("y", &WordV::y);
                    t.RegisterMember("w", &WordV::w);
                    t.RegisterMember("h", &WordV::h);
                    t.RegisterMember("px", &WordV::px);
                    t.RegisterMember("text", &WordV::text);
                    t.RegisterMember("colour", &WordV::colour);
                    t.RegisterMember("cls", &WordV::cls);
                }
                m.RegisterArray<std::vector<BoxV>>();
                m.RegisterArray<std::vector<WordV>>();
                m.Bind("w", &u.w);
                m.Bind("h", &u.h);
                m.Bind("g", &u.g);
                m.Bind("ground", &u.ground);
                m.Bind("boxes", &u.boxes);
                m.Bind("words", &u.words);
            }))
            return false;
    }
    if (s.viewKey != u.key) {
        u.key = s.viewKey;
        const tk::Layout& L = s.layout;
        const float sc = L.s;
        u.w = float(L.w), u.h = float(L.h);
        u.boxes.clear();
        const auto box = [&](const panel::Box& b, float r, const char* cls, float edge = 0) {
            u.boxes.push_back(BoxV{b.x, b.y, b.w, b.h, r, edge, cls});
        };
        u.g = BoxV{L.glass.x, L.glass.y, L.glass.w, L.glass.h, 10 * sc, 1.0f * sc, "glass"};
        char ground[16];
        std::snprintf(ground, sizeof ground, "#%06x%02x", unsigned(uaui::themed(0x10141C)),
                      int(std::lround(uistyle::panel() * 255)) & 0xFF);
        u.ground = ground;
        box(panel::Box{0, 34 * sc - 1.0f * sc, float(L.w), 1.0f * sc}, 0, "rule");  // under the header
        if (s.hover.part == tk::Part::Fold) box(L.fold, 6 * sc, "hot");
        if (s.hover.part == tk::Part::Hide) box(L.hide, 6 * sc, "hot");
        for (size_t i = 0; i < L.entries.size(); ++i) {
            const tk::Placed& p = L.entries[i];
            const bool over = s.hover.entry == int(i) &&
                              (s.hover.part == tk::Part::Head || s.hover.part == tk::Part::Close);
            box(p.box, 8 * sc, over ? "entry over" : "entry");
            if (s.hover.part == tk::Part::Close && s.hover.entry == int(i)) box(p.close, 6 * sc, "hot");
        }
        if (L.more.w > 0) box(L.more, 7 * sc, "more");
        u.words.clear();
        char colour[16];
        for (const tk::Words& w : tk::words(L, s.entries,
                                            tk::Texts{say(settings::Text::TrackerTitle),
                                                      say(settings::Text::TrackerMore)},
                                            s.folded)) {
            std::snprintf(colour, sizeof colour, "#%06x", unsigned(uaui::themed(w.rgb) & 0xFFFFFF));
            const char* align = w.align == tk::Align::Right    ? " right"
                                : w.align == tk::Align::Center ? " centre"
                                                               : "";
            u.words.push_back(WordV{w.box.x, w.box.y, w.box.w, w.box.h, w.px, w.text, colour,
                                    std::string(w.bold ? "word bold" : "word") + align});
        }
        u.doc.dirtyAll();
    }
    u.doc.place(s.layout.x, s.layout.y);
    u.doc.show(true);
    return true;
}

}  // namespace

void frame(HWND game, bool on, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (on != s.on) {
        s.on = on;
        LOG_INFO("tracker: [interface] tracker = %d", on ? 1 : 0);
    }
    readState(s);
    if (!on || s.hidden || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || !inGameWorld()) {
        hide(s);
        uiFrame(s);
        return;
    }
    s.k = float(k);
    s.scale = uistyle::partScale(k) * hudedit::sizeOf(hudlayout::Part::Tracker);  // 3.0.11-beta.75: its size
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    build(s);
    if (s.entries.empty() && GetTickCount64() < s.emptyHintUntil) {  // 3.0.18-beta.145: what to pin, a while
        tk::Entry e;
        e.kind = tk::Kind::Route;
        e.title = say(settings::Text::TrackerEmptyTitle);
        e.tag = say(settings::Text::TrackerEmptyTag);
        e.lines.push_back(tk::Line{say(settings::Text::TrackerEmptyHint), "", false});
        s.entries.push_back(std::move(e));
        s.routeOf.push_back("");
    }
    if (s.entries.empty()) {  // nothing pinned: no panel
        hide(s);
        uiFrame(s);
        return;
    }
    int right = 0, top = 0;
    placeOf(s, right, top);
    s.layout = tk::layout(s.entries, s.folded, right, top, targetHeight - int(std::lround(50 * k)), s.scale);
    hudedit::place(hudlayout::Part::Tracker, float(k), targetWidth, targetHeight, s.layout.w, s.layout.h, s.layout.x,
                   s.layout.y);  // 3.0.11-beta.75: where the player put it
    tk::Hit hover;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy))
        hover = tk::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                            float(cy) * float(k) - float(s.layout.y));
    s.hover = hover;
    // 3.0.11-beta.79 (the owner: «когда тяну мышкой интерфейс то окошко скачет сильно»): its place not in it - a
    // picture is drawn where it stands each frame, a move alone paints nothing again
    std::string key = std::to_string(s.layout.w) + 'x' + std::to_string(s.layout.h) + ';' + std::to_string(s.scale) +
                      ';' + std::to_string(int(hover.part)) + ',' + std::to_string(hover.entry) + ';' +
                      (s.folded ? "f" : "") + '#' +
                      std::to_string(uistyle::generation()) + ';';
    for (const tk::Entry& e : s.entries) {
        key += e.title + '|' + e.tag + '|';
        for (const tk::Line& ln : e.lines) key += ln.text + '/' + ln.right + '|';
    }
    s.viewKey = key;
    if (key != s.key && !uaui::ready()) {  // 3.0.18-beta.25: uaUI's panel needs no picture
        s.key = key;
    }
    s.shown = true;
    s.uiShown = uiFrame(s);
    block(s, game);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    (void)record;  // the old edition (its picture) gone: uaUI draws it
    (void)renderer;
}

// A press on a part of the panel: the mouse, 3.0.18-beta.135 the pad (win/pad_panels)
static void pressPart(State& s, tk::Part part, int entry) {
    const bool known = entry >= 0 && size_t(entry) < s.entries.size();
    const tk::Entry* e = known ? &s.entries[size_t(entry)] : nullptr;
    if (part == tk::Part::Fold) {
        s.folded = !s.folded;
        keepState(s);
    } else if (part == tk::Part::Hide) {
        s.hidden = true;
        keepState(s);
        LOG_INFO("tracker: hidden - shown again by the next pin");
    } else if (part == tk::Part::Close && e) {
        if (e->kind == tk::Kind::Route && s.routeOf[size_t(entry)].empty() && s.emptyHintUntil) {
            s.emptyHintUntil = 0;  // 3.0.18-beta.145: the hint of what to pin, shut
        } else if (e->kind == tk::Kind::Route && s.routeOf[size_t(entry)] == kPlotted) {
            minimapproto::clearPlot();  // 3.0.11-beta.71
        } else if (e->kind == tk::Kind::Route) {
            minimapproto::unfollow();
        } else if (e->kind == tk::Kind::Quest) {
            size_t q = 0;  // the quests come first, in the pins' order
            for (size_t i = 0; i < size_t(entry); ++i) q += s.entries[i].kind == tk::Kind::Quest;
            if (q < s.quests.size()) s.quests.erase(s.quests.begin() + std::ptrdiff_t(q));
        } else {
            size_t n = 0;
            for (size_t i = 0; i < size_t(entry); ++i) n += s.entries[i].kind == tk::Kind::Item;
            if (n < s.items.size()) s.items.erase(s.items.begin() + std::ptrdiff_t(n));
        }
        keepState(s);
    } else if (part == tk::Part::Head && e) {
        size_t n = 0;  // the entry's own pin, by its kind's order
        for (size_t i = 0; i < size_t(entry); ++i) n += s.entries[i].kind == e->kind;
        if (e->kind == tk::Kind::Route && !s.routeOf[size_t(entry)].empty() &&
            s.routeOf[size_t(entry)] != kPlotted)
            referencewin::openMap(s.routeOf[size_t(entry)]);
        else if (e->kind == tk::Kind::Quest && n < s.quests.size())
            referencewin::openQuest(s.quests[n]);
        else if (e->kind == tk::Kind::Item && n < s.items.size())
            referencewin::openItem(s.items[n]);
    }
    s.key.clear();
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = mouseButton(m);
    if (!button && m != WM_MOUSEWHEEL) return false;
    (void)wp;
    float tx = 0, ty = 0;  // where the game's cursor stands, as the hover (3.0.11-beta.43's)
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty)) return false;
    const tk::Hit h = tk::hitTest(s.layout, tx - float(s.layout.x), ty - float(s.layout.y));
    if (h.part == tk::Part::None) return false;
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) {
        pressPart(s, h.part, h.entry);
        if (s.clicksLogged++ < 12)
            LOG_INFO("tracker: a click on part %d of entry %d", int(h.part), h.entry);
        s.key.clear();
    }
    result = 0;
    return true;
}

bool hiddenNow() {
    State& s = state();
    readState(s);
    return s.hidden;
}

void toggleHidden(const char* why) {
    State& s = state();
    readState(s);
    s.hidden = !s.hidden;
    keepState(s);
    s.key.clear();
    s.emptyHintUntil = s.hidden ? 0 : GetTickCount64() + 8000;  // 3.0.18-beta.145: nothing pinned - what to pin
    LOG_INFO("tracker: %s (%s)", s.hidden ? "hidden" : "shown", why);
}

void toggleQuest(const std::string& slug) {
    State& s = state();
    readState(s);
    const auto it = std::find(s.quests.begin(), s.quests.end(), slug);
    const bool was = it != s.quests.end();
    if (was) {
        s.quests.erase(it);
    } else {
        s.quests.push_back(slug);
        s.hidden = false;  // a new pin shows the panel again
    }
    keepState(s);
    LOG_INFO("tracker: the quest %s %s", slug.c_str(), was ? "let go" : "pinned");
}

void toggleItem(int id) {
    State& s = state();
    readState(s);
    const auto it = std::find(s.items.begin(), s.items.end(), id);
    if (it != s.items.end()) {
        s.items.erase(it);
    } else {
        s.items.push_back(id);
        s.hidden = false;
    }
    keepState(s);
}

const std::vector<std::string>& quests() {
    State& s = state();
    readState(s);
    return s.quests;
}

const std::vector<int>& items() {
    State& s = state();
    readState(s);
    return s.items;
}

std::vector<padpanels::Button> padButtons() {
    State& s = state();
    std::vector<padpanels::Button> out;
    if (!s.shown) return out;
    const tk::Layout& l = s.layout;
    const auto add = [&](const panel::Box& b, tk::Part part, int entry) {
        if (b.w <= 0 || b.h <= 0) return;
        out.push_back({panel::Box{float(l.x) + b.x, float(l.y) + b.y, b.w, b.h},
                       [part, entry] { pressPart(state(), part, entry); }});
    };
    add(l.fold, tk::Part::Fold, -1);
    add(l.hide, tk::Part::Hide, -1);
    for (size_t i = 0; i < l.entries.size(); ++i) {
        add(l.entries[i].head, tk::Part::Head, int(i));
        add(l.entries[i].close, tk::Part::Close, int(i));
    }
    return out;
}

}  // namespace uagfx::win::trackerwin
