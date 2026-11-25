#include "win/windows_draw.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "core/game_camera.hpp"
#include "core/gauge_scale.hpp"
#include "core/own_ground.hpp"
#include "core/world_words.hpp"
#include "core/world_windows.hpp"
#include "core/world_signs.hpp"
#include "win/world_signs.hpp"
#include "win/char_block.hpp"
#include "win/context.hpp"
#include "win/game_camera.hpp"
#include "win/chat_window.hpp"
#include "win/hotbar.hpp"
#include "win/item_window.hpp"
#include "win/log.hpp"
#include "win/equip_window.hpp"
#include "win/mob_hp.hpp"
#include "win/npc_tag.hpp"
#include "win/npc_talk.hpp"
#include "win/shop_window.hpp"
#include "win/skill_window.hpp"
#include "win/stat_window.hpp"
#include "win/window_guard.hpp"
#include "win/storage_window.hpp"
#include "win/ask_card.hpp"
#include "win/info_card.hpp"
#include "win/cart_window.hpp"
#include "win/char_make.hpp"
#include "win/char_select.hpp"
#include "win/party_setup.hpp"
#include "win/guild_card.hpp"
#include "win/homun_card.hpp"
#include "win/merc_card.hpp"
#include "win/my_shop.hpp"
#include "win/vend_setup.hpp"
#include "win/options_card.hpp"
#include "win/server_card.hpp"
#include "win/social_card.hpp"
#include "win/pet_card.hpp"
#include "win/pick_card.hpp"
#include "win/vend_shop.hpp"
#include "win/whisper_setup.hpp"
#include "win/login_card.hpp"
#include "win/macro_card.hpp"
#include "win/tip_window.hpp"
#include "win/emote_window.hpp"
#include "win/command_window.hpp"
#include "win/chatroom_window.hpp"
#include "win/room_window.hpp"
#include "win/detail_window.hpp"
#include "win/popup_window.hpp"
#include "win/whisper_window.hpp"
#include "win/loading_bar.hpp"
#include "win/msg_box.hpp"
#include "win/menu_window.hpp"
#include "win/trade_window.hpp"
#include "win/view3d.hpp"
#include "win/far_bars.hpp"

namespace uagfx::win {
namespace {

// uaRO.exe's: the windows' manager's draw (ECX the manager, g_windowMgr 0x727ED8; it draws each window of its list
// with its shown flag +0x38 set) and its call in the game mode's frame
using DrawFn = void(__attribute__((thiscall)) *)(uint32_t manager);
constexpr uint32_t kDraw = 0x4CD720, kDrawCall = 0x559972;
// 3.0.11-beta.203: the login's modes' (FUN_004ea9a0, ..a0180); 3.0.18-beta.180: the map loading's first frame
// (FUN_00559030 makes its loading bar, window 7, at 0x559241 and draws the windows at once)
constexpr uint32_t kOtherCalls[] = {0x4EAD24, 0x5A030E, 0x559264};
struct Known {
    uint32_t at;
    const char* bytes;
    size_t size;
};
const Known kKnown[] = {
    {0x55996D, "\xB9\xD8\x7E\x72\x00\xE8\xA9\x3D\xF7\xFF", 10},  // mov ecx,manager; call the draw
    {0x4CD720, "\x55\x8B\xEC\x83\xEC\x20\x8B\x41\x38\x57", 10},  // the draw's: its windows' list
};
int g_told = 0;
// 3.0.11-beta.185 (the tester: «вода іде оверлеем навіть над меню - і моби з деревами теж»): the renderer ([0x6EE3D8])
// files the interface's faces into its list +0xEC ({depth, face} 8 bytes each, its end at +0xF0) and draws them in
// that order after the world, its depth off (FUN_0040f320; a face with no vertices - its + 4 - skipped)
constexpr uint32_t kRenderer = 0x6EE3D8, kInterfaceList = 0xEC;

}  // namespace

// The interface's faces the game will draw that it filed so far (before its windows: the names over the heads, the
// chat's balloons; at its scene's begin: all)
size_t interfaceFaces() {
    uint32_t renderer = 0, list[2] = {};
    if (!readGame(kRenderer, &renderer, 4) || !renderer || !readGame(renderer + kInterfaceList, list, sizeof list) ||
        list[1] < list[0] || (list[1] - list[0]) % 8 != 0 || (list[1] - list[0]) / 8 > 100000)
        return SIZE_MAX;
    size_t drawn = 0;
    for (uint32_t at = list[0]; at < list[1]; at += 8) {  // the game's own list, read as it reads it a moment later
        const uint32_t face = *reinterpret_cast<const uint32_t*>(uintptr_t(at + 4));
        if (face && *reinterpret_cast<const int32_t*>(uintptr_t(face + 4)) != 0) ++drawn;
    }
    return drawn;
}

namespace {

// 3.0.11-beta.191 (R-FG9GCX «не пофиксилось хп под плашкой интерфейса»): the bars under the characters are the
// manager's windows too - an actor's HP/SP gauge (60 x 9 at its + 0x360, class 0x696D4C), its other gauge (60 x 5 at
// + 0x364, 0x696CB4), its cast's (60 x 6 at + 0x220, 0x696C18; message 0x52) - each drawn by the manager's own draw
// (FUN_004cd720: a window with its shown flag + 0x38, from its list + 0x38 - a node: next + 0, the window + 8). A pass
// of that draw of theirs alone first (the other windows' flags held at 0): their faces among those before the game's
// windows, under the driver's; then the rest with theirs held at 0
// 3.0.11-beta.195 (the tester: a skill's shout over a monk's head over «Навички»): the words over the heads and the
// chat rooms' and shops' signs with them (core/world_windows)
constexpr uint32_t kWindowsList = 0x38, kWindowShown = 0x38;
constexpr size_t kMostWindows = 512;
int g_gaugesTold = 0;

// 3.0.19-beta.17 (R-9KRT3M, R-TNAYJF: the character left out by the 3D view, its HP/SP bar hanging in the air): the
// character's own bar - the game mode's + 0x2E8 (FUN_005707d0 makes it; the others' are their actors' own) - held
// at 0 through the draw while the 3D view leaves the character out
constexpr uint32_t kModeNowAt = 0x74B57C, kModeOwnGauge = 0x2E8;
int g_ownGaugeTold = 0;

void holdOwnGauge(std::vector<std::pair<uint32_t, int32_t>>& held) {
    const uint32_t mode = *reinterpret_cast<const uint32_t*>(uintptr_t(kModeNowAt));
    uint32_t gauge = 0;
    if (!mode || !readGame(mode + kModeOwnGauge, &gauge, 4) || !gauge) return;
    int32_t shown = 0;
    if (!readGame(gauge + 0x38, &shown, 4) || !shown) return;
    held.emplace_back(gauge, shown);
    *reinterpret_cast<int32_t*>(uintptr_t(gauge + 0x38)) = 0;
    if (g_ownGaugeTold < 3) {
        ++g_ownGaugeTold;
        LOG_INFO("windows draw: the character's own HP/SP bar left out - the 3D view leaves the character out");
    }
}

bool worldGauge(uint32_t window) {
    return worldwindows::worldWindow(*reinterpret_cast<const uint32_t*>(uintptr_t(window)));
}

// The shown windows of the manager's list that are (gauges) or are not (the rest) the world's - the bars, the words
// and the signs over the characters - their shown flags held at 0 (kept in `held` to put back)
size_t holdShown(uint32_t manager, bool gauges, std::vector<std::pair<uint32_t, int32_t>>& held) {
    const uint32_t head = *reinterpret_cast<const uint32_t*>(uintptr_t(manager + kWindowsList));
    size_t seen = 0, others = 0;
    for (uint32_t node = head ? *reinterpret_cast<const uint32_t*>(uintptr_t(head)) : 0;
         node && node != head && seen < kMostWindows; node = *reinterpret_cast<const uint32_t*>(uintptr_t(node))) {
        ++seen;
        const uint32_t w = *reinterpret_cast<const uint32_t*>(uintptr_t(node + 8));
        auto* shown = reinterpret_cast<int32_t*>(uintptr_t(w + kWindowShown));
        if (!w || !*shown) continue;
        if (worldGauge(w) != gauges) {
            ++others;
            continue;
        }
        held.emplace_back(w, *shown);
        *shown = 0;
    }
    return others;  // the shown ones left drawn
}

// 3.0.20-beta.84 (R-7WTVBF «когда отдаляю, то полоска хп выглядит огромной»): [interface] far_bars - the faces the
// pass of the world's windows filed (the renderer's list from entry `from` on) that lie in a shown gauge's box (each
// gauge one face of its surface's texture, FUN_00403880 -> FUN_00417c70: its vertices at the face's + 0, their count
// at + 4, its indices' count at + 0x10, 32 bytes a vertex with x and y first - as FUN_0040f320 draws it) smaller
// toward their stack's top middle by the camera's distance (core/gauge_scale, win/far_bars)
constexpr uint32_t kWindowBox = 0x14;  // x, y, width, height (FUN_004cd720 draws the surface there)
constexpr size_t kFaceStride = 32;     // D3DFVF 0x1C4: x y z rhw, two colours, u v
int g_farTold = 0, g_farMissTold = 0;

bool interfaceList(uint32_t list[2]) {
    uint32_t renderer = 0;
    return readGame(kRenderer, &renderer, 4) && renderer && readGame(renderer + kInterfaceList, list, 8) &&
           list[1] >= list[0] && (list[1] - list[0]) % 8 == 0 && (list[1] - list[0]) / 8 <= 100000;
}

void shrinkGauges(uint32_t manager, size_t from) {
    const float scale = farbars::scaleNow();
    if (!(scale < 1.0f)) return;
    std::vector<std::array<int, 4>> boxes;
    const uint32_t head = *reinterpret_cast<const uint32_t*>(uintptr_t(manager + kWindowsList));
    size_t seen = 0;
    for (uint32_t node = head ? *reinterpret_cast<const uint32_t*>(uintptr_t(head)) : 0;
         node && node != head && seen < kMostWindows; node = *reinterpret_cast<const uint32_t*>(uintptr_t(node))) {
        ++seen;
        const uint32_t w = *reinterpret_cast<const uint32_t*>(uintptr_t(node + 8));
        if (!w || !*reinterpret_cast<const int32_t*>(uintptr_t(w + kWindowShown)) ||
            !gaugescale::gaugeClass(*reinterpret_cast<const uint32_t*>(uintptr_t(w))))
            continue;
        std::array<int, 4> box{};
        if (readGame(w + kWindowBox, box.data(), sizeof box) && box[2] > 0 && box[3] > 0 && box[2] <= 512 &&
            box[3] <= 64)
            boxes.push_back(box);
    }
    uint32_t list[2] = {};
    if (boxes.empty() || !interfaceList(list)) return;
    const std::vector<std::array<float, 2>> anchors = gaugescale::anchors(boxes);
    size_t shrunk = 0, faces = 0;
    for (uint32_t at = list[0] + uint32_t(from) * 8; at < list[1]; at += 8) {
        ++faces;
        uint32_t face = 0, f[5] = {};
        if (!readGame(at + 4, &face, 4) || !face || !readGame(face, f, sizeof f) || !f[0] || !f[1] || f[1] > 8 || f[4])
            continue;
        auto* vertices = reinterpret_cast<uint8_t*>(uintptr_t(f[0]));
        const int b = gaugescale::boxOf(vertices, f[1], kFaceStride, boxes);
        if (b < 0) continue;
        gaugescale::scaleVertices(vertices, f[1], kFaceStride, anchors[size_t(b)][0], anchors[size_t(b)][1], scale);
        ++shrunk;
    }
    if (shrunk && g_farTold < 3) {
        ++g_farTold;
        LOG_INFO("windows draw: %zu face(s) of %zu shown gauge(s) under the characters at %.0f %% of their size "
                 "([interface] far_bars)", shrunk, boxes.size(), scale * 100.0f);
    } else if (!shrunk && g_farMissTold < 2) {
        ++g_farMissTold;
        LOG_INFO("windows draw: %zu shown gauge(s), none of the pass's %zu face(s) in their boxes - the bars under the "
                 "characters at the game's size ([interface] far_bars)", boxes.size(), faces);
    }
}

// 3.0.20-beta.85 (R-H3S9MY «ник персонажа который ходит гдето не рядом с ним а на верху, также непонятно с мобами
// бывает такое»): in the 3D view an actor's words over its head (its + 0x214, FUN_0054ac00) and the hovered one's name
// (the game mode's + 0x2DC; the actor's id at + 0x314, the name's place at + 0x40C - under the feet for an actor whose
// + 0x94 is 0) placed as the game places them (core/world_words), from the actor's feet as the 3D view's camera puts
// them on the screen now - the game's own point (+ 0x98, + 0x9C) and scale (+ 0x48) are its render's, an older
// frame's for an actor the game did not draw. Behind the eye, behind the ground or a model, or the character left
// out - not drawn (their shown flags put back after the game's draw)
using MoveFn = void(__attribute__((thiscall)) *)(uint32_t window, int x, int y);
constexpr uint32_t kActorWords = 0x214, kActorNameUnder = 0x94, kActorPoint = 0x98;
constexpr uint32_t kModeHoverName = 0x2DC, kModeHovered = 0x314, kModeNamePlace = 0x40C;
constexpr uint32_t kSpriteScale = 0x695798, kSpriteNorm = 0x6D5EFC, kOwnAccount = 0x771F7C;
constexpr uint32_t kWindowMove = 0x18;  // the window's vtable: Move(x, y), as the game moves them
// their classes (FUN_004418f0; FUN_005674f0 by the name's place: FUN_00441bf0's, or its subclass for 2 and 3) - a
// pointer of another kind at an actor's + 0x214 (a ground item's) never taken for one
constexpr uint32_t kWordsClass = 0x696068, kNameClass = 0x696100, kNameSideClass = 0x69F3F4;
int g_wordsTold = 0, g_wordsHiddenTold = 0;

bool shownWindow(uint32_t w, uint32_t wantClass, uint32_t orClass = 0) {
    uint32_t table = 0;
    int32_t shown = 0;
    return w && readGame(w, &table, 4) && (table == wantClass || (orClass && table == orClass)) &&
           readGame(w + kWindowShown, &shown, 4) && shown;
}

void moveWindow(uint32_t w, worldwords::Point to) {
    int32_t box[2] = {};
    uint32_t table = 0, move = 0;
    if (!readGame(w + kWindowBox, box, sizeof box) || (box[0] == to.x && box[1] == to.y) || !readGame(w, &table, 4) ||
        !table || !readGame(table + kWindowMove, &move, 4) || !move)
        return;
    reinterpret_cast<MoveFn>(uintptr_t(move))(w, to.x, to.y);
}

// any words over a head shown now (most frames none: the actors not walked)
bool anyWords(uint32_t manager) {
    const uint32_t head = *reinterpret_cast<const uint32_t*>(uintptr_t(manager + kWindowsList));
    size_t seen = 0;
    for (uint32_t node = head ? *reinterpret_cast<const uint32_t*>(uintptr_t(head)) : 0;
         node && node != head && seen < kMostWindows; node = *reinterpret_cast<const uint32_t*>(uintptr_t(node))) {
        ++seen;
        if (shownWindow(*reinterpret_cast<const uint32_t*>(uintptr_t(node + 8)), kWordsClass)) return true;
    }
    return false;
}

void placeWorldWords(uint32_t manager, std::vector<std::pair<uint32_t, int32_t>>& hidden) {
    if (!view3dwin::active()) return;
    const uint32_t mode = *reinterpret_cast<const uint32_t*>(uintptr_t(kModeNowAt));
    uint32_t renderer = 0, nameWindow = 0, hovered = 0, own = 0, namePlace = 0;
    int32_t size[2] = {};
    float view[12], k = 0.0f, norm = 0.0f;
    if (!mode || !readGame(kRenderer, &renderer, 4) || !renderer || !readGame(renderer + 0x24, size, sizeof size) ||
        size[0] <= 0 || size[1] <= 0 || !readGameCamera(view) || !readGame(kSpriteScale, &k, 4) ||
        !readGame(kSpriteNorm, &norm, 4))
        return;
    readGame(mode + kModeHoverName, &nameWindow, 4);
    readGame(mode + kModeHovered, &hovered, 4);
    readGame(mode + kModeNamePlace, &namePlace, 4);
    readGame(kOwnAccount, &own, 4);
    if (!shownWindow(nameWindow, kNameClass, kNameSideClass)) nameWindow = 0;
    if (!nameWindow && !anyWords(manager)) return;
    static std::vector<ActorAt> actors;
    readActors(actors);
    const worldwords::Screen screen{size[0], size[1]};
    const camera::Projection p = camera::projectionFor(size[0], size[1]);
    for (const ActorAt& a : actors) {
        uint32_t words = 0;
        readGame(a.actor + kActorWords, &words, 4);
        if (!shownWindow(words, kWordsClass)) words = 0;
        const uint32_t name = a.id == hovered ? nameWindow : 0;
        if (!words && !name) continue;
        const float c[3] = {a.at[0] * view[0] + a.at[1] * view[3] + a.at[2] * view[6] + view[9],
                            a.at[0] * view[1] + a.at[1] * view[4] + a.at[2] * view[7] + view[10],
                            a.at[0] * view[2] + a.at[1] * view[5] + a.at[2] * view[8] + view[11]};
        own_ground::Tl2 feet{};
        const float head[3] = {a.at[0], a.at[1] - 12.0f, a.at[2]};  // y down
        const char* why = nullptr;
        if (a.id == own && view3dwin::ownHidden())
            why = "the character left out";
        else if (!own_ground::placeView(p, c, feet) || !std::isfinite(feet.x) || !std::isfinite(feet.y))
            why = "behind the eye";
        else if (a.id != own && !view3dwin::seenActor(a.id, head))
            why = "behind the ground or a model";
        if (why) {
            for (const uint32_t w : {words, name}) {
                if (!w) continue;
                auto* shown = reinterpret_cast<int32_t*>(uintptr_t(w + kWindowShown));
                hidden.emplace_back(w, *shown);
                *shown = 0;
            }
            if (g_wordsHiddenTold < 3) {
                ++g_wordsHiddenTold;
                LOG_INFO("3D view: actor %u's words over its head not drawn - %s", a.id, why);
            }
            continue;
        }
        const float scale = worldwords::scaleOf(feet.rhw, p.f0, k, norm);
        int32_t point[2] = {};
        readGame(a.actor + kActorPoint, point, sizeof point);
        for (const uint32_t w : {words, name}) {
            int32_t wh[2] = {};
            if (!w || !readGame(w + kWindowBox + 8, wh, sizeof wh)) continue;
            int32_t own94 = 1;  // 0: the name under the feet whatever the game mode's place
            readGame(a.actor + kActorNameUnder, &own94, 4);
            const int place = own94 ? int(namePlace) : 0;
            const worldwords::Point to = w == words
                                             ? worldwords::wordsAt(feet.x, feet.y, scale, wh[0], wh[1], screen)
                                             : worldwords::nameAt(place, feet.x, feet.y, scale, wh[0], wh[1], screen);
            if (g_wordsTold < 6) {
                ++g_wordsTold;
                int32_t was[2] = {};
                readGame(w + kWindowBox, was, sizeof was);
                LOG_INFO("3D view: actor %u's %s at %d, %d - from its feet on the screen now (%.0f, %.0f, scale %.2f); "
                         "the game had it at %d, %d from its point %d, %d", a.id, w == words ? "words" : "name",
                         to.x, to.y, feet.x, feet.y, scale, was[0], was[1], point[0], point[1]);
            }
            moveWindow(w, to);
        }
    }
}

// 3.0.23-beta.12 (R-BC2QMP «название лавки висит в воздухе, хотя я его уже прошел»): in the 3D view the shops' signs
// placed by the game's own camera - a shop passed hung before the eye. Placed now as the words over the heads: over its
// merchant's head from its feet on the screen; out of sight (moved off the screen, so no click finds it either) when
// the merchant is behind the eye or behind the ground or a model
int g_signsTold = 0;

void placeShopSigns(uint32_t manager) {
    if (!view3dwin::active()) return;
    const uint32_t mode = *reinterpret_cast<const uint32_t*>(uintptr_t(kModeNowAt));
    uint32_t renderer = 0;
    int32_t size[2] = {};
    float view[12], k = 0.0f, norm = 0.0f;
    if (!mode || !readGame(kRenderer, &renderer, 4) || !renderer || !readGame(renderer + 0x24, size, sizeof size) ||
        size[0] <= 0 || size[1] <= 0 || !readGameCamera(view) || !readGame(kSpriteScale, &k, 4) ||
        !readGame(kSpriteNorm, &norm, 4))
        return;
    std::vector<uint32_t> shops;
    const uint32_t head = *reinterpret_cast<const uint32_t*>(uintptr_t(manager + kWindowsList));
    size_t seen = 0;
    for (uint32_t node = head ? *reinterpret_cast<const uint32_t*>(uintptr_t(head)) : 0;
         node && node != head && seen < kMostWindows; node = *reinterpret_cast<const uint32_t*>(uintptr_t(node))) {
        ++seen;
        const uint32_t w = *reinterpret_cast<const uint32_t*>(uintptr_t(node + 8));
        uint32_t table = 0;
        if (w && readGame(w, &table, 4) && table == uagfx::worldsigns::kShopClass) shops.push_back(w);
    }
    if (shops.empty()) return;
    static std::vector<ActorAt> actors;
    readActors(actors);
    const worldwords::Screen screen{size[0], size[1]};
    const camera::Projection p = camera::projectionFor(size[0], size[1]);
    for (const uint32_t w : shops) {
        uint32_t account = 0;
        int32_t wh[2] = {};
        if (!readGame(w + uagfx::worldsigns::kShopAccount, &account, 4) || !readGame(w + kWindowBox + 8, wh, sizeof wh))
            continue;
        const auto it = std::find_if(actors.begin(), actors.end(), [account](const ActorAt& a) {
            return a.id == account;
        });
        if (it == actors.end()) continue;  // its merchant not among the actors: the game's place
        const ActorAt& a = *it;
        const float c[3] = {a.at[0] * view[0] + a.at[1] * view[3] + a.at[2] * view[6] + view[9],
                            a.at[0] * view[1] + a.at[1] * view[4] + a.at[2] * view[7] + view[10],
                            a.at[0] * view[2] + a.at[1] * view[5] + a.at[2] * view[8] + view[11]};
        own_ground::Tl2 feet{};
        const float headAt[3] = {a.at[0], a.at[1] - 12.0f, a.at[2]};  // y down
        const char* why = nullptr;
        if (!own_ground::placeView(p, c, feet) || !std::isfinite(feet.x) || !std::isfinite(feet.y))
            why = "behind the eye";
        else if (!view3dwin::seenActor(a.id, headAt))
            why = "behind the ground or a model";
        worldwords::Point to{-4096, -4096};
        if (!why) to = worldwords::wordsAt(feet.x, feet.y, worldwords::scaleOf(feet.rhw, p.f0, k, norm), wh[0], wh[1],
                                           screen);
        if (g_signsTold < 4) {
            ++g_signsTold;
            if (why)
                LOG_INFO("3D view: shop %u's sign out of sight - its merchant %s", account, why);
            else
                LOG_INFO("3D view: shop %u's sign over its merchant's head at %d, %d", account, to.x, to.y);
        }
        moveWindow(w, to);
    }
}

// 3.0.16-beta.7: [interface] signs - the shops' and the chat rooms' signs drawn anew (win/world_signs): their shown
// flags held at 0 through both passes of the game's draw (kept in `held` to put back - the clicks still find them)
void holdSigns(uint32_t manager, std::vector<std::pair<uint32_t, int32_t>>& held) {
    const uint32_t head = *reinterpret_cast<const uint32_t*>(uintptr_t(manager + kWindowsList));
    size_t seen = 0;
    for (uint32_t node = head ? *reinterpret_cast<const uint32_t*>(uintptr_t(head)) : 0;
         node && node != head && seen < kMostWindows; node = *reinterpret_cast<const uint32_t*>(uintptr_t(node))) {
        ++seen;
        const uint32_t w = *reinterpret_cast<const uint32_t*>(uintptr_t(node + 8));
        if (!w) continue;
        auto* shown = reinterpret_cast<int32_t*>(uintptr_t(w + kWindowShown));
        if (!*shown || !worldsigns::take(w)) continue;
        held.emplace_back(w, *shown);
        *shown = 0;
    }
}

void putBack(std::vector<std::pair<uint32_t, int32_t>>& held) {
    for (const auto& [w, shown] : held) *reinterpret_cast<int32_t*>(uintptr_t(w + kWindowShown)) = shown;
    held.clear();
}

// The windows the driver draws anew - each one's game window out of sight before the game draws its windows
// (3.0.11-beta.203: in the login's modes too - their message boxes, «Please wait...», the login, the characters)
void hideReplaced() {
    itemswin::beforeGameDraw();   // «Речі»
    statswin::beforeGameDraw();   // «Характеристики»
    skillswin::beforeGameDraw();  // 3.0.11-beta.96: «Навички»
    equipwin::beforeGameDraw();   // 3.0.11-beta.97: «Спорядження»
    npcwin::beforeGameDraw();     // 3.0.11-beta.98: the NPC talk
    shopwin::beforeGameDraw();    // 3.0.11-beta.100: the shop (its deal's choice pressed)
    storagewin::beforeGameDraw();  // 3.0.11-beta.115: «Склад» (and «Речі» made beside it)
    tradewin::beforeGameDraw();    // 3.0.11-beta.116: «Обмін» (and «Речі» made beside it)
    menuwin::beforeGameDraw();     // 3.0.11-beta.140: the menu and the death screen
    infocardwin::beforeGameDraw();  // 3.0.11-beta.149: the item card and the skill card
    askcardwin::beforeGameDraw();   // 3.0.11-beta.150: the requests and the NPC's inputs
    msgboxwin::beforeGameDraw();    // 3.0.11-beta.151: the game's message boxes
    loginwin::beforeGameDraw();     // 3.0.11-beta.151: the login
    charselectwin::beforeGameDraw();  // 3.0.11-beta.152: the character select
    charmakewin::beforeGameDraw();    // 3.0.11-beta.153: the character make
    cartwin::beforeGameDraw();        // 3.0.11-beta.154: «Візок»
    vendshopwin::beforeGameDraw();    // 3.0.11-beta.156: «Лавка»
    pickcardwin::beforeGameDraw();    // 3.0.11-beta.157: the choice of things
    partysetupwin::beforeGameDraw();  // 3.0.11-beta.158: the party's settings
    petcardwin::beforeGameDraw();     // 3.0.11-beta.159: the pet
    homuncardwin::beforeGameDraw();   // 3.0.11-beta.160: the homunculus
    merccardwin::beforeGameDraw();    // 3.0.11-beta.161: the mercenary
    optionscardwin::beforeGameDraw();  // 3.0.11-beta.162: the game's options
    socialcardwin::beforeGameDraw();   // 3.0.11-beta.163: the friends and the party
    servercardwin::beforeGameDraw();   // 3.0.11-beta.164: the server select
    guildcardwin::beforeGameDraw();    // 3.0.11-beta.165: the guild
    myshopwin::beforeGameDraw();       // 3.0.11-beta.166: one's own shop
    vendsetupwin::beforeGameDraw();    // 3.0.16-beta.33: the shop's opening
    whispersetupwin::beforeGameDraw();  // 3.0.11-beta.168: the whisper's settings
    macrocardwin::beforeGameDraw();     // 3.0.11-beta.169: the chat's macros
    tipwin::beforeGameDraw();           // 3.0.11-beta.204: the tip of the day
    emotewin::beforeGameDraw();         // 3.0.11-beta.205: the emotion list
    commandwin::beforeGameDraw();       // 3.0.11-beta.206: the command list
    chatroomwin::beforeGameDraw();      // 3.0.11-beta.207: the chat rooms
    roomwin::beforeGameDraw();          // 3.0.11-beta.208: the chat room's own window
    detailwin::beforeGameDraw();        // 3.0.11-beta.209: Alt+D - the driver's console
    popupwin::beforeGameDraw();         // 3.0.11-beta.211: the popup menus
    whisperwin::beforeGameDraw();       // 3.0.11-beta.212: the whisper windows
    loadingbarwin::beforeGameDraw();    // 3.0.18-beta.180: the map loading bar
    mobhpwin::beforeGameDraw();   // 3.0.11-beta.100: the monsters' per cent asked
    npctagwin::beforeGameDraw();  // 3.0.11-beta.256: the tagged NPC's name asked
    charpanel::beforeGameDraw();  // the Basic Info window
    hotbarwin::beforeGameDraw();  // the hotkeys' bar
    chatwin::beforeGameDraw();    // the chat (out of sight)
}

void __attribute__((thiscall)) windowsDraw(uint32_t manager) {
    mendWindowFields(manager, "before its windows' draw");  // 3.0.11-beta.194 (R-ZS5B2D): a freed one's name
    hideReplaced();
    std::vector<std::pair<uint32_t, int32_t>> ownHeld;  // 3.0.19-beta.17
    if (view3dwin::ownHidden()) holdOwnGauge(ownHeld);
    std::vector<std::pair<uint32_t, int32_t>> held, signs, words;
    placeShopSigns(manager);    // 3.0.23-beta.12 (R-BC2QMP): the 3D view's
    holdSigns(manager, signs);  // 3.0.16-beta.7
    placeWorldWords(manager, words);  // 3.0.20-beta.85 (R-H3S9MY): the 3D view's
    const size_t gauges = holdShown(manager, false, held);  // 3.0.11-beta.191: the bars under the characters first
    uint32_t filed[2] = {};  // 3.0.20-beta.84: the interface's faces before that pass
    const bool listed = gauges > 0 && interfaceList(filed);
    if (gauges > 0) reinterpret_cast<DrawFn>(uintptr_t(kDraw))(manager);
    if (listed) shrinkGauges(manager, (filed[1] - filed[0]) / 8);
    putBack(held);
    Context::get().markGameWindows(interfaceFaces());  // 3.0.11-beta.109: its own windows from here on - over ours
    if (g_told < 1) {
        ++g_told;
        LOG_INFO("windows draw: the game's replaced windows hidden before it draws its windows (uaRO.exe 0x%X)",
                 kDrawCall);
    }
    if (gauges > 0 && g_gaugesTold < 3) {
        ++g_gaugesTold;
        LOG_INFO("windows draw: %zu of the world's window(s) - the bars, the words and the signs over the characters - "
                 "drawn before the game's windows: under the driver's", gauges);
    }
    if (gauges > 0) holdShown(manager, true, held);
    reinterpret_cast<DrawFn>(uintptr_t(kDraw))(manager);
    putBack(held);
    putBack(signs);
    putBack(ownHeld);
    putBack(words);
}

// 3.0.11-beta.203 (the tester: «при логіні ... там залишились в старому стилі please wait вікна»): the login's modes
// draw the game's windows too (FUN_004ea9a0, FUN_005a0180) - the replaced ones were drawn a frame before the driver's
// frame hid them; out of sight before, as in the game mode
void __attribute__((thiscall)) otherModesDraw(uint32_t manager) {
    hideReplaced();
    reinterpret_cast<DrawFn>(uintptr_t(kDraw))(manager);
}

}  // namespace

bool installWindowsDraw() {
    static int done = -1;  // once: 1 hooked, 0 not
    if (done >= 0) return done == 1;
    done = 0;
    for (const Known& k : kKnown) {
        uint8_t bytes[16] = {};
        SIZE_T got = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(k.at)), bytes, k.size,
                               &got) ||
            got != k.size || std::memcmp(bytes, k.bytes, k.size) != 0) {
            LOG_INFO("windows draw: the game's code is not where this build of uaRO.exe has it (0x%X) - its replaced "
                     "windows hidden at the scene's end, as before", k.at);
            return false;
        }
    }
    auto* rel = reinterpret_cast<uint8_t*>(uintptr_t(kDrawCall + 1));
    DWORD old = 0;
    if (!VirtualProtect(rel, 4, PAGE_EXECUTE_READWRITE, &old)) {
        LOG_INFO("windows draw: the game's code cannot be changed (error %lu)", GetLastError());
        return false;
    }
    const int32_t by = int32_t(reinterpret_cast<uintptr_t>(&windowsDraw) - (uintptr_t(kDrawCall) + 5));
    std::memcpy(rel, &by, sizeof by);
    DWORD unused = 0;
    VirtualProtect(rel, 4, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), rel, 4);
    done = 1;
    // 3.0.11-beta.203: the login's modes' calls of the draw (mov ecx, the manager; call), each where this build has it
    int others = 0;
    for (const uint32_t call : kOtherCalls) {
        uint8_t code[10] = {};
        SIZE_T got = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(call - 5)), code,
                               sizeof code, &got) ||
            got != sizeof code || std::memcmp(code, "\xB9\xD8\x7E\x72\x00\xE8", 6) != 0)
            continue;
        int32_t to = 0;
        std::memcpy(&to, code + 6, 4);
        if (call + 5 + uint32_t(to) != kDraw) continue;
        auto* at = reinterpret_cast<uint8_t*>(uintptr_t(call + 1));
        if (!VirtualProtect(at, 4, PAGE_EXECUTE_READWRITE, &old)) continue;
        const int32_t mine = int32_t(reinterpret_cast<uintptr_t>(&otherModesDraw) - (uintptr_t(call) + 5));
        std::memcpy(at, &mine, sizeof mine);
        VirtualProtect(at, 4, old, &unused);
        FlushInstructionCache(GetCurrentProcess(), at, 4);
        ++others;
    }
    LOG_INFO("windows draw: %d of the login's modes' and the map loading's draws of the game's windows hide the "
             "replaced ones first too",
             others);
    return true;
}

}  // namespace uagfx::win
