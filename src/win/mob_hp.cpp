#include "win/mob_hp.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "core/game_camera.hpp"
#include "core/mob_hp.hpp"
#include "core/own_ground.hpp"
#include "core/reference_index.hpp"
#include "core/ui_style.hpp"
#include "pad/win/runtime.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/reference_load.hpp"
#include "win/target_card.hpp"
#include "win/world_mouse.hpp"

namespace uagfx::win::mobhpwin {
namespace {

struct Bar {
    float x = 0, y = 0, w = 0, h = 0;  // a part's pixels
    float top = 0;                     // 3.0.11-beta.148: its top in the game's pixels (the target's chevron)
    int hp = 0;
    uint32_t id = 0;  // 3.0.11-beta.99: whose (the ring's chevron stands over any)
    bool target = false;
    mobhp::Rank rank = mobhp::Rank::Normal;  // 3.0.13-beta.3
    bool hitting = false;                    // 3.0.13-beta.13: it hit us of late - its edge red
};

struct State {
    MobHp mode = MobHp::All;
    bool modeTold = false;
    bool percent = false;  // 3.0.11-beta.293: [interface] mob_hp_percent
    bool bossBars = false;  // 3.0.13-beta.3: [interface] boss_bars
    // 3.0.13-beta.13: [interface] who_hits - the monsters that hit us, «⚔ N» over the character (the screen's middle)
    bool whoHits = false;
    mobhp::Hitters hitters;
    panel::Picture countPic;
    uint32_t countTex = 0;
    int countW = 0, countH = 0, countX = 0, countY = 0, countShown = 0, hitsTold = 0;
    bool countUpload = false;
    std::unordered_map<int, mobhp::Rank> ranks;  // a class's, as the reference has it
    int rankTold = 0;
    mobhp::Table table;
    std::unordered_map<uint32_t, std::array<float, 4>> boxes;
    // 3.0.11-beta.148 (the owner: «хп над мобом дергается когда тот ходит», «когда моба атакую, хп дергается туда
    // сюда»): each actor's place in the world, each monster's tallest frame of late - the bar over its feet
    std::unordered_map<uint32_t, std::array<float, 3>> places;
    std::unordered_map<uint32_t, mobhp::Tall> talls;
    int steadyTold = 0;
    std::vector<Bar> bars;
    float k = 0;
    uint32_t target = 0;
    // the target's per cent beside its bar
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false, text = false;
    std::string key;
    int textX = 0, textY = 0;
    int heard = 0, told = 0;
    uint32_t pointedLast = 0;  // 3.0.11-beta.99: the target's last, and where it came from (logged)
    int pointedTold = 0;
    // 3.0.11-beta.100: «Усі» - the monsters on the screen whose per cent is asked of the server (the game's own name
    // request FUN_00560650: thiscall, the game mode in ECX, the actor's id; the packet 0x008c, its 2.5 s per actor)
    mobhp::Asker asker;
    std::unordered_map<uint32_t, int> classes;
    std::vector<uint32_t> toAsk;
    std::string map;
    int asked = 0, askTold = 0;
};

State& state() {
    static State s;
    return s;
}

// 3.0.11-beta.99 (the owner: «hp для мобов должно быть доступно и в обычном режиме»): the controller's target while
// played with it; with the mouse the monster under its cursor; else the one the character fights - the card on or not
constexpr uint32_t kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0, kAskName = 0x560650;
constexpr uint32_t kOwnAccount = 0x771F7C;  // 3.0.13-beta.13: the character's own id (as death_recap_watch reads it)
const uint8_t kAskNameCode[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x24, 0x8B, 0x45,
                                0x08, 0x53, 0x56, 0x57, 0x85, 0xC0, 0x8B, 0xF9};
using AskNameFn = void(__attribute__((thiscall)) *)(uint32_t mode, uint32_t id);
constexpr int kFirstMonster = 1001, kLastMonster = 3999;  // the classes the game names as monsters

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

uint32_t targetNow(State& s) {
    const bool pad = uapad::runtime::look().shown;
    const uint32_t padTarget = pad ? uapad::runtime::target() : 0, under = pad ? 0 : pointedActor();
    const uint32_t fighting = targetpanel::fighting();
    const uint32_t target = mobhp::pointed(s.table, pad, padTarget, under, fighting);
    if (target && target != s.pointedLast && s.pointedTold < 20) {
        ++s.pointedTold;
        LOG_INFO("mob hp: the target's bar on %u - %s", target,
                 target == padTarget  ? "the controller's target"
                 : target == under    ? "the monster under the mouse cursor"
                                      : "the one the character fights");
    }
    s.pointedLast = target;
    return target;
}

}  // namespace

void packet(const packets::Event& e) {
    State& s = state();
    if (const auto* a = std::get_if<packets::ActorInfo>(&e)) {
        if (a->hp < 0) return;
        s.table.seen(a->id, a->hp, GetTickCount());
        if (s.heard < 20) {
            ++s.heard;
            LOG_INFO("mob hp: %u at %d%% (its name line)", a->id, a->hp);
        }
    } else if (const auto* h = std::get_if<packets::Hit>(&e)) {  // 3.0.13-beta.13: who hits us
        uint32_t own = 0;
        if (s.whoHits && h->source != h->target && readGame(kOwnAccount, &own, 4) && own && h->target == own &&
            h->source != own)
            s.hitters.hit(h->source, GetTickCount());
    } else if (const auto* v = std::get_if<packets::Vanish>(&e)) {
        s.hitters.gone(v->id);  // 3.0.13-beta.13
        s.table.gone(v->id);
        s.asker.forget(v->id);  // 3.0.11-beta.100: asked again as soon as it is back
        s.talls.erase(v->id);   // 3.0.11-beta.148
    }
}

void setPercent(bool on) { state().percent = on; }
void setBossBars(bool on) { state().bossBars = on; }
void setWhoHits(bool on) {
    State& s = state();
    if (!on) s.hitters.last.clear();
    s.whoHits = on;
}

namespace {
// 3.0.13-beta.3: a monster class's rank by the reference - a boss (mode 0x20), an MVP by its MVP reward; none loaded
// yet: plain, asked again later
mobhp::Rank rankOf(State& s, int cls) {
    if (cls < kFirstMonster || cls > kLastMonster) return mobhp::Rank::Normal;
    if (const auto it = s.ranks.find(cls); it != s.ranks.end()) return it->second;
    const std::shared_ptr<const refindex::Index> index = reference::index();
    if (!index) return mobhp::Rank::Normal;
    const auto mob = index->mobs.find(cls);
    bool mvp = false;
    if (mob != index->mobs.end())
        for (const refindex::Drop& d : mob->second.drops) mvp = mvp || d.kind == 1;
    const mobhp::Rank r = mobhp::rankOf(mob != index->mobs.end() && mob->second.boss, mvp);
    s.ranks[cls] = r;
    if (r != mobhp::Rank::Normal && s.rankTold < 10) {
        ++s.rankTold;
        LOG_INFO("mob hp: %s - %d %s, its bar wider", r == mobhp::Rank::Mvp ? "an MVP" : "a boss", cls,
                 mob->second.name.c_str());
    }
    return r;
}
}  // namespace

void frame(HWND game, MobHp mode, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (mode != s.mode || !s.modeTold) {
        s.mode = mode;
        s.modeTold = true;
        LOG_INFO("mob hp: [interface] mob_hp = %s", mobHpToken(mode));
    }
    s.bars.clear();
    s.text = false;
    s.target = 0;
    s.countShown = 0;
    if (mode == MobHp::Off || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || !inGameWorld()) return;
    const std::string map = readGameMap();
    if (map != s.map) {  // a new map: its monsters are others
        s.hitters.last.clear();  // 3.0.13-beta.13
        s.map = map;
        s.table.clear();
        s.asker.clear();
        s.toAsk.clear();
        s.talls.clear();
    }
    if (s.whoHits) {  // 3.0.13-beta.13: «⚔ N» over the character - the camera keeps it in the screen's middle
        const int n = s.hitters.count(GetTickCount());
        if (n > 0) {
            const float scale = uistyle::partScale(k);
            const std::string text = "\xE2\x9A\x94 " + std::to_string(n);
            const std::wstring wide = utf8Wide(text);
            const float px = 13 * scale;
            const int w = wordsWidth(wide, px, true) + int(std::lround(12 * scale)), h = int(std::lround(18 * scale));
            if (panel::blank(s.countPic, w, h)) {
                panel::roundRect(s.countPic, panel::Box{0, 0, float(w), float(h)}, float(h) / 2, 0xC62828, 0.85f);
                drawWords(s.countPic,
                          {Words{panel::Box{0, 0, float(w), float(h)}, wide, 0xFFFFFF, px, true, DT_CENTER}});
                s.countUpload = true;
            }
            s.countX = targetWidth / 2 - w / 2;
            s.countY = targetHeight / 2 - int(std::lround(130 * scale)) - h;
            s.countShown = n;
            if (s.hitsTold < 5 && n >= 2) {
                ++s.hitsTold;
                LOG_INFO("mob hp: %d monsters hit the character of late - «⚔ %d» over it, their bars framed red", n, n);
            }
        }
    }
    const uint32_t target = targetNow(s);
    const std::vector<mobhp::Shown> list = s.table.shown(mode, target, GetTickCount());
    if (list.empty() && mode != MobHp::All) return;
    readActorBoxes(s.boxes, mode == MobHp::All || s.bossBars ? &s.classes : nullptr);  // 3.0.13-beta.3: ranks
    const float kk = float(k);
    const int gameHeight = int(std::lround(float(targetHeight) / kk));
    if (mode == MobHp::All) {  // 3.0.11-beta.100: the monsters on the screen not known yet, the nearest first
        const int gameWidth = int(std::lround(float(targetWidth) / kk));
        std::vector<std::pair<float, uint32_t>> nearest;
        for (const auto& [id, cls] : s.classes) {
            if (cls < kFirstMonster || cls > kLastMonster || s.table.hp(id) >= 0) continue;
            const auto& b = s.boxes[id];
            const float dx = (b[0] + b[2]) / 2 - float(gameWidth) / 2, dy = (b[1] + b[3]) / 2 - float(gameHeight) / 2;
            nearest.emplace_back(dx * dx + dy * dy, id);
        }
        std::sort(nearest.begin(), nearest.end());
        std::vector<uint32_t> ids;
        for (const auto& [d, id] : nearest) ids.push_back(id);
        for (const uint32_t id : s.asker.due(ids, s.table, GetTickCount()))
            if (s.toAsk.size() < 32) s.toAsk.push_back(id);
    }
    if (list.empty()) return;
    s.k = kk;
    // 3.0.11-beta.148: the bars over the monsters' feet as the game's camera puts them on the screen, at their tallest
    // frame's height of late; a monster whose place is not read - over its box, as before
    float view[12];
    const bool camera = readGameCamera(view);
    const camera::Projection p = camera::projectionFor(int(std::lround(float(targetWidth) / kk)), gameHeight);
    if (camera) readActorPlaces(s.places);
    const uint32_t nowMs = GetTickCount();
    int steady = 0;
    for (const mobhp::Shown& m : list) {
        const auto it = s.boxes.find(m.id);
        if (it == s.boxes.end()) continue;  // not on the screen now
        const auto place = camera ? s.places.find(m.id) : s.places.end();
        own_ground::Tl2 feet{};
        mobhp::Bar b;
        if (place != s.places.end()) {
            const float* w = place->second.data();
            const float c[3] = {w[0] * view[0] + w[1] * view[3] + w[2] * view[6] + view[9],
                                w[0] * view[1] + w[1] * view[4] + w[2] * view[7] + view[10],
                                w[0] * view[2] + w[1] * view[5] + w[2] * view[8] + view[11]};
            if (!own_ground::placeView(p, c, feet)) feet.rhw = 0;
        }
        if (place != s.places.end() && feet.rhw > 0 && std::isfinite(feet.x) && std::isfinite(feet.y)) {
            mobhp::Tall& tall = s.talls[m.id];
            mobhp::keepTall(tall, feet.y - it->second[1], nowMs);
            b = mobhp::barOverFeet(feet.x, feet.y, tall.px, m.target, gameHeight);
            ++steady;
        } else {
            b = mobhp::barOver(it->second.data(), m.target, gameHeight);
        }
        mobhp::Rank rank = mobhp::Rank::Normal;  // 3.0.13-beta.3
        if (s.bossBars)
            if (const auto c = s.classes.find(m.id); c != s.classes.end()) rank = rankOf(s, c->second);
        b = mobhp::ranked(b, rank);
        s.bars.push_back(Bar{b.x * kk, b.y * kk, b.w * kk, b.h * kk, b.y, m.hp, m.id, m.target, rank,
                             s.whoHits && s.hitters.hitting(m.id, nowMs)});
        if (!m.target) continue;
        s.target = m.id;
        if (!s.percent) continue;  // 3.0.11-beta.293: the plate off
        const float scale = uistyle::partScale(k);
        const std::string text = mobhp::percentText(m.hp);
        const std::string key = text + ';' + std::to_string(scale);
        if (key != s.key) {
            s.key = key;
            const std::wstring wide = utf8Wide(text);
            const float px = 12 * scale;
            const int w = wordsWidth(wide, px, true) + int(std::lround(8 * scale));
            const int h = int(std::lround(16 * scale));
            if (panel::blank(s.picture, w, h)) {
                panel::roundRect(s.picture, panel::Box{0, 0, float(w), float(h)}, float(h) / 2, 0x000000, 0.5f);
                drawWords(s.picture, {Words{panel::Box{0, 0, float(w), float(h)}, wide, 0xECEFF5, px, true,
                                            DT_CENTER}});
                s.upload = true;
            }
        }
        s.textX = int(std::lround((b.x + b.w) * kk + 4 * scale));
        s.textY = int(std::lround((b.y + b.h / 2) * kk - float(s.picture.height) / 2));
        s.text = s.picture.width > 0;
    }
    if (!s.bars.empty() && s.told < 10) {
        ++s.told;
        LOG_INFO("mob hp: %zu bar(s) over the monsters%s (%zu per cents known); %d of them over their feet, steady",
                 s.bars.size(), s.target ? ", the target's with its per cent" : "", s.table.size(), steady);
    }
    if (!s.bars.empty() && size_t(steady) < s.bars.size() && s.steadyTold < 5) {
        ++s.steadyTold;
        LOG_INFO("mob hp: %zu of %zu bars over their sprites' boxes - %s", s.bars.size() - size_t(steady),
                 s.bars.size(), camera ? "their places in the world not read" : "the game's camera not read");
    }
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (s.bars.empty() || s.k <= 0) return;
    // 3.0.11-beta.99: where the world ends - under the game's own windows, its names and its cursor, as the world is
    const size_t world = record.firstInterface(), before = record.commands().size();
    const float edge = std::max(1.0f, s.k * 0.75f);
    for (const Bar& b : s.bars) {
        if (b.hitting) {  // 3.0.13-beta.13: it hits us - a red frame round its bar
            const float red = 2.0f * edge;
            drawFill(record, b.x - red - edge, b.y - red - edge, b.w + 2 * (red + edge), b.h + 2 * (red + edge), s.k,
                     0xF0FF4D4Du);
        }
        drawFill(record, b.x - edge, b.y - edge, b.w + 2 * edge, b.h + 2 * edge, s.k, 0xB4000000u);
        const float done = b.w * float(std::clamp(b.hp, 0, 100)) / 100.0f;
        drawFill(record, b.x, b.y, done, b.h, s.k, mobhp::fillColour(b.hp, b.rank));
    }
    if (s.text)
        drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.textX, s.textY, s.k);
    if (s.countShown > 0 && s.countPic.width > 0)  // 3.0.13-beta.13: «⚔ N» over the character
        drawPart(record, renderer, s.countPic, s.countTex, s.countW, s.countH, s.countUpload, s.countX, s.countY, s.k);
    const size_t added = record.commands().size() - before;  // the per cent's upload too, when it went now
    if (added) record.moveLastBefore(added, world);
}

void beforeGameDraw() {
    State& s = state();
    if (s.toAsk.empty()) return;
    std::vector<uint32_t> ids;
    ids.swap(s.toAsk);
    static int known = -1;  // the game's name request where this build has it: checked once
    if (known < 0) {
        uint8_t have[sizeof kAskNameCode] = {};
        known = readGame(kAskName, have, sizeof have) && std::memcmp(have, kAskNameCode, sizeof have) == 0 ? 1 : 0;
        if (!known) LOG_INFO("mob hp: the game's name request is not where this build has it - no monster asked");
    }
    const uint32_t mode = read32(kModeNow);
    if (!known || !mode || !inGameWorld() || read32(read32(mode) + kModeMsgSlot) != kModeMsg) return;
    for (const uint32_t id : ids) reinterpret_cast<AskNameFn>(uintptr_t(kAskName))(mode, id);
    s.asked += int(ids.size());
    if (s.askTold < 10) {
        ++s.askTold;
        LOG_INFO("mob hp: «Усі» - %zu monster(s) on the screen asked of the server for their per cent (the game's own "
                 "name request, the packet 0x008c; %d so far)", ids.size(), s.asked);
    }
}

int hp(uint32_t id) {
    const State& s = state();
    return s.mode == MobHp::Off ? -1 : s.table.hp(id);
}

bool targetBarTop(uint32_t id, const float box[4], int screenHeight, float& top) {
    const State& s = state();
    if (!id) return false;
    (void)box;
    (void)screenHeight;
    for (const Bar& b : s.bars) {
        if (b.id != id) continue;
        top = b.top;  // 3.0.11-beta.148: the bar's own place (over the feet, steady)
        return true;
    }
    return false;
}

}  // namespace uagfx::win::mobhpwin
