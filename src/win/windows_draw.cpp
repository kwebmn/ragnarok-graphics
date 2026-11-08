#include "win/windows_draw.hpp"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "core/world_windows.hpp"
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
#include "win/msg_box.hpp"
#include "win/menu_window.hpp"
#include "win/trade_window.hpp"

namespace uagfx::win {
namespace {

// uaRO.exe's: the windows' manager's draw (ECX the manager, g_windowMgr 0x727ED8; it draws each window of its list
// with its shown flag +0x38 set) and its call in the game mode's frame
using DrawFn = void(__attribute__((thiscall)) *)(uint32_t manager);
constexpr uint32_t kDraw = 0x4CD720, kDrawCall = 0x559972;
constexpr uint32_t kOtherCalls[] = {0x4EAD24, 0x5A030E};  // 3.0.11-beta.203: the login's modes' (FUN_004ea9a0, ..a0180)
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
    mobhpwin::beforeGameDraw();   // 3.0.11-beta.100: the monsters' per cent asked
    npctagwin::beforeGameDraw();  // 3.0.11-beta.256: the tagged NPC's name asked
    charpanel::beforeGameDraw();  // the Basic Info window
    hotbarwin::beforeGameDraw();  // the hotkeys' bar
    chatwin::beforeGameDraw();    // the chat (out of sight)
}

void __attribute__((thiscall)) windowsDraw(uint32_t manager) {
    mendWindowFields(manager, "before its windows' draw");  // 3.0.11-beta.194 (R-ZS5B2D): a freed one's name
    hideReplaced();
    std::vector<std::pair<uint32_t, int32_t>> held, signs;
    holdSigns(manager, signs);  // 3.0.16-beta.7
    const size_t gauges = holdShown(manager, false, held);  // 3.0.11-beta.191: the bars under the characters first
    if (gauges > 0) reinterpret_cast<DrawFn>(uintptr_t(kDraw))(manager);
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
    LOG_INFO("windows draw: %d of the login's modes' draws of the game's windows hide the replaced ones first too",
             others);
    return true;
}

}  // namespace uagfx::win
