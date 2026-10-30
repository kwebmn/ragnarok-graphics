#include "win/tracker.hpp"

#include <algorithm>
#include <cmath>
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
#include "win/log.hpp"
#include "win/minimap_proto.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/reference_window.hpp"
#include "win/ui_cost.hpp"

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
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(startup().lang, t); }

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

std::vector<Words> gdiWords(const std::vector<tk::Words>& in) {
    std::vector<Words> out;
    for (const tk::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == tk::Align::Right    ? DT_RIGHT
                                 : w.align == tk::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("tracker", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    tk::paint(s.picture, s.layout, s.entries, s.hover);
    drawWords(s.picture, gdiWords(tk::words(s.layout, s.entries,
                                            tk::Texts{say(settings::Text::TrackerTitle),
                                                      say(settings::Text::TrackerMore)},
                                            s.folded)));
    s.upload = true;
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
}

// Its entries: the pinned quests, the way the minimap leads, the pinned items
void build(State& s) {
    s.entries.clear();
    s.routeOf.clear();
    const settings::Lang l = startup().lang;
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
        return;
    }
    s.k = float(k);
    s.scale = uistyle::partScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    build(s);
    if (s.entries.empty()) {  // nothing pinned: no panel
        hide(s);
        return;
    }
    int right = 0, top = 0;
    placeOf(s, right, top);
    s.layout = tk::layout(s.entries, s.folded, right, top, targetHeight - int(std::lround(50 * k)), s.scale);
    tk::Hit hover;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy))
        hover = tk::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                            float(cy) * float(k) - float(s.layout.y));
    s.hover = hover;
    std::string key = std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ',' + std::to_string(s.layout.h) +
                      ';' + std::to_string(s.scale) + ';' + std::to_string(int(hover.part)) + ',' +
                      std::to_string(hover.entry) + ';' + (s.folded ? "f" : "") + '#' +
                      std::to_string(uistyle::generation()) + ';';
    for (const tk::Entry& e : s.entries) {
        key += e.title + '|' + e.tag + '|';
        for (const tk::Line& ln : e.lines) key += ln.text + '/' + ln.right + '|';
    }
    if (key != s.key) {
        s.key = key;
        render(s);
    }
    s.shown = true;
    block(s, game);
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
    (void)wp;
    float tx = 0, ty = 0;  // where the game's cursor stands, as the hover (3.0.11-beta.43's)
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty)) return false;
    const tk::Hit h = tk::hitTest(s.layout, tx - float(s.layout.x), ty - float(s.layout.y));
    if (h.part == tk::Part::None) return false;
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) {
        const bool known = h.entry >= 0 && size_t(h.entry) < s.entries.size();
        const tk::Entry* e = known ? &s.entries[size_t(h.entry)] : nullptr;
        if (h.part == tk::Part::Fold) {
            s.folded = !s.folded;
            keepState(s);
        } else if (h.part == tk::Part::Hide) {
            s.hidden = true;
            keepState(s);
            LOG_INFO("tracker: hidden - shown again by the next pin");
        } else if (h.part == tk::Part::Close && e) {
            if (e->kind == tk::Kind::Route && s.routeOf[size_t(h.entry)] == kPlotted) {
                minimapproto::clearPlot();  // 3.0.11-beta.71
            } else if (e->kind == tk::Kind::Route) {
                minimapproto::unfollow();
            } else if (e->kind == tk::Kind::Quest) {
                size_t q = 0;  // the quests come first, in the pins' order
                for (size_t i = 0; i < size_t(h.entry); ++i) q += s.entries[i].kind == tk::Kind::Quest;
                if (q < s.quests.size()) s.quests.erase(s.quests.begin() + std::ptrdiff_t(q));
            } else {
                size_t n = 0;
                for (size_t i = 0; i < size_t(h.entry); ++i) n += s.entries[i].kind == tk::Kind::Item;
                if (n < s.items.size()) s.items.erase(s.items.begin() + std::ptrdiff_t(n));
            }
            keepState(s);
        } else if (h.part == tk::Part::Head && e) {
            size_t n = 0;  // the entry's own pin, by its kind's order
            for (size_t i = 0; i < size_t(h.entry); ++i) n += s.entries[i].kind == e->kind;
            if (e->kind == tk::Kind::Route && !s.routeOf[size_t(h.entry)].empty() &&
                s.routeOf[size_t(h.entry)] != kPlotted)
                referencewin::openMap(s.routeOf[size_t(h.entry)]);
            else if (e->kind == tk::Kind::Quest && n < s.quests.size())
                referencewin::openQuest(s.quests[n]);
            else if (e->kind == tk::Kind::Item && n < s.items.size())
                referencewin::openItem(s.items[n]);
        }
        if (s.clicksLogged++ < 12)
            LOG_INFO("tracker: a click on part %d of entry %d", int(h.part), h.entry);
        s.key.clear();
    }
    result = 0;
    return true;
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

}  // namespace uagfx::win::trackerwin
