#include "win/sound_hook.hpp"
#include "win/party_panel.hpp"
#include "win/own_windows.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "core/ui_glide.hpp"
#include "core/party_alarm.hpp"
#include "core/ui_chime.hpp"
#include "core/buff_timers.hpp"
#include "core/char_block.hpp"
#include "core/layout.hpp"
#include "core/party_panel.hpp"
#include "core/reference_index.hpp"
#include "core/ai/texture_cache.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/buff_column.hpp"
#include "win/char_block.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/game_files.hpp"
#include "win/guild_card.hpp"
#include "win/head_picture.hpp"
#include "win/hud_edit.hpp"
#include "win/log.hpp"
#include "win/minimap_proto.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_window.hpp"
#include "win/ui_cost.hpp"
#include "win/uaui.hpp"
#include "core/ui_icons.hpp"

namespace uagfx::win::partywin {
namespace {

namespace pp = uagfx::partypanel;

constexpr uint32_t kOwnAccount = 0x771F7C;
// The game's Basic Info window (UI window 0) when the character block is off: the panel under it
constexpr uint32_t kBasicInfo = 0x727ED8 + 0x90, kShown = 0x38, kRect = 0x14;
constexpr int kLow = 30;  // per cent: red under it (as [notify] party_hp's default)
// 3.0.11-beta.82 (the owner's 2): the game mode waiting for a skill's target (its class's vtable; FUN_00561c20 - a
// click on a character): its state +0x3C4 (1 a place, 2 an enemy, 4 a friend), the skill +0x3C8, its range +0x3CC, its
// level +0x3D0; the world +0xC4, its player +0x3C, whose SendMsg (vtable + 8) takes 0x29 (a target's id, the skill,
// the range) then 0x5A (the level) - or for a place 0x41 (the skill, x, y)
constexpr uint32_t kModeClass = 0x69F388, kWaitState = 0x3C4, kWaitSkill = 0x3C8, kWaitRange = 0x3CC;
constexpr uint32_t kWaitLevel = 0x3D0, kWorld = 0xC4, kPlayer = 0x3C;
constexpr int kUseOn = 0x29, kUseLevel = 0x5A, kUsePlace = 0x41;
constexpr ULONGLONG kPosMs = 20000;  // a member's cell this fresh still shown
using ActorMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

struct Seat {
    std::string name, map;  // the game's code page, as they came
    bool leader = false, online = true;
    int hp = -1, most = 0;
    // 3.0.11-beta.81 (the owner: «1-8 делай»): seen dying and not back yet; its cast (0x013e) - the skill, when, how
    // long
    bool dead = false;
    int castSkill = 0;
    ULONGLONG castAt = 0;
    uint32_t castMs = 0;
    // 3.0.11-beta.82: its cell as the server told the party (0x0107) and when
    int x = -1, y = -1;
    ULONGLONG posAt = 0;
};

// 3.0.11-beta.80 (the owner: «иконки можно сделать как лица персонажей?»): a member's looks as last seen in sight
// (0x022a/b/c) - its face made of them (win/head_picture); kept across games
struct Look {
    int sex = 0, hair = 0, colour = 0, job = 0, level = 0;
    bool operator==(const Look& o) const {
        return sex == o.sex && hair == o.hair && colour == o.colour && job == o.job && level == o.level;
    }
};

// 3.0.11-beta.80 (the owner: «чтобы видить какие касты стоят у персонажей»): a member's buffs as the server told them
// to those near it (0x0196, uAthena's clif_status_change sends AREA), their times by the casts seen (0x011a, as the
// character's own - core/buff_timers)
struct Buffs {
    buffs::Timers timers;
    std::map<uint16_t, uint64_t> on;    // icon -> when put on
    std::map<uint16_t, uint64_t> mine;  // 3.0.11-beta.81: the character's own casts on it - skill -> when
};

struct State {
    partyalarm::Watch alarm;  // 3.0.11-beta.282: the healer's alarm ([interface] party_alarm)
    bool alarmOn = true;
    int alarmTold = 0;
    std::map<uint32_t, Buffs> buffs;  // 3.0.11-beta.80: by the account
    std::vector<uint32_t> order;
    std::map<uint32_t, Seat> seats;
    std::map<uint32_t, Look> looks;  // 3.0.11-beta.80: by the account
    bool looksRead = false;
    ULONGLONG looksChangedAt = 0;    // to keep (0: kept)
    std::map<std::string, panel::Picture> faces;  // by sex:hair:colour:side
    int facesTold = 0;
    std::vector<uint32_t> rowsOf;  // 3.0.11-beta.82: each row's account (a click's member)
    unsigned codePage = 0;         // 3.0.11-beta.83: the names' (the minimap's members)
    std::map<std::string, std::string> mapNames;  // 3.0.11-beta.200: the client's data\mapnametable.txt, read once
    bool namesRead = false;
    bool on = false, shown = false;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    std::vector<pp::Member> members;
    pp::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
    // 3.0.11-beta.79 (the owner: «плашка с пати её надо иметь возможность сворачивать»): folded to its header, kept
    bool folded = false, stateRead = false;
    bool compact = false;  // 3.0.11-beta.84 (the owner's 8): two columns of small cells, kept
    bool uiShown = false;  // 3.0.18-beta.19: drawn by uaUI
};

State& state() {
    static State s;
    return s;
}

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

uint32_t own() { return read32(kOwnAccount); }

void readLooks(State& s) {
    if (s.looksRead) return;
    s.looksRead = true;
    std::string text;
    if (!readTextFile(layout::join(gameDir(), layout::kPartyLooks), text)) return;
    std::istringstream in(text);
    uint32_t account = 0;
    Look l;
    while (in >> account >> l.sex >> l.hair >> l.colour >> l.job >> l.level) s.looks[account] = l;
}

void keepLooks(State& s) {
    std::string text;
    for (const auto& [account, l] : s.looks)
        text += std::to_string(account) + ' ' + std::to_string(l.sex) + ' ' + std::to_string(l.hair) + ' ' +
                std::to_string(l.colour) + ' ' + std::to_string(l.job) + ' ' + std::to_string(l.level) + '\n';
    if (!writeFileAtomic(layout::join(gameDir(), layout::kPartyLooks), text))
        LOG_INFO("party: %s cannot be written", layout::kPartyLooks);
}

// A member's face: its head as last seen, made once a look and a size
const panel::Picture* faceOf(State& s, uint32_t account, int side, const std::string& name) {
    auto it = s.looks.find(account);
    if (it == s.looks.end()) {  // 3.0.11-beta.263: not seen yet - a guild mate's looks from the guild's list
        guildcardwin::MateLook g;
        if (guildcardwin::mateLook(account, g)) {
            it = s.looks.emplace(account, Look{g.sex, g.hair, g.colour, g.job, g.level}).first;
            if (!s.looksChangedAt) s.looksChangedAt = GetTickCount64();
            if (s.facesTold < 20)
                LOG_INFO("party: %s's looks from the guild's list (not seen yet)", name.c_str());
        }
    }
    if (it == s.looks.end() || side <= 0) return nullptr;
    const Look& l = it->second;
    const std::string key = std::to_string(l.sex) + ':' + std::to_string(l.hair) + ':' + std::to_string(l.colour) +
                            ':' + std::to_string(side);
    auto f = s.faces.find(key);
    if (f == s.faces.end()) {
        if (s.faces.size() > 64) s.faces.clear();  // a long game's many parties never pile up
        f = s.faces.emplace(key, panel::Picture{}).first;
        std::string from;
        const bool made = headpicture::make(l.sex, l.hair, l.colour, side, f->second, &from);
        if (s.facesTold < 20) {
            ++s.facesTold;
            LOG_INFO("party: %s's face - %s", name.c_str(), made ? from.c_str() : (from + ", its letter").c_str());
        }
    }
    return f->second.width > 0 ? &f->second : nullptr;
}

void readState(State& s) {
    if (s.stateRead) return;
    s.stateRead = true;
    std::string text;
    if (readTextFile(layout::join(gameDir(), layout::kPartyState), text)) {
        s.folded = text.find("folded 1") != std::string::npos;
        s.compact = text.find("view compact") != std::string::npos;  // 3.0.11-beta.84
    }
}

void keepState(const State& s) {
    const std::string text = std::string("folded ") + (s.folded ? "1" : "0") + "\nview " +
                             (s.compact ? "compact" : "list") + "\n";
    writeFileAtomic(layout::join(gameDir(), layout::kPartyState), text);
}

// 3.0.11-beta.200: the client's names of the maps (the members elsewhere: «Payon Cave 3», not «pay_dun03.rsw»)
const std::map<std::string, std::string>& mapNames(State& s) {
    if (!s.namesRead) {
        s.namesRead = true;
        gamefiles::Reader files(gameDir());
        std::vector<uint8_t> bytes;
        if (files.read("data\\mapnametable.txt", bytes, nullptr))
            s.mapNames = refindex::mapNames(std::string(bytes.begin(), bytes.end()));
    }
    return s.mapNames;
}

void take(State& s, uint32_t account, const std::string& name, const std::string& map, bool leader, bool online) {
    if (!s.seats.count(account)) s.order.push_back(account);
    Seat& seat = s.seats[account];
    seat.name = name;
    // 3.0.11-beta.200: the party's packets name a map with its file's ending («pay_dun03.rsw»), the game's own map
    // has none - compared and shown by its code
    if (!map.empty()) seat.map = ai::mapName(map.c_str(), map.size());
    seat.leader = leader;
    seat.online = online;
}

void uiRect(float& x, float& y, float& w, float& h);

void block(const State& s, HWND game) {  // the mouse's clicks over it are its own
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    panel::Box b = s.layout.glass;
    float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    uiRect(x0, y0, b.w, b.h);  // 3.0.18-beta.50 (R-VMGJE8): as uaUI draws it - its clicks and the editor's frame
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Party, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Party, nullptr, nullptr, false);
    s.shown = false;
}

// Its top: under the character block when it shows, else under the game's own Basic Info window
int topOf(double k, float scale) {
    int x = 0, y = 0, w = 0, h = 0;
    if (charpanel::panelBox(x, y, w, h)) return y + h + int(std::lround(8 * scale));
    const uint32_t window = read32(kBasicInfo);
    int32_t r[4] = {0, 0, 0, 0};
    if (window && read32(window + kShown) && readGame(window + kRect, r, sizeof r) && r[3] > 0 && r[1] + r[3] < 400)
        return int(std::lround(double(r[1] + r[3]) * k)) + int(std::lround(8 * scale));
    return int(std::lround(10 * scale));
}

// 3.0.11-beta.82: where the camera looks along the ground (as the minimap's: its view's forward east and north)
bool readLook(float& east, float& north) {
    float view[12];
    if (!readGameCamera(view)) return false;
    const float x = view[2], z = view[8], len = std::hypot(x, z);
    if (len < 1e-3f) return false;
    east = x / len;
    north = z / len;
    return true;
}

// The game mode waiting for a skill's target: its state (1 a place, 2 an enemy, 4 a friend); 0 none
int waitingState(uint32_t& mode) {
    mode = readGameMode();
    if (!mode || read32(mode) != kModeClass) return 0;
    const int state = int(read32(mode + kWaitState));
    return state == 1 || state == 2 || state == 4 ? state : 0;
}

// 3.0.11-beta.82 (the owner's 2: «клик по участнику — навык на него»): the skill waited for cast on a member as a click
// on it in the world does (FUN_00561c20) - a place one on its cell; false: none waited, or no cell of it
bool castOn(uint32_t account, const Seat& seat, std::string& said) {
    uint32_t mode = 0;
    const int state = waitingState(mode);
    if (!state) return false;
    const uint32_t world = read32(mode + kWorld), player = world ? read32(world + kPlayer) : 0;
    const uint32_t vtable = player ? read32(player) : 0, fn = vtable ? read32(vtable + 8) : 0;
    if (!fn || fn < 0x401000 || fn >= 0x6A0000) return false;
    const int skill = int(read32(mode + kWaitSkill)), range = int(read32(mode + kWaitRange));
    const int level = int(read32(mode + kWaitLevel));
    const auto msg = reinterpret_cast<ActorMsgFn>(uintptr_t(fn));
    if (state == 1) {  // a place: the member's cell
        if (seat.x < 0 || GetTickCount64() - seat.posAt > kPosMs) return false;
        msg(player, 0, kUsePlace, skill, seat.x, seat.y);
        said = "skill " + std::to_string(skill) + " Lv" + std::to_string(level) + " on its cell " +
               std::to_string(seat.x) + ", " + std::to_string(seat.y);
    } else {
        msg(player, 0, kUseOn, int(account), skill, range);
        said = "skill " + std::to_string(skill) + " Lv" + std::to_string(level) + " on it";
    }
    msg(player, 0, kUseLevel, level, 0, 0);
    return true;
}


// ▾ / ▸ fold it to its header and open it (3.0.11-beta.79); ▦ / ▤ compact or a list (3.0.11-beta.84); a member's
// row clicked (3.0.11-beta.82) - the old picture's clicks and uaUI's events alike
void toggleFold(State& s) {
    s.folded = !s.folded;
    keepState(s);
    LOG_INFO("party: its panel %s", s.folded ? "folded" : "opened");
}

void toggleView(State& s) {
    s.compact = !s.compact;
    keepState(s);
    LOG_INFO("party: its panel %s", s.compact ? "compact" : "a list");
}

void rowClicked(State& s, int row) {
    if (row < 0 || size_t(row) >= s.rowsOf.size()) return;
    // 3.0.11-beta.82 (the owner's 2 and 4): a member clicked - the skill waited for cast on it; else elsewhere its
    // map's way followed, here a way to its cell
    const uint32_t account = s.rowsOf[size_t(row)];
    const auto it = s.seats.find(account);
    const std::string name = size_t(row) < s.members.size() ? s.members[size_t(row)].name : "?";
    if (it != s.seats.end()) {
        const Seat& seat = it->second;
        const std::string here = readGameMap();
        std::string said;
        if (castOn(account, seat, said)) {
            LOG_INFO("party: %s - %s", name.c_str(), said.c_str());
        } else if (seat.online && !seat.map.empty() && seat.map != here) {
            if (minimapproto::canFollow()) minimapproto::follow(seat.map, true);
            else referencewin::openMap(seat.map);
            LOG_INFO("party: %s is on %s - %s", name.c_str(), seat.map.c_str(),
                     minimapproto::canFollow() ? "the way there followed" : "its card opened");
        } else if (seat.x >= 0 && GetTickCount64() - seat.posAt < kPosMs && minimapproto::plotTo(seat.x, seat.y)) {
            LOG_INFO("party: a way to %s at %d, %d", name.c_str(), seat.x, seat.y);
        }
    }
}


// ---- 3.0.18-beta.19 (the owner: «Переписывать 73 части в разметку RmlUi вручную»): the panel as an RmlUi document
// while [interface] uaui is on - its rows from the members (core partypanel's words and states), the header's fold
// and view, a row's click (castOn, the way); its picture not painted then ----

const char* kDoc = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="pp" data-model="party">
<div class="glass">
 <div class="head" data-class-folded="folded"><span class="title">{{title}}</span><span class="count">{{count}}</span>
  <span class="hbtn viewb" data-event-click="view()">{{view_sign}}</span><span class="hbtn foldb"
  data-event-click="fold()">{{fold_sign}}</span><span class="hbtn closeb"
  data-event-click="close()">&#x2715;</span></div>
 <div data-if="!folded && !compact">
  <div class="row" data-for="r, i : rows" data-class-buffs="r.buffs.size > 0" data-class-low="r.low"
       data-class-target="r.target" data-class-chosen="r.chosen" data-class-dead="r.dead"
       data-class-alarm="r.alarm != ''" data-event-click="row(i)">
   <div class="face" data-class-away="r.away"><img data-if="r.face != ''" data-attr-src="r.face"/><span class="letter" data-if="r.face == ''">{{r.letter}}</span></div>
   <span class="name" data-class-bad="r.bad" data-class-away="r.faint">{{r.name}}<img class="crown" data-if="r.leader" data-attr-src="crown"/><span
     class="after" data-class-cast="r.casting" data-if="r.after != ''">{{r.after}}</span></span>
   <span class="value" data-attr-class="'value ' + r.tone"><img data-if="r.route" data-attr-src="route"/>{{r.value}}</span>
   <div class="bar" data-if="r.bar"><span class="fill" data-class-bad="r.low" data-style-width="bars[i] + '%'"></span></div>
   <div class="castline" data-if="r.cast >= 0"><span class="fill" data-style-width="r.cast + '%'"></span></div>
   <div class="where" data-if="r.arrow"><img data-attr-src="arrow" data-style-transform="'rotate(' + r.deg + 'deg)'"/>{{r.cells}}</div>
   <div class="bufrow" data-if="r.buffs.size > 0"><span class="buf" data-for="b : r.buffs" data-class-glow="b.glow != ''">
    <img data-attr-src="b.icon"/><span class="time" data-if="b.left >= 0"><span class="fill" data-class-end="b.left < 10"
     data-style-width="b.left + '%'"></span></span></span></div>
  </div>
 </div>
 <div class="cells" data-if="!folded && compact"><div class="cell" data-for="r, i : rows" data-event-click="row(i)">
   <div class="face" data-class-away="r.away"><img data-if="r.face != ''" data-attr-src="r.face"/><span class="letter" data-if="r.face == ''">{{r.letter}}</span></div>
   <span class="name" data-class-bad="r.bad" data-class-away="r.faint">{{r.name}}</span>
   <div class="bar" data-if="r.bar"><span class="fill" data-class-bad="r.low" data-style-width="bars[i] + '%'"></span></div>
 </div></div>
</div>
</body></rml>)RML";

struct BuffView {
    Rml::String icon, glow;
    float left = -1;
};
struct RowView {
    Rml::String name, letter, face, after, value, tone, cells, alarm;
    bool leader = false, low = false, bad = false, faint = false, away = false, target = false, chosen = false,
         dead = false, casting = false, route = false, bar = false, arrow = false;
    float pct = 0, cast = -1, deg = 0;
    float aim = 0;  // 3.0.18-beta.63: the real share (pct glides to it with [interface] uaui_motion)
    std::vector<BuffView> buffs;
};

struct Ui {
    std::map<Rml::String, uiglide::Glide> glides;  // 3.0.18-beta.63
    // 3.0.18-beta.79: the bars' shown shares apart from the rows - while they glide only they are evaluated again
    std::vector<float> bars;
    uaui::Doc doc;
    Rml::String title, count, viewSign, foldSign, crown, route, arrow;
    bool folded = false, compact = false;
    std::vector<RowView> rows;
    std::string key;
    std::map<const panel::Picture*, std::string> sources;  // the faces' and icons' <img> sources, by their pictures
    panel::Picture crownPic, routePic, arrowPic;
    std::string crownSrc, routeSrc, arrowSrc;
    float iconsPx = 0;
    int gen = 0;
};

Ui& ui() {
    static Ui u;
    return u;
}

// 3.0.18-beta.49: the size uaUI draws it at last frame (its document times its size in «Розміщення інтерфейсу») - the
// editor's frame round what is seen; untouched while uaUI does not draw it
void uiSize(int& w, int& h) {
    int x = 0, y = 0, dw = 0, dh = 0;
    if (!uaui::ready() || !ui().doc.box(x, y, dw, dh)) return;
    const float size = hudedit::sizeOf(hudlayout::Part::Party);
    w = int(std::lround(float(dw) * size)), h = int(std::lround(float(dh) * size));
}

// The place and size uaUI draws it at last frame, in the target's pixels; untouched while uaUI does not draw it
void uiRect(float& x, float& y, float& w, float& h) {
    int dx = 0, dy = 0, dw = 0, dh = 0;
    if (!uaui::ready() || !ui().doc.box(dx, dy, dw, dh)) return;
    const float size = hudedit::sizeOf(hudlayout::Part::Party);
    x = float(dx), y = float(dy), w = float(dw) * size, h = float(dh) * size;
}

std::string sourceOf(Ui& u, const panel::Picture* p) {
    if (!p || p->width <= 0) return {};
    return uaui::picture(p, uaui::stamp(p), u.sources[p]);
}

bool uiFrame(State& s) {
    Ui& u = ui();
    if (!uaui::ready()) {
        u.doc.show(false);
        return false;
    }
    if (!u.doc.made() && !u.doc.make("party", kDoc, [&](Rml::DataModelConstructor& m) {
            if (auto b = m.RegisterStruct<BuffView>()) {
                b.RegisterMember("icon", &BuffView::icon);
                b.RegisterMember("glow", &BuffView::glow);
                b.RegisterMember("left", &BuffView::left);
            }
            m.RegisterArray<std::vector<BuffView>>();
            if (auto r = m.RegisterStruct<RowView>()) {
                r.RegisterMember("name", &RowView::name);
                r.RegisterMember("letter", &RowView::letter);
                r.RegisterMember("face", &RowView::face);
                r.RegisterMember("after", &RowView::after);
                r.RegisterMember("value", &RowView::value);
                r.RegisterMember("tone", &RowView::tone);
                r.RegisterMember("cells", &RowView::cells);
                r.RegisterMember("alarm", &RowView::alarm);
                r.RegisterMember("leader", &RowView::leader);
                r.RegisterMember("low", &RowView::low);
                r.RegisterMember("bad", &RowView::bad);
                r.RegisterMember("faint", &RowView::faint);
                r.RegisterMember("away", &RowView::away);
                r.RegisterMember("target", &RowView::target);
                r.RegisterMember("chosen", &RowView::chosen);
                r.RegisterMember("dead", &RowView::dead);
                r.RegisterMember("casting", &RowView::casting);
                r.RegisterMember("route", &RowView::route);
                r.RegisterMember("bar", &RowView::bar);
                r.RegisterMember("arrow", &RowView::arrow);
                r.RegisterMember("pct", &RowView::pct);
                r.RegisterMember("cast", &RowView::cast);
                r.RegisterMember("deg", &RowView::deg);
                r.RegisterMember("buffs", &RowView::buffs);
            }
            m.RegisterArray<std::vector<RowView>>();
            m.Bind("title", &u.title);
            m.Bind("count", &u.count);
            m.Bind("view_sign", &u.viewSign);
            m.Bind("fold_sign", &u.foldSign);
            m.Bind("folded", &u.folded);
            m.Bind("compact", &u.compact);
            m.Bind("rows", &u.rows);
            m.RegisterArray<std::vector<float>>();
            m.Bind("bars", &u.bars);
            m.Bind("crown", &u.crown);
            m.Bind("route", &u.route);
            m.Bind("arrow", &u.arrow);
            m.BindEventCallback("fold", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                toggleFold(state());
            });
            m.BindEventCallback("view", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                toggleView(state());
            });
            // 3.0.18-beta.120 (the concept «Ігровий інтерфейс»: «✕ на каждом окне»): hidden till its icon or key
            m.BindEventCallback("close", [](Rml::DataModelHandle, Rml::Event& e, const Rml::VariantList&) {
                ownwin::setClosed(closedwin::Window::Party, true, "its ✕");
                e.StopPropagation();
            });
            m.BindEventCallback("row", [](Rml::DataModelHandle, Rml::Event& e, const Rml::VariantList& a) {
                if (!a.empty()) rowClicked(state(), a[0].Get<int>());
                e.StopPropagation();
            });
        }))
        return false;
    const float dp = uaui::dp() * hudedit::sizeOf(hudlayout::Part::Party);
    const float px = 13 * dp;
    if (px != u.iconsPx) {  // the crown, the route mark and the way's arrow (a gold triangle up, turned per member)
        u.iconsPx = px;
        ++u.gen;
        const auto icon = [&](panel::Picture& p, uiicons::Icon i, float stroke) {
            const int side = std::max(8, int(std::ceil(px)));
            p.width = p.height = side;
            p.px.assign(size_t(side) * size_t(side), 0u);
            uiicons::draw(p, i, 0, 0, float(side), 0xC9A55B, 1.0f, stroke);
        };
        icon(u.crownPic, uiicons::Icon::Mvp, 2.2f);
        icon(u.routePic, uiicons::Icon::Route, 2.2f);
        const int side = std::max(8, int(std::ceil(14 * dp)));
        panel::Picture& a = u.arrowPic;
        a.width = a.height = side;
        a.px.assign(size_t(side) * size_t(side), 0u);
        const float c = float(side) / 2, len = float(side) * 0.45f;
        for (int y = 0; y < side; ++y)
            for (int x = 0; x < side; ++x) {
                const float along = c - (float(y) + 0.5f), across = std::fabs(float(x) + 0.5f - c);
                if (along < -len * 0.8f || along > len || across > (len - along) * 0.6f) continue;
                panel::over(a.px[size_t(y) * size_t(side) + size_t(x)], 0xC9A55B, 0.95f);
            }
        u.crown = uaui::picture(&u.crownPic, u.gen, u.crownSrc);
        u.route = uaui::picture(&u.routePic, u.gen, u.routeSrc);
        u.arrow = uaui::picture(&u.arrowPic, u.gen, u.arrowSrc);
        u.doc.dirtyAll();
    }
    const settings::Lang lang = language();
    const std::string offline = settings::tr(lang, settings::Text::PartyOffline);
    const std::string dead = settings::tr(lang, settings::Text::PartyDead);
    int online = 0;
    for (const pp::Member& m : s.members) online += m.online;
    std::vector<RowView> rows;
    for (const pp::Member& m : s.members) {
        RowView r;
        const bool here = m.online && m.where.empty();
        r.low = pp::low(m, kLow);
        r.bad = r.low || m.dead;
        r.faint = !here && !r.bad;
        r.away = !here || m.dead;
        r.name = r.low ? "! " + m.name : m.name;
        const bool wide = !m.name.empty() && (static_cast<unsigned char>(m.name[0]) & 0xE0) == 0xC0;
        r.letter = m.name.empty() ? "?" : m.name.substr(0, wide ? 2 : 1);
        r.face = sourceOf(u, m.face);
        r.leader = m.leader;
        r.casting = !m.casting.empty() && m.castPart >= 0;
        r.after = r.casting ? m.casting : m.job;
        if (m.dead && m.online && !dead.empty()) r.value = dead, r.tone = "bad";
        else if (!m.online) r.value = offline, r.tone = "muted";
        else if (!m.where.empty()) r.value = m.where, r.tone = "muted", r.route = true;
        else if (m.percent >= 0) r.value = std::to_string(m.percent) + " %", r.tone = r.low ? "bad" : "";
        r.target = s.layout.targeting && here;
        r.chosen = m.chosen;
        r.dead = m.dead;
        if (m.alarm > 0 && !m.dead && !m.chosen) {  // the healer's alarm: its row pulsing red
            char b[16];
            std::snprintf(b, sizeof b, "#e5534b%02x", int(std::lround((0.10f + 0.30f * m.alarm) * 255)));
            r.alarm = b;
        }
        r.bar = here && !m.dead;
        r.pct = r.aim = float(std::clamp(m.percent, 0, 100));
        r.cast = m.castPart >= 0 && here ? std::clamp(m.castPart, 0.0f, 1.0f) * 100 : -1;
        r.arrow = m.cells >= 0 && here && (m.dirX != 0 || m.dirY != 0);
        r.cells = m.cells >= 0 ? std::to_string(m.cells) : "";
        r.deg = float(std::atan2(m.dirX, -m.dirY) * 180.0 / 3.14159265);  // up the screen 0, clockwise
        for (const pp::Buff& b : m.buffs) {
            BuffView v;
            v.icon = sourceOf(u, b.icon);
            v.left = b.left >= 0 ? std::clamp(b.left, 0.0f, 1.0f) * 100 : -1;
            if (b.glow > 0) {
                char g[48];
                std::snprintf(g, sizeof g, "#c9a55b%02x 0 0 0 2dp", int(std::lround(std::min(1.0f, b.glow) * 255)));
                v.glow = g;
            }
            r.buffs.push_back(v);
        }
        rows.push_back(std::move(r));
    }
    const std::string key = std::to_string(s.members.size()) + (s.folded ? "f" : "") + (s.compact ? "c" : "") +
                            std::to_string(online) + s.key;  // the picture's key: the members' states, the times
    if (key != u.key) {
        u.key = key;
        u.title = settings::tr(lang, settings::Text::PartyTitle);
        u.count = std::to_string(online) + "/" + std::to_string(s.members.size());
        u.viewSign = s.compact ? "\xE2\x96\xA4" : "\xE2\x96\xA6";
        u.foldSign = s.folded ? "\xE2\x96\xB8" : "\xE2\x96\xBE";
        u.folded = s.folded;
        u.compact = s.compact;
        u.rows = std::move(rows);
        for (RowView& r : u.rows) {  // 3.0.18-beta.63: from where its bar was
            const auto g = u.glides.find(r.name);
            if (g != u.glides.end() && g->second.started) r.pct = g->second.shown;
        }
        u.doc.dirtyAll();
    }
    {  // 3.0.18-beta.63: each member's HP bar glides to its share (by name; a new one at once)
        const uint64_t now = GetTickCount64();
        for (RowView& r : u.rows) {
            const float v = u.glides[r.name].step(r.aim, now, uaui::motion());
            r.pct = v;
        }
        if (u.glides.size() > 64) u.glides.clear();
        std::vector<float> bars(u.rows.size());
        for (size_t i = 0; i < u.rows.size(); ++i) bars[i] = u.rows[i].pct;
        if (bars != u.bars) u.bars = std::move(bars), u.doc.dirty("bars");
    }
    if (Rml::ElementDocument* d = u.doc.doc()) {
        const float size = hudedit::sizeOf(hudlayout::Part::Party);
        u.doc.prop("transform", size != 1.0f ? "scale(" + std::to_string(size) + ")" : "none");  // .70: on change
        if (Rml::Element* g = d->GetFirstChild()) {
            char ground[32];
            std::snprintf(ground, sizeof ground, "#%06x%02x", unsigned(uaui::themed(0x10141C)),
                          int(std::lround(uistyle::panel() * 255)));
            static Rml::Element* groundOn = nullptr;  // 3.0.18-beta.70: set when it changes (or the body is new)
            static std::string groundWas;
            if (g != groundOn || groundWas != ground) {
                groundOn = g, groundWas = ground;
                g->SetProperty("background-color", ground);
            }
        }
    }
    u.doc.place(s.layout.x, s.layout.y);
    u.doc.show(true);
    return true;
}

}  // namespace

static int g_expShare = -1;  // 3.0.18-beta.141

void packet(const packets::Event& e) {
    if (const auto* x = std::get_if<packets::PartyExp>(&e)) {  // 3.0.18-beta.141
        if (g_expShare != int(x->even))
            LOG_INFO("party: the experience %s", x->even ? "shared evenly" : "each one's own");
        g_expShare = x->even ? 1 : 0;
        return;
    }
    State& s = state();
    const uint32_t me = own();
    if (const auto* l = std::get_if<packets::PartyList>(&e)) {  // the whole list anew: the HP known stays
        std::map<uint32_t, Seat> was;
        was.swap(s.seats);
        s.order.clear();
        for (const packets::PartySeat& seat : l->seats) {
            if (seat.account == me) continue;
            take(s, seat.account, seat.name, seat.map, seat.leader, seat.online);
            const auto it = was.find(seat.account);
            if (it != was.end()) {
                s.seats[seat.account].hp = it->second.hp;
                s.seats[seat.account].most = it->second.most;
            }
        }
        if (s.told < 20) {
            ++s.told;
            LOG_INFO("party: its list - %zu others", s.order.size());
        }
    } else if (const auto* m = std::get_if<packets::PartyMember>(&e)) {
        if (m->account != me) take(s, m->account, m->name, m->map, m->leader, m->online);
    } else if (const auto* gone = std::get_if<packets::PartyLeft>(&e)) {
        if (gone->account == me) {  // the character left it: none
            s.seats.clear();
            s.order.clear();
        } else if (s.seats.erase(gone->account)) {
            s.buffs.erase(gone->account);
            s.order.erase(std::remove(s.order.begin(), s.order.end(), gone->account), s.order.end());
        }
    } else if (const auto* hp = std::get_if<packets::PartyHp>(&e)) {
        const auto it = s.seats.find(hp->account);
        if (it != s.seats.end()) {
            it->second.hp = hp->hp;
            it->second.most = hp->most;
            if (hp->hp > 0) it->second.dead = false;  // 3.0.11-beta.81: back
        }
    } else if (const auto* pos = std::get_if<packets::PartyPos>(&e)) {  // 3.0.11-beta.82: a member's cell
        if (const auto it = s.seats.find(pos->account); it != s.seats.end()) {
            it->second.x = pos->x;
            it->second.y = pos->y;
            it->second.posAt = GetTickCount64();
        }
    } else if (const auto* casting = std::get_if<packets::Casting>(&e)) {  // 3.0.11-beta.81: a member's cast
        if (const auto it = s.seats.find(casting->source); it != s.seats.end()) {
            it->second.castSkill = casting->skill;
            it->second.castAt = GetTickCount64();
            it->second.castMs = casting->ms;
        }
    } else if (const auto* icon = std::get_if<packets::StatusIcon>(&e)) {  // 3.0.11-beta.80: a member's buff
        if (s.seats.count(icon->id)) {
            Buffs& b = s.buffs[icon->id];
            const uint64_t now = GetTickCount64();
            b.timers.icon(icon->icon, icon->on, now);
            if (icon->on) b.on[icon->icon] = now;
            else b.on.erase(icon->icon);
        }
    } else if (const auto* cast = std::get_if<packets::SkillCast>(&e)) {  // its time: a cast seen on it
        const uint64_t now = GetTickCount64();
        if (s.seats.count(cast->target)) {
            Buffs& b = s.buffs[cast->target];
            b.timers.cast(cast->skill, cast->level, now);
            if (cast->source == me) b.mine[cast->skill] = now;  // 3.0.11-beta.81: the character's own
        }
        if (const auto it = s.seats.find(cast->source); it != s.seats.end() && it->second.castSkill == cast->skill)
            it->second.castSkill = 0;  // its cast went off
    } else if (const auto* a = std::get_if<packets::Appear>(&e)) {  // 3.0.11-beta.80: a member in sight - its looks
        if (const auto seat = s.seats.find(a->id); seat != s.seats.end()) seat->second.dead = a->dead;  // .81
        if (s.seats.count(a->id) && a->hair > 0) {
            readLooks(s);
            const Look l{a->sex ? 1 : 0, a->hair, a->hairColour, a->job, a->level};
            Look& was = s.looks[a->id];
            if (!(was == l)) {
                was = l;
                if (!s.looksChangedAt) s.looksChangedAt = GetTickCount64();
            }
        }
    } else if (const auto* v = std::get_if<packets::Vanish>(&e)) {
        const auto it = s.seats.find(v->id);
        if (it != s.seats.end() && v->type == 1) {  // seen dying
            it->second.hp = 0;
            it->second.dead = true;  // 3.0.11-beta.81
            it->second.castSkill = 0;
        }
    }
}

void setAlarm(bool on) {  // 3.0.11-beta.282
    State& s = state();
    if (on != s.alarmOn) LOG_INFO("party: [interface] party_alarm = %d", on ? 1 : 0);
    s.alarmOn = on;
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (on != s.on) {
        s.on = on;
        LOG_INFO("party: [interface] party = %d", on ? 1 : 0);
    }
    if (!on || s.order.empty() || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || !inGameWorld() ||
        ownwin::closed(closedwin::Window::Party)) {  // 3.0.18-beta.120: ✕
        hide(s);
        return;
    }
    readState(s);
    readLooks(s);
    s.codePage = codePage;
    if (s.looksChangedAt && GetTickCount64() - s.looksChangedAt > 2000) {  // 3.0.11-beta.80: the looks seen, kept
        s.looksChangedAt = 0;
        keepLooks(s);
    }
    const std::string here = readGameMap();
    s.members.clear();
    s.rowsOf.clear();
    const uint32_t chosen = uapad::runtime::choosing();  // 3.0.11-beta.85
    int ownX = -1, ownY = -1, mapW = 0, mapH = 0;  // 3.0.11-beta.82: the character's cell and the camera's way
    const bool ownKnown = readPlayerCell(ownX, ownY, mapW, mapH);
    float lookE = 0, lookN = 1;
    readLook(lookE, lookN);
    const float partScale = uistyle::partScale(k) * hudedit::sizeOf(hudlayout::Part::Party);
    const int faceSide = int(std::lround((s.compact ? 16 : 20) * partScale));  // 3.0.11-beta.84: a cell's round
    const int buffSide = int(std::lround(14 * partScale));
    for (const uint32_t account : s.order) {
        const auto it = s.seats.find(account);
        if (it == s.seats.end()) continue;
        const Seat& seat = it->second;
        pp::Member m;
        m.name = utf8Of(widened(seat.name, codePage));
        m.face = faceOf(s, account, faceSide, m.name);  // 3.0.11-beta.80
        const uint64_t now = GetTickCount64();
        if (const auto look = s.looks.find(account); look != s.looks.end() && look->second.job >= 0) {
            const char* job = uagfx::charblock::jobName(look->second.job);  // 3.0.11-beta.81: «Priest 85»
            const int level = look->second.level;
            if (job && *job) m.job = std::string(job) + (level > 0 ? " " + std::to_string(level) : "");
        }
        const bool nearHere = seat.online && (seat.map.empty() || seat.map == here);
        m.dead = seat.dead && nearHere;
        if (nearHere && ownKnown && seat.x >= 0 && now - seat.posAt < kPosMs) {  // 3.0.11-beta.82: its way, its cells
            const float dx = float(seat.x - ownX), dy = float(seat.y - ownY);  // east, north
            m.cells = int(std::max(std::fabs(dx), std::fabs(dy)));
            const float right = dx * lookN - dy * lookE, up = dx * lookE + dy * lookN, len = std::hypot(right, up);
            if (len > 0.5f) {
                m.dirX = right / len;
                m.dirY = -up / len;
            }
        }
        if (seat.castSkill && seat.castMs && now < seat.castAt + seat.castMs + 250 && nearHere) {
            m.casting = clienttables::skillWords(seat.castSkill, codePage);
            m.castPart = std::clamp(float(double(now - seat.castAt) / double(seat.castMs)), 0.0f, 1.0f);
        }
        if (const auto bf = s.buffs.find(account); bf != s.buffs.end()) {  // 3.0.11-beta.80: its buffs
            if (!nearHere) {  // (`near`: Windows' macro)
                s.buffs.erase(bf);  // not near: the server tells it no more
            } else {
                const std::vector<buffs::Timers::Running> running = bf->second.timers.running();
                for (auto on = bf->second.on.begin(); on != bf->second.on.end();) {
                    float left = -1;
                    uint64_t leftMs = 0;
                    bool over = false;
                    for (const buffs::Timers::Running& r : running)
                        if (r.icon == on->first && r.end > r.from) {
                            over = now >= r.end + 2000;  // its time out and no word of it: gone
                            leftMs = r.end > now ? r.end - now : 0;
                            left = std::clamp(float(double(leftMs) / double(r.end - r.from)), 0.0f, 1.0f);
                        }
                    if (over) {
                        on = bf->second.on.erase(on);
                        continue;
                    }
                    // 3.0.11-beta.81 (the owner's «напоминание перекастовать»): the character's own, its last 10 s
                    // - a gold edge pulsing
                    float glow = 0;
                    if (left >= 0 && leftMs <= 10000)
                        for (const auto& [skill, at] : bf->second.mine)
                            if (buffs::gives(skill, on->first) && now - at < 3600000) {
                                const float wave = 0.5f + 0.5f * std::sin(float(now % 100000) / 160.0f);
                                glow = std::round((0.35f + 0.65f * wave) * 8) / 8;
                            }
                    if (const panel::Picture* pic = buffcolumn::iconPicture(on->first, buffSide))
                        m.buffs.push_back(pp::Buff{pic, left, glow});
                    ++on;
                }
            }
        }
        m.leader = seat.leader;
        m.online = seat.online;
        if (!seat.map.empty() && seat.map != here) m.where = refindex::mapWords(mapNames(s), seat.map);  // .200
        if (seat.most > 0 && seat.hp >= 0) m.percent = int(int64_t(seat.hp) * 100 / seat.most);
        m.chosen = chosen != 0 && account == chosen;  // 3.0.11-beta.85: the pad's support skill would go on it
        s.members.push_back(m);
        s.rowsOf.push_back(account);
    }
    {  // 3.0.11-beta.282 (the owner's feature 18): one here falling under 30 % HP - a chime, its row pulsing red
        const uint64_t now = GetTickCount64();
        const uint32_t me = own();
        std::map<uint32_t, int> percents;
        for (size_t i = 0; i < s.members.size() && i < s.rowsOf.size(); ++i)
            if (s.rowsOf[i] != me && s.members[i].online && s.members[i].where.empty() && !s.members[i].dead)
                percents[s.rowsOf[i]] = s.members[i].percent;
        if (s.alarm.step(percents, now) && s.alarmOn) {
            static const std::vector<uint8_t> sound = chime::wav(chime::Kind::PartyLow);
            const bool played = win::playOwn(sound.data());
            if (s.alarmTold++ < 20)
                LOG_INFO("party: one fell under 30 %% HP - the alarm %s", played ? "played" : "not played (sound off)");
        }
        for (size_t i = 0; i < s.members.size() && i < s.rowsOf.size(); ++i)
            s.members[i].alarm = s.alarmOn ? s.alarm.pulse(s.rowsOf[i], now) : 0;
    }
    const float scale = uistyle::partScale(k) * hudedit::sizeOf(hudlayout::Part::Party);  // 3.0.11-beta.75: its size
    s.k = float(k);
    s.scale = scale;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.layout = pp::layout(pp::rowsOf(s.members), int(std::lround(10 * scale)), topOf(k, scale), scale, s.folded,
                          s.compact);
    uint32_t mode = 0;
    s.layout.targeting = waitingState(mode) != 0 || chosen != 0;  // 3.0.11-beta.82: a click on a member casts it there
    {  // 3.0.18-beta.49 (R-786CGH: the editor's frame narrower than the panel): uaUI's document's own size
        int w = s.layout.w, h = s.layout.h;
        uiSize(w, h);
        hudedit::place(hudlayout::Part::Party, float(k), targetWidth, targetHeight, w, h, s.layout.x, s.layout.y);
    }
    std::string key = std::to_string(s.layout.w) + 'x' + std::to_string(s.layout.h) + ';' + std::to_string(scale) +
                      (s.folded ? ";f;" : ";;");
    for (const pp::Member& m : s.members)
        key += m.name + '|' + m.where + '|' + (m.leader ? "L" : "") + (m.online ? "" : "o") +
               std::to_string(m.percent) + (m.face ? std::to_string(uintptr_t(m.face)) : "") + (m.chosen ? "c" : "") +
               std::to_string(int(m.alarm * 8)) + ';';
    for (const pp::Member& m : s.members) {  // 3.0.11-beta.80: its buffs, their times by twentieths (.81: glow)
        for (const pp::Buff& b : m.buffs)
            key += std::to_string(uintptr_t(b.icon)) + ':' + std::to_string(int(b.left * 20)) + ':' +
                   std::to_string(int(b.glow * 8)) + ',';
        key += m.job + '|' + m.casting + '|' + std::to_string(int(m.castPart * 20)) + (m.dead ? "d;" : ";") +
               std::to_string(m.cells) + ',' + std::to_string(int(m.dirX * 16)) + ',' +
               std::to_string(int(m.dirY * 16)) + ';';
    }
    key += s.layout.targeting ? "T" : "";
    key += s.compact ? "C" : "";
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()); drawn != s.key) {  // its grounds
        s.key = drawn;
        s.uiShown = uiFrame(s);  // 3.0.18-beta.19: by uaUI while [interface] uaui is on
    } else {
        s.uiShown = uiFrame(s);
    }
    s.shown = true;
    block(s, game);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown) ui().doc.show(false);  // 3.0.18-beta.19
    (void)record;  // the old edition (its picture) gone: uaUI draws it
    (void)renderer;
}

bool counts(int& online, int& total) {
    const State& s = state();
    if (s.seats.empty()) return false;  // the party's list, the character's own seat not among them
    online = 1;
    total = 1 + int(s.seats.size());
    for (const auto& [account, seat] : s.seats) online += seat.online ? 1 : 0;
    return true;
}

bool panelBox(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.shown) return false;
    x = s.layout.x + int(s.layout.glass.x);
    y = s.layout.y + int(s.layout.glass.y);
    w = int(s.layout.glass.w);
    h = int(s.layout.glass.h);
    return true;
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown || s.uiShown) return false;  // 3.0.18-beta.19: uaUI's events take its clicks
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    (void)wp;
    POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    if (m == WM_MOUSEWHEEL && !ScreenToClient(game, &p)) return false;  // the wheel's point is the screen's
    RECT client{};
    if (!GetClientRect(game, &client) || client.right <= 0 || client.bottom <= 0) return false;
    const float tx = float(p.x) * float(s.targetW) / float(client.right) - float(s.layout.x);
    const float ty = float(p.y) * float(s.targetH) / float(client.bottom) - float(s.layout.y);
    if (!s.layout.glass.contains(tx, ty)) return false;
    result = 0;  // the click is not the world's
    const pp::Hit hit = pp::hitTest(s.layout, tx, ty);
    if ((m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) && hit.part == pp::Part::Fold) {
        toggleFold(s);
    } else if ((m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) && hit.part == pp::Part::View) {
        toggleView(s);
    } else if (m == WM_LBUTTONDOWN && hit.part == pp::Part::Row) {
        rowClicked(s, hit.row);
    }
    return true;
}

std::vector<uint32_t> hereIds() {
    State& s = state();
    std::vector<uint32_t> out;
    if (!s.on || s.order.empty()) return out;
    const std::string here = readGameMap();
    for (const uint32_t account : s.order) {
        const auto it = s.seats.find(account);
        if (it != s.seats.end() && it->second.online && !it->second.dead &&
            (it->second.map.empty() || it->second.map == here))
            out.push_back(account);
    }
    return out;
}

bool inParty(uint32_t account) {
    State& s = state();
    return s.on && account && s.seats.count(account) != 0;
}

std::vector<OnMap> onMap(int faceSide) {
    State& s = state();
    std::vector<OnMap> out;
    if (!s.on || s.order.empty()) return out;
    const std::string here = readGameMap();
    const ULONGLONG now = GetTickCount64();
    for (const uint32_t account : s.order) {
        const auto it = s.seats.find(account);
        if (it == s.seats.end()) continue;
        const Seat& seat = it->second;
        if (!seat.online || (!seat.map.empty() && seat.map != here) || seat.x < 0 || now - seat.posAt > kPosMs)
            continue;
        OnMap m;
        m.x = seat.x;
        m.y = seat.y;
        m.name = utf8Of(widened(seat.name, s.codePage));
        m.face = faceOf(s, account, faceSide, m.name);
        m.dead = seat.dead;
        out.push_back(m);
    }
    return out;
}

std::vector<Person> people() {
    const State& s = state();
    std::vector<Person> out;
    for (const pp::Member& m : s.members) {
        Person p;
        p.name = m.name, p.job = m.job, p.where = m.where;
        p.percent = m.percent, p.cells = m.cells, p.dead = m.dead, p.online = m.online, p.face = m.face;
        out.push_back(std::move(p));
    }
    return out;
}

bool routeTo(const std::string& name) {
    State& s = state();
    for (size_t i = 0; i < s.members.size(); ++i)
        if (s.members[i].name == name) {
            rowClicked(s, int(i));
            return true;
        }
    return false;
}

int expShare() { return g_expShare; }

}  // namespace uagfx::win::partywin
