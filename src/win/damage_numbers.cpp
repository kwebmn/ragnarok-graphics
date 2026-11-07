#include "win/damage_numbers.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "core/damage_numbers.hpp"
#include "core/game_camera.hpp"
#include "core/hotbar_panel.hpp"
#include "core/ui_style.hpp"
#include "win/code_detour.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"

namespace uagfx::win::damagewin {
namespace {

namespace dn = damagenum;
// uaRO.exe 0x557EB0, FUN_00557eb0(this actor, from, value, ARGB, kind) - its first instructions (push ebp; mov ebp,
// esp; push -1; push 0x68F95B), whole
constexpr uint32_t kShow = 0x557EB0;
constexpr uint8_t kShowCode[] = {0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68, 0x5B, 0xF9, 0x68, 0x00};
constexpr uint32_t kActorId = 0x208;
constexpr float kBody = 16.0f;  // a character's height in the world's units, about (3.0.13-beta.27's measure)
constexpr size_t kMost = 64;

using ShowFn = void(__attribute__((thiscall)) *)(uint32_t self, uint32_t from, int value, int argb, int kind);
uintptr_t g_original = 0;
std::atomic<bool> g_on{false};

struct Number {
    uint32_t actor = 0;
    float at[3] = {};
    int value = 0;
    uint32_t argb = 0;
    dn::Kind kind = dn::Kind::Other;
    ULONGLONG born = 0;
};
std::vector<Number> g_numbers;  // the game's thread both makes them (its message 0x58) and draws them (frame)

void __attribute__((thiscall)) shown(uint32_t self, uint32_t from, int value, int argb, int kind) {
    if (!g_on.load()) {
        reinterpret_cast<ShowFn>(g_original)(self, from, value, argb, kind);
        return;
    }
    Number n;
    std::memcpy(&n.actor, reinterpret_cast<const void*>(uintptr_t(self + kActorId)), 4);
    std::memcpy(n.at, reinterpret_cast<const void*>(uintptr_t(self + 4)), sizeof n.at);  // its place in the world
    n.value = value;
    n.argb = uint32_t(argb);
    n.kind = dn::kindOf(kind);
    n.born = GetTickCount64();
    if (g_numbers.size() >= kMost) g_numbers.erase(g_numbers.begin());
    g_numbers.push_back(n);
}

struct Label {
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int x = 0, y = 0;
    float opacity = 1;
};
struct State {
    std::vector<Label> pool;
    size_t used = 0;
    float k = 0;
    bool told = false;
};
State& state() {
    static State s;
    return s;
}

}  // namespace

void install() {
    static bool done = false;
    if (done) return;
    done = true;
    g_original = codedetour::detour(kShow, kShowCode, sizeof kShowCode, reinterpret_cast<const void*>(&shown));
    LOG_INFO("damage numbers: the game's «show a number» %s", g_original ? "caught" : "not caught (another uaRO.exe)");
}

void frame(HWND game, bool on, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.used = 0;
    g_on = on && g_original;
    std::vector<Number> now;
    {
        const ULONGLONG t = GetTickCount64();
        g_numbers.erase(std::remove_if(g_numbers.begin(), g_numbers.end(),
                                       [&](const Number& n) { return t - n.born > dn::kLifeMs; }),
                        g_numbers.end());
        if (!g_on) g_numbers.clear();
        now = g_numbers;
    }
    if (now.empty() || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || !inGameWorld()) return;
    float view[12];
    if (!readGameCamera(view)) return;
    const int gameW = int(std::lround(targetWidth / k)), gameH = int(std::lround(targetHeight / k));
    const camera::Projection p = camera::projectionFor(gameW, gameH);
    const float scale = uistyle::readingScale(k);
    const ULONGLONG t = GetTickCount64();
    s.k = float(k);
    for (const Number& n : now) {
        const dn::Pose pose = dn::poseAt(uint32_t(t - n.born), n.kind);
        float w[3] = {n.at[0], n.at[1] - kBody * (1.0f + pose.rise), n.at[2]};  // the world's y goes down: up is -
        float c[3];
        for (int j = 0; j < 3; ++j) c[j] = w[0] * view[j] + w[1] * view[3 + j] + w[2] * view[6 + j] + view[9 + j];
        if (c[2] <= p.nearZ) continue;
        const float sx = c[0] / c[2] * p.f0 + p.cx, sy = c[1] / c[2] * p.f1 + p.cy;
        if (sx < -50 || sy < -50 || sx > float(gameW) + 50 || sy > float(gameH) + 50) continue;
        if (s.used >= s.pool.size()) s.pool.emplace_back();
        Label& l = s.pool[s.used++];
        const std::string text = dn::textOf(n.value, n.kind);
        const uint32_t rgb = dn::rgbOf(n.argb, n.kind);
        const float px = std::round(17.0f * scale * pose.size);
        const std::string key = text + '#' + std::to_string(rgb) + '#' + std::to_string(int(px));
        if (key != l.key) {
            l.key = key;
            const int lw = int(std::ceil(panel::wordsWidth(text, px, true) + 8 * scale));
            const int lh = int(std::ceil(px * 1.5f));
            if (panel::blank(l.picture, lw, lh)) {
                drawWords(l.picture, {Words{panel::Box{0, 0, float(lw), float(lh)}, utf8Wide(text), rgb, px, true,
                                            DT_CENTER, false, true}});
                l.upload = true;
            }
        }
        l.x = int(sx * float(k)) - l.picture.width / 2;
        l.y = int(sy * float(k)) - l.picture.height / 2;
        l.opacity = pose.opacity;
    }
    if (s.used && !s.told) {
        s.told = true;
        LOG_INFO("damage numbers: the game's numbers drawn as words ([interface] damage_numbers)");
    }
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.used || s.k <= 0) return;
    const size_t world = record.firstInterface(), before = record.commands().size();
    for (size_t i = 0; i < s.used; ++i) {
        Label& l = s.pool[i];
        if (l.picture.width > 0)
            drawPart(record, renderer, l.picture, l.texture, l.texW, l.texH, l.upload, l.x, l.y, s.k, l.opacity);
    }
    const size_t added = record.commands().size() - before;
    if (added) record.moveLastBefore(added, world);
}

}  // namespace uagfx::win::damagewin
