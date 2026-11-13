#include "win/hunt_panel.hpp"
#include "win/own_windows.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/char_status.hpp"
#include "core/hud_layout.hpp"
#include "core/hunt_stats.hpp"
#include "core/reference_index.hpp"
#include "core/settings/strings.hpp"
#include "core/skill_info.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/game_camera.hpp"
#include "win/hud_edit.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/uaui.hpp"
#include "win/ui_cost.hpp"
#include "win/window_input.hpp"
#include "win/words.hpp"

namespace uagfx::win::huntwin {
namespace {

using uapad::gamewindow::Blocker;
constexpr uint32_t kOwnAccount = 0x771F7C, kActorClass = 0x20C;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kText = 0xEEF2F8, kMuted = 0x9AA3B5, kGold = 0xE6CB8A;
constexpr float kW = 300, kLineH = 19, kPad = 10, kTitleH = 24;

struct State {
    bool on = false, shown = false, upload = false;
    hunt::Stats stats;
    std::unordered_map<uint32_t, std::pair<std::string, uint64_t>> fought;  // a monster hit: its name, when
    std::unordered_map<uint32_t, std::string> party;  // 3.0.13-beta.14: the party's members (account, name)
    int partyTold = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0, x = 0, y = 0, w = 0, h = 0;
    panel::Box reset;
    bool hoverReset = false;
    panel::Box close;         // 3.0.18-beta.120 (the concept «Ігровий інтерфейс»): ✕ - hidden till its icon or key
    bool hoverClose = false;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    int told = 0;
    bool uiShown = false;  // 3.0.18-beta.25: drawn by uaUI
    std::vector<std::string> lines;
    std::string viewKey;
};

State& state() {
    static State s;
    return s;
}

uint32_t own() {
    uint32_t v = 0;
    return readGame(kOwnAccount, &v, 4) ? v : 0;
}

std::string monsterName(uint32_t id) {
    const uint32_t actor = actorAddress(id);
    uint32_t cls = 0;
    if (!actor || !readGame(actor + kActorClass, &cls, 4)) return "";
    const auto index = reference::index();
    if (!index) return "";
    const auto m = index->mobs.find(int(cls));
    return m == index->mobs.end() ? std::string() : m->second.name;
}

// 3.0.18-beta.25 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 4 - by hand): the hunt's panel in
// RmlUi's markup - its card, the title, the lines (the first bright), «↺»; drawn only (passive): «↺» and the
// blocker stay this part's; sizes by its own scale (the interface editor's size too), not dp
const char* kDoc = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="hpn" data-model="hunt" data-style-width="w + 'px'" data-style-height="h + 'px'"
  data-style-border-radius="10 * s + 'px'" data-style-border-width="s + 'px'" data-style-background-color="ground">
<span class="title" data-style-left="10 * s + 'px'" data-style-top="10 * s + 'px'" data-style-width="w - 72 * s + 'px'"
  data-style-height="20 * s + 'px'" data-style-line-height="20 * s + 'px'" data-style-font-size="13 * s + 'px'"
  >{{title}}</span>
<span data-attr-class="closeHot ? 'ownx hot' : 'ownx'" data-style-left="w - 32 * s + 'px'"
  data-style-top="10 * s + 'px'" data-style-width="22 * s + 'px'" data-style-height="20 * s + 'px'"
  data-style-line-height="20 * s + 'px'" data-style-border-radius="6 * s + 'px'"
  data-style-font-size="12 * s + 'px'">&#x2715;</span>
<span data-for="l, i : lines" data-attr-class="i == 0 ? 'line first' : 'line'" data-style-left="10 * s + 'px'"
  data-style-top="(34 + 19 * i) * s + 'px'" data-style-width="w - 20 * s + 'px'" data-style-height="19 * s + 'px'"
  data-style-line-height="19 * s + 'px'" data-style-font-size="12 * s + 'px'">{{l}}</span>
<span data-attr-class="hot ? 'reset hot' : 'reset'" data-style-left="w - 58 * s + 'px'" data-style-top="10 * s + 'px'"
  data-style-width="22 * s + 'px'" data-style-height="20 * s + 'px'" data-style-line-height="20 * s + 'px'"
  data-style-border-radius="6 * s + 'px'" data-style-font-size="13 * s + 'px'">↺</span>
</body></rml>)RML";

struct Ui {
    uaui::Doc doc;
    float w = 0, h = 0, s = 1;
    bool hot = false, closeHot = false;
    Rml::String title, ground = "#00000000";  // 3.0.18-beta.76: never an empty style value
    std::vector<Rml::String> lines;
    std::string key;
};

Ui& ui() {
    static Ui u;
    return u;
}

bool uiFrame(State& s) {
    Ui& u = ui();
    if (!uaui::ready() || !s.shown || s.lines.empty()) {
        u.doc.show(false);
        return false;
    }
    if (!u.doc.made()) {
        u.doc.passive = true;
        if (!u.doc.make("hunt", kDoc, [&](Rml::DataModelConstructor& m) {
                m.RegisterArray<std::vector<Rml::String>>();
                m.Bind("w", &u.w);
                m.Bind("h", &u.h);
                m.Bind("s", &u.s);
                m.Bind("hot", &u.hot);
                m.Bind("closeHot", &u.closeHot);
                m.Bind("title", &u.title);
                m.Bind("ground", &u.ground);
                m.Bind("lines", &u.lines);
            }))
            return false;
    }
    if (s.viewKey != u.key) {
        u.key = s.viewKey;
        u.w = float(s.w), u.h = float(s.h), u.s = s.scale, u.hot = s.hoverReset, u.closeHot = s.hoverClose;
        char g[16];
        std::snprintf(g, sizeof g, "#%06x%02x", unsigned(uaui::themed(0x10141C)),
                      int(std::lround(uistyle::panel() * 255)) & 0xFF);
        u.ground = g;
        u.title = s.lines[0];
        u.lines.assign(s.lines.begin() + 1, s.lines.end());
        u.doc.dirtyAll();
    }
    u.doc.place(s.x, s.y);
    u.doc.show(true);
    return true;
}

}  // namespace

void packet(const packets::Event& e) {
    State& s = state();
    if (const auto* l = std::get_if<packets::PartyList>(&e)) {  // 3.0.13-beta.14: who is in the party
        s.party.clear();
        for (const packets::PartySeat& seat : l->seats) s.party[seat.account] = seat.name;
        return;
    }
    if (const auto* m = std::get_if<packets::PartyMember>(&e)) {
        s.party[m->account] = m->name;
        return;
    }
    if (const auto* gone = std::get_if<packets::PartyLeft>(&e)) {
        s.party.erase(gone->account);
        return;
    }
    if (!s.on) return;
    const uint64_t now = GetTickCount64();
    if (const auto* h = std::get_if<packets::Hit>(&e)) {
        const uint32_t me = own();
        if (me && h->source != me && h->target != me && s.party.count(h->source) && !s.party.count(h->target)) {
            s.stats.partyHit(h->source, s.party[h->source], h->damage, now);  // 3.0.13-beta.14: a mate's blow
            if (s.partyTold++ < 3) LOG_INFO("hunt: the party's damage counted - %s", s.party[h->source].c_str());
            return;
        }
        if (!me || h->source != me || h->target == me) return;
        s.stats.hit(h->damage, h->skill, now);
        if (s.fought.size() > 500) s.fought.clear();
        auto& f = s.fought[h->target];
        if (f.first.empty()) f.first = monsterName(h->target);
        f.second = now;
    } else if (const auto* v = std::get_if<packets::Vanish>(&e)) {
        const auto it = s.fought.find(v->id);
        if (v->type == 1 && it != s.fought.end() && now - it->second.second < 15000 && !it->second.first.empty())
            s.stats.kill(it->second.first);
        if (it != s.fought.end() && v->type == 1) s.fought.erase(it);
    }
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    (void)codePage;
    State& s = state();
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("hunt: [interface] hunt_panel = %d", on ? 1 : 0);
    }
    const auto hide = [&] {
        if (s.shown) uapad::gamewindow::setBlocked(Blocker::Hunt, nullptr, nullptr, false);
        s.shown = false;
        s.uiShown = uiFrame(s);
    };
    if (!on || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || !inGameWorld()) return hide();
    if (ownwin::closed(closedwin::Window::Hunt)) return hide();  // 3.0.18-beta.120: ✕
    const uint64_t now = GetTickCount64();
    charstatus::Status st;
    if (readOwnStatus(st)) s.stats.status(st.baseLevel, st.baseExp, st.baseNext, st.zeny, now);
    if (!s.stats.started()) s.stats.reset(now);
    const float scale = uistyle::partScale(k) * hudedit::sizeOf(hudlayout::Part::Hunt);
    s.k = float(k);
    s.scale = scale;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.w = int(std::lround(kW * scale));
    const int rows = s.stats.view(now).party.empty() ? 5 : 6;  // 3.0.13-beta.14: the party's line under the rest
    s.h = int(std::lround((2 * kPad + kTitleH + float(rows) * kLineH) * scale));
    s.x = targetWidth - s.w - int(std::lround(12 * scale));
    s.y = int(targetHeight * 0.52f);
    hudedit::place(hudlayout::Part::Hunt, float(k), targetWidth, targetHeight, s.w, s.h, s.x, s.y);
    s.close = panel::Box{float(s.w) - (kPad + 22) * scale, kPad * scale, 22 * scale, 20 * scale};
    s.reset = panel::Box{float(s.w) - (kPad + 48) * scale, kPad * scale, 22 * scale, 20 * scale};
    s.hoverReset = s.hoverClose = false;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy)) {
        const float px = float(cx) * float(k) - float(s.x), py = float(cy) * float(k) - float(s.y);
        s.hoverReset = s.reset.contains(px, py);
        s.hoverClose = s.close.contains(px, py);
    }
    const hunt::Texts t{say(settings::Text::HuntTitle),  say(settings::Text::HuntDps),
                        say(settings::Text::RecapAttack), say(settings::Text::HuntExp),
                        say(settings::Text::HuntToLevel), say(settings::Text::HuntZeny),
                        say(settings::Text::HuntKills),   "\xE2\x80\x94",
                        say(settings::Text::HuntParty),   say(settings::Text::HuntMe)};  // 3.0.13-beta.14
    const hunt::View v = s.stats.view(now);
    std::vector<std::string> names;
    for (const auto& [skill, share] : v.skills) names.push_back(skill ? skillinfo::name(skill) : "");
    const std::vector<std::string> lines = hunt::lines(v, t, names);
    std::string key = std::to_string(s.w) + 'x' + std::to_string(s.h) + (s.hoverReset ? "r" : "") +
                      (s.hoverClose ? "c" : "") + '#' +
                      std::to_string(uistyle::generation());
    for (const std::string& l : lines) key += '|' + l;
    s.lines = lines;
    s.viewKey = key;
    s.shown = true;
    s.uiShown = uiFrame(s);
    RECT client{};
    if (!GetClientRect(game, &client)) return;
    const RECT r = clientBox(client, targetWidth, targetHeight, s.x, s.y, s.w, s.h);
    uapad::gamewindow::setBlocked(Blocker::Hunt, game, &r, false);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    (void)record;  // the old edition (its picture) gone: uaUI draws it
    (void)renderer;
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    (void)wp;
    if (!s.shown) return false;
    const bool button = mouseButton(m);
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const float lx = tx - float(s.x), ly = ty - float(s.y);
    if (lx < 0 || ly < 0 || lx >= float(s.w) || ly >= float(s.h)) return false;
    result = 0;
    if (m == WM_LBUTTONDOWN && s.close.contains(lx, ly)) {  // 3.0.18-beta.120: ✕
        ownwin::setClosed(closedwin::Window::Hunt, true, "its ✕");
        s.key.clear();
        return true;
    }
    if (m == WM_LBUTTONDOWN && s.reset.contains(lx, ly)) {
        s.stats.reset(GetTickCount64());
        s.fought.clear();
        s.key.clear();
        if (s.told++ < 10) LOG_INFO("hunt: begun anew («↺»)");
    }
    return true;
}

std::vector<padpanels::Button> padButtons() {
    State& s = state();
    std::vector<padpanels::Button> out;
    if (!s.shown) return out;
    const auto at = [&](const panel::Box& b) { return panel::Box{float(s.x) + b.x, float(s.y) + b.y, b.w, b.h}; };
    out.push_back({at(s.reset), [] {
                       State& st = state();
                       st.stats.reset(GetTickCount64());
                       st.fought.clear();
                       st.key.clear();
                   }});
    out.push_back({at(s.close), [] { ownwin::setClosed(closedwin::Window::Hunt, true, "the pad"); }});
    return out;
}

}  // namespace uagfx::win::huntwin
