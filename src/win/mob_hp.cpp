#include "win/mob_hp.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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

namespace uagfx::win::mobhpwin {
namespace {

struct Bar {
    float x = 0, y = 0, w = 0, h = 0;  // a part's pixels
    int hp = 0;
};

struct State {
    MobHp mode = MobHp::Wounded;
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
};

State& state() {
    static State s;
    return s;
}

uint32_t targetNow() {  // the controller's target while played with it, else the one the character fights
    const uint32_t pad = uapad::runtime::look().shown ? uapad::runtime::target() : 0;
    return pad ? pad : targetpanel::current();
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
    const uint32_t target = targetNow();
    const std::vector<mobhp::Shown> list = s.table.shown(mode, target, GetTickCount());
    if (list.empty()) return;
    readActorBoxes(s.boxes);
    const float kk = float(k);
    const int gameHeight = int(std::lround(float(targetHeight) / kk));
    s.k = kk;
    for (const mobhp::Shown& m : list) {
        const auto it = s.boxes.find(m.id);
        if (it == s.boxes.end()) continue;  // not on the screen now
        const mobhp::Bar b = mobhp::barOver(it->second.data(), m.target, gameHeight);
        s.bars.push_back(Bar{b.x * kk, b.y * kk, b.w * kk, b.h * kk, m.hp});
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
    int cursor = -1, cx = 0, cy = 0;  // under the game's own cursor, as the panels
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const float edge = std::max(1.0f, s.k * 0.75f);
    for (const Bar& b : s.bars) {
        drawFill(record, b.x - edge, b.y - edge, b.w + 2 * edge, b.h + 2 * edge, s.k, 0xB4000000u);
        const float done = b.w * float(std::clamp(b.hp, 0, 100)) / 100.0f;
        drawFill(record, b.x, b.y, done, b.h, s.k, mobhp::fillColour(b.hp));
    }
    if (s.text)
        drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.textX, s.textY, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

int hp(uint32_t id) {
    const State& s = state();
    return s.mode == MobHp::Off ? -1 : s.table.hp(id);
}

bool targetBarTop(uint32_t id, const float box[4], int screenHeight, float& top) {
    const State& s = state();
    if (!id || id != s.target || s.bars.empty()) return false;
    top = mobhp::barOver(box, true, screenHeight).y;
    return true;
}

}  // namespace uagfx::win::mobhpwin
