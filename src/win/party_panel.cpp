#include "win/party_panel.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "core/buff_timers.hpp"
#include "core/char_block.hpp"
#include "core/layout.hpp"
#include "core/party_panel.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/buff_column.hpp"
#include "win/char_block.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/head_picture.hpp"
#include "win/hud_edit.hpp"
#include "win/log.hpp"
#include "win/minimap_proto.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_window.hpp"
#include "win/ui_cost.hpp"

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
    const auto it = s.looks.find(account);
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
    const settings::Lang lang = startup().lang;
    drawWords(s.picture, gdiWords(pp::words(s.layout, s.members, kLow, settings::tr(lang, settings::Text::PartyOffline),
                                            settings::tr(lang, settings::Text::PartyTitle),
                                            settings::tr(lang, settings::Text::PartyDead))));
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
        if (!seat.map.empty() && seat.map != here) m.where = seat.map;
        if (seat.most > 0 && seat.hp >= 0) m.percent = int(int64_t(seat.hp) * 100 / seat.most);
        s.members.push_back(m);
        s.rowsOf.push_back(account);
    }
    const float scale = uistyle::partScale(k) * hudedit::sizeOf(hudlayout::Part::Party);  // 3.0.11-beta.75: its size
    s.k = float(k);
    s.scale = scale;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.layout = pp::layout(pp::rowsOf(s.members), int(std::lround(10 * scale)), topOf(k, scale), scale, s.folded,
                          s.compact);
    uint32_t mode = 0;
    s.layout.targeting = waitingState(mode) != 0;  // 3.0.11-beta.82: a click on a member casts it there
    hudedit::place(hudlayout::Part::Party, float(k), targetWidth, targetHeight, s.layout.w, s.layout.h, s.layout.x,
                   s.layout.y);  // 3.0.11-beta.75: where the player put it
    std::string key = std::to_string(s.layout.w) + 'x' + std::to_string(s.layout.h) + ';' + std::to_string(scale) +
                      (s.folded ? ";f;" : ";;");
    for (const pp::Member& m : s.members)
        key += m.name + '|' + m.where + '|' + (m.leader ? "L" : "") + (m.online ? "" : "o") +
               std::to_string(m.percent) + (m.face ? std::to_string(uintptr_t(m.face)) : "") + ';';
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
    result = 0;  // the click is not the world's
    const pp::Hit hit = pp::hitTest(s.layout, tx, ty);
    if ((m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) && hit.part == pp::Part::Fold) {
        s.folded = !s.folded;  // 3.0.11-beta.79: ▾ folds it to its header, ▸ opens it
        keepState(s);
        LOG_INFO("party: its panel %s", s.folded ? "folded" : "opened");
    } else if ((m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) && hit.part == pp::Part::View) {
        s.compact = !s.compact;  // 3.0.11-beta.84: ▦ two columns of small cells, ▤ the list
        keepState(s);
        LOG_INFO("party: its panel %s", s.compact ? "compact" : "a list");
    } else if (m == WM_LBUTTONDOWN && hit.part == pp::Part::Row && size_t(hit.row) < s.rowsOf.size()) {
        // 3.0.11-beta.82 (the owner's 2 and 4): a member clicked - the skill waited for cast on it; else elsewhere its
        // map's way followed, here a way to its cell
        const uint32_t account = s.rowsOf[size_t(hit.row)];
        const auto it = s.seats.find(account);
        const std::string name = size_t(hit.row) < s.members.size() ? s.members[size_t(hit.row)].name : "?";
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
    return true;
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

}  // namespace uagfx::win::partywin
