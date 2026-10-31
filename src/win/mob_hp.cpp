#include "win/mob_hp.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "core/mob_hp.hpp"
#include "core/ui_style.hpp"
#include "pad/win/runtime.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/target_card.hpp"
#include "win/world_mouse.hpp"

namespace uagfx::win::mobhpwin {
namespace {

struct Bar {
    float x = 0, y = 0, w = 0, h = 0;  // a part's pixels
    int hp = 0;
    uint32_t id = 0;  // 3.0.11-beta.99: whose (the ring's chevron stands over any)
    bool target = false;
};

struct State {
    MobHp mode = MobHp::All;
    bool modeTold = false;
    mobhp::Table table;
    std::unordered_map<uint32_t, std::array<float, 4>> boxes;
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
    } else if (const auto* v = std::get_if<packets::Vanish>(&e)) {
        s.table.gone(v->id);
        s.asker.forget(v->id);  // 3.0.11-beta.100: asked again as soon as it is back
    }
}

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
    if (mode == MobHp::Off || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || !inGameWorld()) return;
    const std::string map = readGameMap();
    if (map != s.map) {  // a new map: its monsters are others
        s.map = map;
        s.table.clear();
        s.asker.clear();
        s.toAsk.clear();
    }
    const uint32_t target = targetNow(s);
    const std::vector<mobhp::Shown> list = s.table.shown(mode, target, GetTickCount());
    if (list.empty() && mode != MobHp::All) return;
    readActorBoxes(s.boxes, mode == MobHp::All ? &s.classes : nullptr);
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
    for (const mobhp::Shown& m : list) {
        const auto it = s.boxes.find(m.id);
        if (it == s.boxes.end()) continue;  // not on the screen now
        const mobhp::Bar b = mobhp::barOver(it->second.data(), m.target, gameHeight);
        s.bars.push_back(Bar{b.x * kk, b.y * kk, b.w * kk, b.h * kk, m.hp, m.id, m.target});
        if (!m.target) continue;
        s.target = m.id;
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
        LOG_INFO("mob hp: %zu bar(s) over the monsters%s (%zu per cents known)", s.bars.size(),
                 s.target ? ", the target's with its per cent" : "", s.table.size());
    }
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (s.bars.empty() || s.k <= 0) return;
    // 3.0.11-beta.99: where the world ends - under the game's own windows, its names and its cursor, as the world is
    const size_t world = record.firstInterface(), before = record.commands().size();
    const float edge = std::max(1.0f, s.k * 0.75f);
    for (const Bar& b : s.bars) {
        drawFill(record, b.x - edge, b.y - edge, b.w + 2 * edge, b.h + 2 * edge, s.k, 0xB4000000u);
        const float done = b.w * float(std::clamp(b.hp, 0, 100)) / 100.0f;
        drawFill(record, b.x, b.y, done, b.h, s.k, mobhp::fillColour(b.hp));
    }
    if (s.text)
        drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.textX, s.textY, s.k);
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
    for (const Bar& b : s.bars) {
        if (b.id != id) continue;
        top = mobhp::barOver(box, b.target, screenHeight).y;
        return true;
    }
    return false;
}

}  // namespace uagfx::win::mobhpwin
