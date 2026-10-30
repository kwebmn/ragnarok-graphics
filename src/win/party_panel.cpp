#include "win/party_panel.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "core/party_panel.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/char_block.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/hud_edit.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"

namespace uagfx::win::partywin {
namespace {

namespace pp = uagfx::partypanel;

constexpr uint32_t kOwnAccount = 0x771F7C;
// The game's Basic Info window (UI window 0) when the character block is off: the panel under it
constexpr uint32_t kBasicInfo = 0x727ED8 + 0x90, kShown = 0x38, kRect = 0x14;
constexpr int kLow = 30;  // per cent: red under it (as [notify] party_hp's default)

struct Seat {
    std::string name, map;  // the game's code page, as they came
    bool leader = false, online = true;
    int hp = -1, most = 0;
};

struct State {
    std::vector<uint32_t> order;
    std::map<uint32_t, Seat> seats;
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

void take(State& s, uint32_t account, const std::string& name, const std::string& map, bool leader, bool online) {
    if (!s.seats.count(account)) s.order.push_back(account);
    Seat& seat = s.seats[account];
    seat.name = name;
    if (!map.empty()) seat.map = map;
    seat.leader = leader;
    seat.online = online;
}

std::vector<Words> gdiWords(const std::vector<pp::Words>& in) {
    std::vector<Words> out;
    for (const pp::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == pp::Align::Right    ? DT_RIGHT
                                 : w.align == pp::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("party", s.layout.w, s.layout.h);  // 3.0.11-beta.55
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;  // 3.0.11-beta.57: in the memory it had
    pp::paint(s.picture, s.layout, s.members, kLow);
    drawWords(s.picture, gdiWords(pp::words(s.layout, s.members, kLow,
                                            settings::tr(startup().lang, settings::Text::PartyOffline))));
    s.upload = true;
}

void block(const State& s, HWND game) {  // the mouse's clicks over it are its own
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
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

}  // namespace

void packet(const packets::Event& e) {
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
            s.order.erase(std::remove(s.order.begin(), s.order.end(), gone->account), s.order.end());
        }
    } else if (const auto* hp = std::get_if<packets::PartyHp>(&e)) {
        const auto it = s.seats.find(hp->account);
        if (it != s.seats.end()) {
            it->second.hp = hp->hp;
            it->second.most = hp->most;
        }
    } else if (const auto* v = std::get_if<packets::Vanish>(&e)) {
        const auto it = s.seats.find(v->id);
        if (it != s.seats.end() && v->type == 1) it->second.hp = 0;  // seen dying
    }
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (on != s.on) {
        s.on = on;
        LOG_INFO("party: [interface] party = %d", on ? 1 : 0);
    }
    if (!on || s.order.empty() || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || !inGameWorld()) {
        hide(s);
        return;
    }
    const std::string here = readGameMap();
    s.members.clear();
    for (const uint32_t account : s.order) {
        const auto it = s.seats.find(account);
        if (it == s.seats.end()) continue;
        const Seat& seat = it->second;
        pp::Member m;
        m.name = utf8Of(widened(seat.name, codePage));
        m.leader = seat.leader;
        m.online = seat.online;
        if (!seat.map.empty() && seat.map != here) m.where = seat.map;
        if (seat.most > 0 && seat.hp >= 0) m.percent = int(int64_t(seat.hp) * 100 / seat.most);
        s.members.push_back(m);
    }
    const float scale = uistyle::partScale(k) * hudedit::sizeOf(hudlayout::Part::Party);  // 3.0.11-beta.75: its size
    s.k = float(k);
    s.scale = scale;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.layout = pp::layout(s.members.size(), int(std::lround(10 * scale)), topOf(k, scale), scale);
    hudedit::place(hudlayout::Part::Party, float(k), targetWidth, targetHeight, s.layout.w, s.layout.h, s.layout.x,
                   s.layout.y);  // 3.0.11-beta.75: where the player put it
    std::string key = std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ';' + std::to_string(scale) + ';';
    for (const pp::Member& m : s.members)
        key += m.name + '|' + m.where + '|' + (m.leader ? "L" : "") + (m.online ? "" : "o") +
               std::to_string(m.percent) + ';';
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()); drawn != s.key) {  // its grounds
        s.key = drawn;
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
    POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    if (m == WM_MOUSEWHEEL && !ScreenToClient(game, &p)) return false;  // the wheel's point is the screen's
    RECT client{};
    if (!GetClientRect(game, &client) || client.right <= 0 || client.bottom <= 0) return false;
    const float tx = float(p.x) * float(s.targetW) / float(client.right) - float(s.layout.x);
    const float ty = float(p.y) * float(s.targetH) / float(client.bottom) - float(s.layout.y);
    if (!s.layout.glass.contains(tx, ty)) return false;
    result = 0;  // nothing of its own to do yet: the click is not the world's
    return true;
}

}  // namespace uagfx::win::partywin
