#include "win/window_stack.hpp"

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

struct State {
    winstack::Stack stack;
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

void frame() {
    State& s = state();
    for (int i = 0; i < winstack::kWins; ++i) s.stack.see(Win(i), shown(Win(i)));
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
