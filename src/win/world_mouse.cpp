#include "win/world_mouse.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>

#include "pad/win/client_mouse.hpp"
#include "pad/win/game_window.hpp"
#include "win/hotbar.hpp"
#include "win/equip_window.hpp"
#include "win/game_camera.hpp"
#include "win/item_window.hpp"
#include "win/log.hpp"
#include "win/cart_window.hpp"
#include "win/storage_window.hpp"
#include "win/trade_window.hpp"

namespace uagfx::win {
namespace {

// uaRO.exe's: its windows' manager's look at the mouse (the manager in ECX), its one call in the game mode's frame
// (FUN_00566c80), and a plain window's cursor - the arrow on the game mode (two arguments it does not read; each
// window's slot +0x78, the manager calls it on the window under the cursor every frame)
using InputFn = int(__attribute__((thiscall)) *)(uint32_t manager);
using ArrowFn = void(__attribute__((stdcall)) *)(int x, int y);
constexpr uint32_t kInput = 0x4CCFB0, kArrow = 0x4C9D10;
constexpr uint32_t kCall = 0x566C9A;
// 3.0.11-beta.33 (the owner: «поверх диалога справочника мышкой курсор видит нпс на локации»): the actor the game's
// cursor points at - the world's actors' list in ECX (0x74B5F0), the cursor's x and y (floats; ret 8), its one call in
// the game mode's frame. It looks through every window: the frame's hover keeps the last monster or player pointed at
// (mode [0x39], [0x3a]) and from there names an NPC and sets its cursor without asking whose the cursor is
using PickFn = void*(__attribute__((thiscall)) *)(uint32_t world, float x, float y);
constexpr uint32_t kPick = 0x4F9890, kPickCall = 0x566DFA;
// 3.0.11-beta.40: a skill or an item dragged and let go - the game mode's drop (its message 0x12, FUN_005707d0) asks
// the windows' manager for a shown window there (FUN_004ccef0: ECX the manager, x, y, the drag; ret 0xC, AL taken);
// none, an item goes on the ground (its count's dialog). Over a part of the driver's: the hotbar takes it into its
// cell, the rest as a window that takes nothing (the base window's drop FUN_004c9910: the drag ended)
using DropFn = int(__attribute__((thiscall)) *)(uint32_t manager, int x, int y, uint32_t drag);
using NoDropFn = void(__attribute__((thiscall)) *)(uint32_t window, int x, int y, uint32_t drag);
constexpr uint32_t kDrop = 0x4CCEF0, kDropCall = 0x57357B, kNoDrop = 0x4C9910;

// The bytes the call, the answer's use and both functions are made of in the build this knows.
struct Known {
    uint32_t at;
    const char* bytes;
    size_t size;
};
const Known kKnown[] = {
    {0x566C95, "\xB9\xD8\x7E\x72\x00\xE8\x11\x63\xF6\xFF\x8B\xF8", 12},  // mov ecx,manager; call; mov edi,eax
    {0x566D03, "\x83\xFF\x02\x75\x07", 5},                                // cmp edi,2: the world's after all
    {0x4CCFB0, "\x55\x8B\xEC\x83\xEC\x28\xA1\x04\xE7\x6E\x00", 11},      // the manager's: the cursor's x first
    {0x4C9D10, "\x8B\x0D\x7C\xB5\x74\x00\x6A\x00\xE8\x33\xBD\x02\x00\xC2\x08\x00", 16},  // the mode; 0; ret 8
    // mov ecx,actors; fstp; mov edx,y; push edx; push eax (x); call the pick; mov ebx,..; mov edi,eax (the actor)
    {0x566DED, "\xB9\xF0\xB5\x74\x00\xD9\x5D\xF8\x8B\x55\xF8\x52\x50\xE8\x91\x2A\xF9\xFF", 18},
    {0x566DFF, "\x8B\x1D\xE4\x7F\x72\x00\x8B\xF8", 8},
    {0x4F9890, "\x55\x8B\xEC\x83\xEC\x08\x53\x56\x8B\xF1\x57", 11},  // the pick's: this in ESI, its list
    // 3.0.11-beta.40: push the drag, y, x; mov ecx,manager; call the manager's drop; test al,al
    {0x573573, "\x56\x50\x51\xB9\xD8\x7E\x72\x00\xE8\x70\x99\xF5\xFF\x84\xC0", 15},
    {0x4CCEF0, "\x55\x8B\xEC\x51\x53\x56\x8B\x71\x38\x57", 10},  // the manager's drop: its list
    // the base window's drop: mov ecx,mode; push 0 x3; push 0x13; call [mode's +0x14]; ret 0xC
    {0x4C9910, "\x8B\x0D\x7C\xB5\x74\x00\x6A\x00\x6A\x00\x6A\x00\x8B\x01\x6A\x13\xFF\x50\x14\xC2\x0C\x00", 22},
};

// In Blocker's order (3.0.11-beta.33: the big map was left out - the world reacted under it)
const char* const kParts[] = {"the windows' panel", "the answer card", "the new minimap", "«Довідник»",
                              "the new minimap's big map", "the buffs' column", "the character block",
                              "the target's card", "the party's panel", "the hotbar", "the chat",
                              "the new «Речі»", "the new «Характеристики»", "the buffs' further columns",
                              "the tracker", "the interface's layout", "the controller's windows' tile",
                              "the controller's «Призначити»", "the new «Навички»", "the new «Спорядження»",
                              "the NPC talk", "the shop", "the new «Склад»", "the new «Обмін»", "the new menu",
                              "the new item card", "the new skill card", "the new request or input",
                              "the new message box", "the new login", "the new character select",
                              "the new character make", "the new «Візок»", "the new «Лавка»",
                              "the new choice of things", "the new party's settings", "the new pet card",
                              "the new homunculus card", "the new mercenary card", "the new options card",
                              "the new friends and party card", "the new server select", "the new guild card",
                              "the new own shop card", "the new whisper settings", "the new macros card",
                              "the login screen's bar", "the shelf's «додано» note", "the new tip of the day",
                              "the new emotions card", "the new command list", "the new chat room card",
                              "the new chat room", "the new popup menu", "the new whisper cards",
                              "the mercenary's block", "the hunt's panel", "the MVP panel",
                              "the new shop's opening", "uaUI's window"};
static_assert(std::size(kParts) == size_t(uapad::gamewindow::kBlockerCount), "a name for each part");
// 3.0.11-beta.167: 64 bits - the parts passed 32 at 3.0.11-beta.154 («Візок»: part 32), a 32-bit 1u << 32 wrapped
uint64_t g_told = 0;    // the parts whose first cursor over them is logged
uint64_t g_hidden = 0;  // 3.0.11-beta.33: the parts whose first actor hidden under them is logged
static_assert(uapad::gamewindow::kBlockerCount <= 64, "a bit for each part in g_told and g_hidden");
int g_drops = 0;        // 3.0.11-beta.40: the drops over the parts logged
// 3.0.11-beta.99: this frame's answer (1: a window under the cursor - the game's or a part) and the actor the game's
// cursor points at in the world (the pick's record, its id at +0x18)
int g_answer = 0;
uint32_t g_pointed = 0;
DWORD g_pointedAt = 0;
constexpr uint32_t kRecordId = 0x18;
constexpr DWORD kPointedMs = 500;  // the game mode's frame not run since: none

int windowsAnswer(uint32_t manager) {
    const int answer = reinterpret_cast<InputFn>(uintptr_t(kInput))(manager);
    if (answer == 1) return answer;  // a window of the game's own, or one holding the mouse
    int x = 0, y = 0, width = 0, height = 0;
    if (!uapad::clientmouse::read(x, y, width, height)) return answer;
    const int part = uapad::gamewindow::blockedUnder(x, y, width, height);
    if (part < 0 || part >= int(std::size(kParts))) return answer;
    if (answer == 0) reinterpret_cast<ArrowFn>(uintptr_t(kArrow))(0, 0);  // no window of the game's set one there
    if (!(g_told & (uint64_t(1) << part))) {
        g_told |= uint64_t(1) << part;
        LOG_INFO("world mouse: the cursor over %s (%d, %d in %dx%d) - a window for the game, the world under it does "
                 "not react", kParts[part], x, y, width, height);
    }
    return 1;
}

int __attribute__((thiscall)) windowsInput(uint32_t manager) {
    g_answer = windowsAnswer(manager);
    return g_answer;
}

// 3.0.11-beta.33: no actor under a part of the driver's - the frame's hover forgets the last one pointed at as over
// the bare ground, and names none
void* pickedUnder(uint32_t world, float x, float y) {
    void* const actor = reinterpret_cast<PickFn>(uintptr_t(kPick))(world, x, y);
    int cx = 0, cy = 0, width = 0, height = 0;
    if (!actor || !uapad::clientmouse::read(cx, cy, width, height)) return actor;
    const int part = uapad::gamewindow::blockedUnder(int(x), int(y), width, height);
    if (part < 0 || part >= int(std::size(kParts))) return actor;
    if (!(g_hidden & (uint64_t(1) << part))) {
        g_hidden |= uint64_t(1) << part;
        LOG_INFO("world mouse: under %s the game pointed at an actor (%d, %d) - hidden: no name, no talk or sword "
                 "cursor through it", kParts[part], int(x), int(y));
    }
    return nullptr;
}

// 3.0.11-beta.211 (the tester: «якось важко відкрити його, наче треба влучити в якийсь хітбокс менший за розмірами
// ніж персонаж»): the facts first - at a right press in the world, the actors' pick boxes near the cursor (the pick's
// quad tree: a leaf's list +0x28 of {next, prev, box: x0 y0 z x1 y1 depth}, children +0x14..+0x20) and the game's
// smallest box (0x76D2BC: 40 x its screen scale, made once)
constexpr uint32_t kSmallestBox = 0x76D2BC;
struct Closest {
    float dist = 1e9f, w = 0, h = 0;
    int boxes = 0;
};

void nearBoxes(uint32_t node, float x, float y, int depth, Closest& out) {
    if (!node || depth > 8) return;
    uint32_t children[4] = {};
    if (!readGame(node + 0x14, children, sizeof children)) return;
    if (children[0]) {
        for (const uint32_t c : children) nearBoxes(c, x, y, depth + 1, out);
        return;
    }
    uint32_t head = 0;
    if (!readGame(node + 0x28, &head, 4) || !head) return;
    uint32_t at = 0;
    readGame(head, &at, 4);
    for (int n = 0; at && at != head && n < 512; ++n) {
        uint32_t boxAt = 0, next = 0;
        float b[6] = {};
        if (!readGame(at + 8, &boxAt, 4) || !readGame(at, &next, 4) || !boxAt || !readGame(boxAt, b, sizeof b)) break;
        ++out.boxes;
        const float dx = std::max({0.0f, b[0] - x, x - b[3]}), dy = std::max({0.0f, b[1] - y, y - b[4]});
        const float d = std::sqrt(dx * dx + dy * dy);
        if (d < out.dist) out = Closest{d, b[3] - b[0], b[4] - b[1], out.boxes};
        at = next;
    }
}

void tellRightPress(uint32_t world, float x, float y, const void* actor) {
    static bool wasDown = false;
    static int told = 0;
    const bool down = GetAsyncKeyState(VK_RBUTTON) < 0;
    const bool pressed = down && !wasDown;
    wasDown = down;
    if (!pressed || told >= 30 || g_answer == 1) return;
    ++told;
    Closest c;
    nearBoxes(world, x, y, 0, c);
    uint32_t smallest = 0;
    readGame(kSmallestBox, &smallest, 4);
    LOG_INFO("world mouse: a right press at (%d, %d) - %s; the nearest of %d boxes %.0f px away, %.0fx%.0f (the "
             "game's smallest %u)",
             int(x), int(y), actor ? "an actor under it" : "no actor under it", c.boxes, c.boxes ? c.dist : -1.0f,
             c.w, c.h, smallest);
}

void* __attribute__((thiscall)) pickedActor(uint32_t world, float x, float y) {
    void* const actor = pickedUnder(world, x, y);
    tellRightPress(world, x, y, actor);  // 3.0.11-beta.211: the facts about the pick boxes
    g_pointed = 0;  // 3.0.11-beta.99: over a window the world's actor is not pointed at
    if (actor && g_answer != 1) std::memcpy(&g_pointed, static_cast<const uint8_t*>(actor) + kRecordId, 4);
    g_pointedAt = GetTickCount();
    return actor;
}

// 3.0.11-beta.40: a drag let go over a part of the driver's is a window's drop, not the ground's
int __attribute__((thiscall)) droppedOn(uint32_t manager, int x, int y, uint32_t drag) {
    const int answer = reinterpret_cast<DropFn>(uintptr_t(kDrop))(manager, x, y, drag);
    if (answer & 0xFF) return answer;  // a window of the game's own took it
    int cx = 0, cy = 0, width = 0, height = 0;
    if (!uapad::clientmouse::read(cx, cy, width, height)) return answer;
    const int part = uapad::gamewindow::blockedUnder(x, y, width, height);
    if (part < 0 || part >= int(std::size(kParts))) return answer;
    // 3.0.11-beta.42: «Речі» gives it to the game's own window (from the storage, the cart, the gear into the bag)
    const bool taken = (part == int(uapad::gamewindow::Blocker::Hotbar) && hotbarwin::drop(x, y, drag)) ||
                       (part == int(uapad::gamewindow::Blocker::Items) && itemswin::drop(x, y, drag)) ||
                       (part == int(uapad::gamewindow::Blocker::Equip) && equipwin::drop(x, y, drag)) ||  // .97
                       (part == int(uapad::gamewindow::Blocker::Storage) && storagewin::drop(x, y, drag)) ||  // .115
                       (part == int(uapad::gamewindow::Blocker::Trade) && tradewin::drop(x, y, drag)) ||  // .116
                       (part == int(uapad::gamewindow::Blocker::Cart) && cartwin::drop(x, y, drag));  // .154
    if (!taken) reinterpret_cast<NoDropFn>(uintptr_t(kNoDrop))(0, x, y, drag);
    if (g_drops++ < 10)
        LOG_INFO("world mouse: a drag let go over %s (%d, %d) - %s", kParts[part], x, y,
                 taken ? "the part took it" : "a window that takes nothing, not the ground's drop");
    return 1;
}

// A call's 4 bytes after its E8 to `to`
bool redirect(uint32_t call, const void* to) {
    auto* rel = reinterpret_cast<uint8_t*>(uintptr_t(call + 1));
    DWORD old = 0;
    if (!VirtualProtect(rel, 4, PAGE_EXECUTE_READWRITE, &old)) return false;
    const int32_t by = int32_t(reinterpret_cast<uintptr_t>(to) - (uintptr_t(call) + 5));
    std::memcpy(rel, &by, sizeof by);
    DWORD unused = 0;
    VirtualProtect(rel, 4, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), rel, 4);
    return true;
}

}  // namespace

bool installWorldMouse() {
    static int done = -1;  // once: 1 hooked, 0 not
    if (done >= 0) return done == 1;
    done = 0;
    for (const Known& k : kKnown) {
        uint8_t bytes[32] = {};
        SIZE_T got = 0;
        if (k.size > sizeof bytes ||
            !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(k.at)), bytes, k.size,
                               &got) ||
            got != k.size || std::memcmp(bytes, k.bytes, k.size) != 0) {
            LOG_INFO("world mouse: the world reacts to the cursor under the driver's parts too - the game's code is "
                     "not where this build of uaRO.exe has it (0x%X)", k.at);
            return false;
        }
    }
    if (!uapad::clientmouse::supported()) {
        LOG_INFO("world mouse: the world reacts to the cursor under the driver's parts too - the game's cursor is "
                 "not where this build of uaRO.exe has it");
        return false;
    }
    if (!redirect(kCall, reinterpret_cast<const void*>(&windowsInput)) ||
        !redirect(kPickCall, reinterpret_cast<const void*>(&pickedActor)) ||
        !redirect(kDropCall, reinterpret_cast<const void*>(&droppedOn))) {
        LOG_INFO("world mouse: the world reacts to the cursor under the driver's parts too - the game's code cannot "
                 "be changed (error %lu)", GetLastError());
        return false;
    }
    done = 1;
    LOG_INFO("world mouse: the driver's parts in the picture are windows for the game (uaRO.exe 0x%X), the world's "
             "actors under them hidden (0x%X), a drag let go over them a window's (0x%X) - the world under them does "
             "not react to the cursor", kCall, kPickCall, kDropCall);
    return true;
}

uint32_t pointedActor() {
    return g_pointed && GetTickCount() - g_pointedAt <= kPointedMs ? g_pointed : 0;
}

bool cursorOverWorld() { return g_answer != 1; }  // 3.0.13-beta.11

}  // namespace uagfx::win
