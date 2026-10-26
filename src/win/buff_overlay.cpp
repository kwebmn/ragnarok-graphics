#include "win/buff_overlay.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <variant>
#include <vector>

#include "core/buff_timers.hpp"
#include "core/window_panel.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"

namespace uagfx::win::buffoverlay {
namespace {

constexpr uint32_t kIconsBegin = 0x728484, kIconsEnd = 0x728488;  // the player's icons (ints), FUN_00593d00's
constexpr uint32_t kScreen = 0x6EE3D8;  // an object: the screen's width +0x24, height +0x28 (FUN_005d56a0)
constexpr uint32_t kSession = 0x771148, kOwnAccount = 0x771F7C;
constexpr uint32_t kIconImage = 0x648BC0;  // CSession::IconImage(icon, pass): its .tga, or none in that pass
const uint8_t kIconImageCode[] = {0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x0C, 0x83, 0xF8, 0x03,
                                  0x0F, 0x87, 0x95, 0x05, 0x00, 0x00};
constexpr int kIcon = 32, kStep = 35, kTop = 150, kBottomRoom = 50, kMostIcons = 64;
// 3.0.4-beta.36 (R-C8NYXS «подписи под кастами както некрасиво смотрятся»): the layout's place is the icon's middle -
// the game puts each icon as an effect of a 16 half side there (FUN_005d56a0: +0x2d8, +0x2e4 = 16.0f); taken for its
// top left the times stood 16 down and right of their icons, over the next one and cut by the screen's edge (the
// shot: the icon's left 1873 of the game's 1920, the layout's 1888)
constexpr int kHalf = kIcon / 2, kFoot = 13;
using ImageFn = const char*(__attribute__((thiscall)) *)(void* session, uint32_t icon, uint32_t pass);

struct Slot {
    uint16_t icon;
    int x, y;  // its middle, the game's pixels (3.0.4-beta.36)
};

struct State {
    buffs::Timers timers;
    uint32_t account = 0;
    bool on = false, logged = false;
    float k = 0;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0, x = 0, y = 0;
    bool upload = false;
    std::string shown;  // what the picture says, where
};

State& state() {
    static State s;
    return s;
}

bool known() {
    static const bool k = knownGameBuild() && std::memcmp(reinterpret_cast<const void*>(uintptr_t(kIconImage)),
                                                          kIconImageCode, sizeof kIconImageCode) == 0;
    return k;
}

uint32_t own() {
    uint32_t id = 0;
    readGame(kOwnAccount, &id, 4);
    return id;
}

// FUN_005d56a0's layout, with the game's own image lookup
std::vector<Slot> layout() {
    std::vector<Slot> out;
    uint32_t begin = 0, end = 0, screen = 0;
    int32_t size[2] = {0, 0};
    if (!inGameWorld() || !readGame(kIconsBegin, &begin, 4) || !readGame(kIconsEnd, &end, 4) ||
        !readGame(kScreen, &screen, 4) || !begin || end < begin || (end - begin) / 4 > kMostIcons || !screen ||
        !readGame(screen + 0x24, size, sizeof size))
        return out;
    std::vector<int32_t> icons((end - begin) / 4);
    if (!icons.empty() && !readGame(begin, icons.data(), icons.size() * 4)) return out;
    const int height = size[1], width = size[0];
    const auto image = reinterpret_cast<ImageFn>(uintptr_t(kIconImage));
    void* session = reinterpret_cast<void*>(uintptr_t(kSession));
    std::vector<int32_t> keys;
    int inColumn = 0, x = 0, y = 0;
    for (uint32_t pass = 0; pass < 5; ++pass)
        for (const int32_t* it = icons.data(); it != icons.data() + icons.size(); ++it) {
            if (*it < 0 || *it > 0x7FFF) continue;
            const int32_t key = *it == 0x2A ? 0xC : *it;
            if (!image(session, uint32_t(*it), pass)) continue;
            if (std::find(keys.begin(), keys.end(), key) != keys.end()) continue;
            if (inColumn * kStep + kTop < height - kBottomRoom) {
                y += kStep;
            } else {  // a new column to the left
                x -= kStep;
                y = kStep;
                inColumn = 0;
            }
            out.push_back(Slot{uint16_t(*it), width - kIcon + x, y + kTop});  // the game's +0x358, +0x35c
            keys.push_back(key);
            ++inColumn;
        }
    return out;
}

// The picture of the times: on the foot of each icon that has one, white words in a dark rim (3.0.4-beta.36: a dark
// box under them covered a third of the icon)
void render(State& s, const std::vector<Slot>& slots, const std::vector<std::string>& labels) {
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -(1 << 30), y1 = -(1 << 30);
    for (size_t i = 0; i < slots.size(); ++i)
        if (!labels[i].empty()) {
            x0 = std::min(x0, slots[i].x - kHalf), y0 = std::min(y0, slots[i].y - kHalf);
            x1 = std::max(x1, slots[i].x + kHalf), y1 = std::max(y1, slots[i].y + kHalf + 2);  // the rim's room
        }
    s.picture = panel::Picture{};
    if (x1 <= x0) return;
    const float k = s.k;
    s.x = int(std::lround(float(x0) * k));
    s.y = int(std::lround(float(y0) * k));
    s.picture.width = int(std::ceil(float(x1 - x0) * k));
    s.picture.height = int(std::ceil(float(y1 - y0) * k));
    s.picture.px.assign(size_t(s.picture.width) * size_t(s.picture.height), 0u);
    std::vector<Words> words;
    for (size_t i = 0; i < slots.size(); ++i) {
        if (labels[i].empty()) continue;
        const panel::Box foot{float(slots[i].x - kHalf - x0) * k, float(slots[i].y + kHalf - kFoot - y0) * k,
                              float(kIcon) * k, float(kFoot) * k};
        words.push_back(Words{foot, utf8Wide(labels[i]), 0xFFFFFF, 10.5f * k, true, DT_CENTER});
    }
    const float rim = std::max(1.0f, std::round(k));  // the rim: the words in black a pixel each way first
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            if (!dx && !dy) continue;
            std::vector<Words> dark = words;
            for (Words& w : dark) w.box.x += float(dx) * rim, w.box.y += float(dy) * rim, w.rgb = 0x000000;
            drawWords(s.picture, dark);
        }
    drawWords(s.picture, words);
    s.upload = true;
}

}  // namespace

void packet(const packets::Event& e) {
    State& s = state();
    if (!known()) return;
    const uint32_t me = own();
    if (me != s.account) {  // another character: its buffs are its own
        s.account = me;
        s.timers.clear();
    }
    const uint64_t now = GetTickCount64();
    if (const auto* c = std::get_if<packets::SkillCast>(&e)) {
        if (c->target == me) s.timers.cast(c->skill, c->level, now);
    } else if (const auto* i = std::get_if<packets::StatusIcon>(&e)) {
        if (i->id == me) s.timers.icon(i->icon, i->on, now);
    }
}

const buffs::Timers& timers() { return state().timers; }

void frame(bool on, float k) {
    State& s = state();
    s.on = on && known();
    s.k = k;
    if (!s.logged && known()) {
        s.logged = true;
        LOG_INFO("buff timers: the icons laid out as the game lays them ([buffs] timers %s)", on ? "on" : "off");
    }
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.on || s.k <= 0) return;
    const std::vector<Slot> slots = layout();
    const uint64_t now = GetTickCount64();
    std::vector<std::string> labels;
    std::string shown = std::to_string(s.k) + ';';
    for (const Slot& slot : slots) {
        labels.push_back(buffs::label(s.timers.left(slot.icon, now)));
        shown += std::to_string(slot.x) + ',' + std::to_string(slot.y) + ',' + labels.back() + ';';
    }
    if (shown != s.shown) {
        s.shown = shown;
        render(s, slots, labels);
    }
    if (s.picture.width <= 0) return;
    int cursor = -1, cx = 0, cy = 0;  // under the game's cursor, as the windows' panel
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    drawPicture(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.x, s.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

}  // namespace uagfx::win::buffoverlay
