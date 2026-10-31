#include "win/stall_watch.hpp"

#include <atomic>

#include "win/iat.hpp"

namespace uagfx::win {
namespace {

using PeekFn = BOOL(WINAPI*)(LPMSG, HWND, UINT, UINT, UINT);
PeekFn g_next = nullptr;
std::atomic<DWORD> g_thread{0};
std::atomic<uint32_t> g_beats{0};

BOOL WINAPI peekBeat(LPMSG m, HWND hwnd, UINT lo, UINT hi, UINT remove) {
    const DWORD game = g_thread.load(std::memory_order_relaxed);
    if (!game || GetCurrentThreadId() == game) g_beats.fetch_add(1, std::memory_order_relaxed);
    return g_next(m, hwnd, lo, hi, remove);
}

}  // namespace

void installStallWatch() {
    if (!hookImport("user32.dll", "PeekMessageA", &peekBeat, g_next)) g_next = nullptr;
}

void setStallThread(DWORD gameThread) { g_thread.store(gameThread, std::memory_order_relaxed); }

bool stallBeats(uint32_t& beats) {
    beats = g_beats.load(std::memory_order_relaxed);
    return g_next != nullptr;
}

}  // namespace uagfx::win
