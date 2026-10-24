#include "core/game_windows.hpp"

#include <algorithm>
#include <cstring>

namespace uagfx::gamewindows {

void putName(Entry& e, const std::string& utf8) {
    size_t n = std::min(utf8.size(), kName - 1);
    while (n > 0 && n < utf8.size() && (static_cast<unsigned char>(utf8[n]) & 0xC0) == 0x80) --n;  // a letter's tail
    std::memset(e.name, 0, kName);
    std::memcpy(e.name, utf8.data(), n);
}

namespace {

constexpr int kMostLevel = 999;

int8_t percent(int part, int most) {
    if (most <= 0 || part < 0) return -1;
    const int64_t p = int64_t(part) * 100 / most;
    return int8_t(p > 100 ? 100 : p == 0 && part > 0 ? 1 : p);
}

int16_t level(int l) { return int16_t(l > 0 && l <= kMostLevel ? l : 0); }

}  // namespace

void putStatus(Entry& e, const charstatus::Status* s) {
    e.baseLevel = s ? level(s->baseLevel) : 0;
    e.jobLevel = s ? level(s->jobLevel) : 0;
    e.hp = s ? percent(s->hp, s->hpMost) : -1;
    e.sp = s ? percent(s->sp, s->spMost) : -1;
}

std::vector<Window> windows(const Table& t, const Alive& alive) {
    std::vector<Window> out;
    for (int i = 0; i < kMost; ++i) {
        const Entry& e = t.entries[i];
        if (e.pid == 0 || e.number != uint32_t(i + 1) || !alive(e)) continue;
        Window w;
        w.pid = e.pid;
        w.window = e.window;
        w.number = int(e.number);
        w.frontAt = e.frontAt;
        w.name.assign(e.name, strnlen(e.name, kName - 1));
        w.baseLevel = e.baseLevel;
        w.jobLevel = e.jobLevel;
        w.hp = e.hp;
        w.sp = e.sp;
        out.push_back(w);
    }
    return out;  // by number: the places' order
}

const Window* previous(const std::vector<Window>& all, uint32_t selfPid) {
    const Window* best = nullptr;
    for (const Window& w : all)
        if (w.pid != selfPid && w.frontAt > 0 && (!best || w.frontAt > best->frontAt)) best = &w;
    if (best) return best;
    int self = 0;
    for (const Window& w : all)
        if (w.pid == selfPid) self = w.number;
    for (const Window& w : all)  // never in front: the next number, after the last the first
        if (w.pid != selfPid && w.number > self) return &w;
    for (const Window& w : all)
        if (w.pid != selfPid) return &w;
    return nullptr;
}

const Window* byNumber(const std::vector<Window>& all, int number) {
    for (const Window& w : all)
        if (w.number == number) return &w;
    return nullptr;
}

KeyAction keyAction(unsigned vk, bool ctrl, bool alt, bool shift, WindowKeys keys) {
    KeyAction a;
    if (keys == WindowKeys::Off || !ctrl) return a;
    if (vk == kTab && !alt) {
        a.action = Action::Back;
        return a;
    }
    if (keys != WindowKeys::All || !alt || shift) return a;
    const int digit = vk >= '0' && vk <= '9' ? int(vk - '0') : vk >= kPad0 && vk <= kPad0 + 9 ? int(vk - kPad0) : -1;
    if (digit == 0) a.action = Action::Panel;
    if (digit >= 1 && digit <= kMost) {
        a.action = Action::Window;
        a.number = digit;
    }
    return a;
}

}  // namespace uagfx::gamewindows
