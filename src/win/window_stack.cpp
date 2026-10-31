#include "win/window_stack.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "pad/win/game_window.hpp"
#include "win/chat_window.hpp"
#include "win/equip_window.hpp"
#include "win/item_window.hpp"
#include "win/log.hpp"
#include "win/npc_talk.hpp"
#include "win/reference_window.hpp"
#include "win/shop_window.hpp"
#include "win/skill_window.hpp"
#include "win/stat_window.hpp"

namespace uagfx::win::windowstack {
namespace {

using winstack::Win;
using uapad::gamewindow::Blocker;

constexpr const char* kNames[] = {"«Речі»",  "«Характеристики»", "«Навички»", "«Спорядження»",
                                  "the shop", "the NPC talk",      "«Довідник»"};
constexpr int kMostTold = 40;

struct Dock {  // 3.0.11-beta.111: «Спорядження» beside «Речі»
    bool on = false;
    winstack::Side side = winstack::Side::Left;  // the gear's side of the bag
    bool unsaved = false;                        // the follower moved during a drag, its place not kept yet
};

struct State {
    winstack::Stack stack;
    Dock dock;
    std::optional<Win> told;  // the top the log named last
    int tells = 0;
};

State& state() {
    static State s;
    return s;
}

const char* nameOf(Win w) { return kNames[int(w)]; }

void tell(State& s, const char* what, Win w) {
    if (s.tells >= kMostTold) return;
    ++s.tells;
    LOG_INFO("windows: %s %s (%zu shown)", nameOf(w), what, s.stack.bottomUp().size());
}

bool shown(Win w) {
    switch (w) {
        case Win::Items: return itemswin::shown();
        case Win::Stats: return statswin::shown();
        case Win::Skills: return skillswin::shown();
        case Win::Equip: return equipwin::shown();
        case Win::Shop: return shopwin::shown();
        case Win::Npc: return npcwin::shown();
        case Win::Reference: return referencewin::isOpen();
    }
    return false;
}

Blocker blockerOf(Win w) {
    switch (w) {
        case Win::Items: return Blocker::Items;
        case Win::Stats: return Blocker::Stats;
        case Win::Skills: return Blocker::Skills;
        case Win::Equip: return Blocker::Equip;
        case Win::Shop: return Blocker::Shop;
        case Win::Npc: return Blocker::Npc;
        case Win::Reference: break;
    }
    return Blocker::Reference;
}

void drawOne(Win w, FrameRecord& record, gpu::Renderer& renderer) {
    switch (w) {
        case Win::Items: itemswin::draw(record, renderer); break;
        case Win::Stats: statswin::draw(record, renderer); break;
        case Win::Skills: skillswin::draw(record, renderer); break;
        case Win::Equip: equipwin::draw(record, renderer); break;
        case Win::Shop: shopwin::draw(record, renderer); break;
        case Win::Npc: npcwin::draw(record, renderer); break;
        case Win::Reference: referencewin::draw(record, renderer); break;
    }
}

bool mouseOne(Win w, HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    switch (w) {
        case Win::Items: return itemswin::mouse(game, m, wp, lp, result);
        case Win::Stats: return statswin::mouse(game, m, wp, lp, result);
        case Win::Skills: return skillswin::mouse(game, m, wp, lp, result);
        case Win::Equip: return equipwin::mouse(game, m, wp, lp, result);
        case Win::Shop: return shopwin::mouse(game, m, wp, lp, result);
        case Win::Npc: return npcwin::mouse(game, m, wp, lp, result);
        case Win::Reference: return referencewin::mouse(game, m, wp, lp, result);
    }
    return false;
}

bool keyOne(Win w, UINT m, WPARAM wp, LPARAM lp) {  // the windows with keys of their own
    switch (w) {
        case Win::Items: return itemswin::key(m, wp, lp);  // its search's while it types
        case Win::Shop: return shopwin::key(m, wp, lp);
        case Win::Npc: return npcwin::key(m, wp, lp);
        case Win::Reference: return referencewin::key(m, wp, lp);
        default: return false;
    }
}

bool movable(Win w) { return w != Win::Npc && w != Win::Reference; }

bool boxOf(Win w, winstack::Box& b) {
    switch (w) {
        case Win::Items: return itemswin::box(b.x, b.y, b.w, b.h);
        case Win::Stats: return statswin::box(b.x, b.y, b.w, b.h);
        case Win::Skills: return skillswin::box(b.x, b.y, b.w, b.h);
        case Win::Equip: return equipwin::box(b.x, b.y, b.w, b.h);
        case Win::Shop: return shopwin::box(b.x, b.y, b.w, b.h);
        case Win::Npc: return npcwin::box(b.x, b.y, b.w, b.h);
        case Win::Reference: return referencewin::box(b.x, b.y, b.w, b.h);
    }
    return false;
}

void moveTo(Win w, int x, int y, bool keep = true) {
    switch (w) {
        case Win::Items: itemswin::moveTo(x, y, keep); break;
        case Win::Stats: statswin::moveTo(x, y); break;
        case Win::Skills: skillswin::moveTo(x, y); break;
        case Win::Equip: equipwin::moveTo(x, y, keep); break;
        case Win::Shop: shopwin::moveTo(x, y); break;
        default: break;
    }
}

// 3.0.11-beta.111 (question 1: «два окна вплотную друг к другу как одно окно»): «Спорядження» and «Речі» opened
// together stand side by side, tops in line (core/window_stack docked) - the gear left of the bag where there is room;
// the one dragged by its title leads, the other follows (kept when the drag ends); Shift held while dragging - apart
void dockFrame(State& s, bool opened, int targetW, int targetH) {
    winstack::Box bag, gear;
    if (!boxOf(Win::Items, bag) || !boxOf(Win::Equip, gear)) {
        s.dock = Dock{};
        return;
    }
    const bool bagMoving = itemswin::moving(), gearMoving = equipwin::moving(), dragged = bagMoving || gearMoving;
    if (opened && !s.dock.on) {
        s.dock.on = true;
        s.dock.side = winstack::dockSide(bag, gear, targetW);
        if (s.tells < kMostTold) {
            ++s.tells;
            LOG_INFO("windows: «Спорядження» docked %s of «Речі»",
                     s.dock.side == winstack::Side::Left ? "left" : "right");
        }
    }
    if (!s.dock.on) return;
    if (dragged && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
        s.dock.on = false;
        if (s.tells < kMostTold) {
            ++s.tells;
            LOG_INFO("windows: «Спорядження» and «Речі» apart (dragged with Shift)");
        }
        return;
    }
    const bool gearLeads = gearMoving && !bagMoving;
    winstack::Box want = gearLeads ? winstack::docked(gear, bag, winstack::opposite(s.dock.side))
                                   : winstack::docked(bag, gear, s.dock.side);
    want.x = std::clamp(want.x, 0, std::max(0, targetW - want.w));  // the screen's edge: as near as it goes
    want.y = std::clamp(want.y, 0, std::max(0, targetH - want.h));
    const Win follower = gearLeads ? Win::Items : Win::Equip;
    const winstack::Box at = gearLeads ? bag : gear;
    if (want.x != at.x || want.y != at.y) {
        moveTo(follower, want.x, want.y, !dragged);
        s.dock.unsaved = dragged;
    } else if (!dragged && s.dock.unsaved) {
        moveTo(follower, at.x, at.y, true);  // the drag over: the follower's place kept too
        s.dock.unsaved = false;
    }
}

// 3.0.11-beta.109 (question 4 «ок»): a window just opened where another one stands - beside it (8 game pixels apart),
// else 24 down and right of it (core/window_stack place)
void placeNew(State& s, Win w, int targetW, int targetH, double k) {
    winstack::Box b;
    if (!movable(w) || !boxOf(w, b) || targetW <= 0 || targetH <= 0) return;
    std::vector<winstack::Box> open;
    for (const Win o : s.stack.bottomUp()) {
        winstack::Box ob;
        if (boxOf(o, ob)) open.push_back(ob);
    }
    const int gap = int(std::lround(8 * k)), step = int(std::lround(24 * k));
    const winstack::Box to = winstack::place(b, open, targetW, targetH, gap, step);
    if (to.x == b.x && to.y == b.y) return;
    moveTo(w, to.x, to.y);
    if (s.tells < kMostTold) {
        ++s.tells;
        LOG_INFO("windows: %s opened where another stands - at %d, %d instead of %d, %d", nameOf(w), to.x, to.y, b.x,
                 b.y);
    }
}

// Esc on the top one: closed as its own ✕ does (the NPC talk and the shop have an Esc of their own)
bool close(Win w) {
    switch (w) {
        case Win::Items: itemswin::padCommand(itemswin::PadCommand::Close); return true;
        case Win::Stats: statswin::padCommand(statswin::PadCommand::Close); return true;
        case Win::Skills: skillswin::padCommand(skillswin::PadCommand::Close); return true;
        case Win::Equip: equipwin::padCommand(equipwin::PadCommand::Close); return true;
        case Win::Reference: referencewin::padCommand(referencewin::PadCommand::Close); return true;
        default: return false;
    }
}

}  // namespace

void frame(int targetWidth, int targetHeight, double k) {
    State& s = state();
    bool pairOpened = false;  // 3.0.11-beta.111: «Речі» or «Спорядження» opened this frame
    for (int i = 0; i < winstack::kWins; ++i) {
        const Win w = Win(i);
        const bool now = shown(w);
        const auto& order = s.stack.bottomUp();
        if (now && std::find(order.begin(), order.end(), w) == order.end()) {
            placeNew(s, w, targetWidth, targetHeight, k);  // the others' places before it joins them
            pairOpened = pairOpened || w == Win::Items || w == Win::Equip;
        }
        s.stack.see(w, now);
    }
    dockFrame(s, pairOpened, targetWidth, targetHeight);
    std::vector<Blocker> order;
    for (const Win w : s.stack.topDown()) order.push_back(blockerOf(w));
    uapad::gamewindow::setOrder(order);
    const std::optional<Win> t = s.stack.top();
    if (t != s.told && t && s.stack.bottomUp().size() >= 2) tell(s, "on top", *t);
    s.told = t;
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    for (const Win w : state().stack.bottomUp()) drawOne(w, record, renderer);
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    const bool press = m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN || m == WM_RBUTTONDBLCLK ||
                       m == WM_MBUTTONDOWN;
    for (const Win w : s.stack.topDown()) {
        if (!mouseOne(w, game, m, wp, lp, result)) continue;
        if (press && s.stack.top() != w) {
            s.stack.raise(w);
            s.told = w;
            tell(s, "raised by a click", w);
        }
        return true;
    }
    return false;
}

bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    const std::optional<Win> t = s.stack.top();
    if (t && keyOne(*t, m, wp, lp)) return true;  // the top's own keys first
    if (t && m == WM_KEYDOWN && wp == VK_ESCAPE && chatwin::inputEmpty() && close(*t)) {
        tell(s, "closed by Esc (the top one)", *t);
        return true;
    }
    for (const Win w : s.stack.topDown())
        if ((!t || w != *t) && keyOne(w, m, wp, lp)) return true;
    return !referencewin::isOpen() && referencewin::key(m, wp, lp);  // Ctrl+F opens it
}

std::optional<winstack::Win> top() { return state().stack.top(); }

bool marked(winstack::Win w) { return state().stack.marked(w); }

}  // namespace uagfx::win::windowstack
