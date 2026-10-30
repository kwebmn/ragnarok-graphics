#include "win/packet_tap.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "core/game_packets.hpp"
#include "win/buff_overlay.hpp"
#include "win/party_panel.hpp"
#include "win/cast_bar.hpp"
#include "win/chat_window.hpp"
#include "win/target_card.hpp"
#include "win/notifier.hpp"

namespace uagfx::win {
namespace {

constexpr uint32_t kCallAt = 0x579B02, kTakeId = 0x419350;
const uint8_t kCall[] = {0xE8, 0x49, 0xF8, 0xE9, 0xFF};
// push ebp; mov ebp, esp; mov eax, [ebp+8]; mov ax, [eax]; pop ebp; ret 4 - the id from the packet's first bytes
const uint8_t kTakeIdCode[] = {0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08, 0x66, 0x8B, 0x00, 0x5D, 0xC2, 0x04, 0x00};
// The loop's buffer [ebp-0x54b4] holds the whole packet and the frame goes on past it: this much is read at most
// (a whisper names its own length; the rest are a few dozen bytes)
constexpr size_t kReadable = 2048;

using TakeFn = uint16_t(__attribute__((thiscall)) *)(void* connection, const uint8_t* packet);
TakeFn g_take = nullptr;
bool g_on = false;

// 3.0.7-beta.6 (the user's notification 9): the game mode's loop polls its connection first (`call 0x418c60` at
// 0x579a65, CRagConnection: the socket read and written, false when the server closed it - the socket shut, and
// the game shows its own «disconnected» at once: CModeMgr's vfunc 0x14 (1)). A socket the game shut itself (a map on
// another server) is polled as fine - only the server's closing fails it.
constexpr uint32_t kPollAt = 0x579A65, kPoll = 0x418C60;
const uint8_t kPollCall[] = {0xE8, 0xF6, 0xF1, 0xE9, 0xFF};
// push esi; mov esi, ecx; cmp dword [esi+4], -1; je +0x1f; call 0x418eb0
const uint8_t kPollCode[] = {0x56, 0x8B, 0xF1, 0x83, 0x7E, 0x04, 0xFF, 0x74, 0x1F, 0xE8, 0x42, 0x02, 0x00, 0x00};
using PollFn = char(__attribute__((thiscall)) *)(void* connection);
PollFn g_poll = nullptr;

char __attribute__((thiscall)) poll(void* connection) {
    const char fine = g_poll(connection);
    if (!fine) notifier::disconnected();
    return fine;
}

// The call at `at` (its bytes `call`, the function's first bytes `code`) through `to` instead
bool redirect(uint32_t at, const uint8_t* call, uint32_t fn, const uint8_t* code, size_t codeSize, void* to) {
    auto* p = reinterpret_cast<uint8_t*>(uintptr_t(at));
    if (std::memcmp(p, call, 5) != 0 || std::memcmp(reinterpret_cast<const void*>(uintptr_t(fn)), code, codeSize) != 0)
        return false;
    DWORD was = 0;
    if (!VirtualProtect(p + 1, 4, PAGE_EXECUTE_READWRITE, &was)) return false;
    const int32_t rel = int32_t(reinterpret_cast<uintptr_t>(to) - (uintptr_t(at) + 5));
    std::memcpy(p + 1, &rel, 4);
    DWORD unused = 0;
    VirtualProtect(p + 1, 4, was, &unused);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    return true;
}

uint16_t __attribute__((thiscall)) take(void* connection, const uint8_t* packet) {
    const uint16_t id = g_take(connection, packet);
    const packets::Event e = packets::parse(packet, kReadable);
    if (!std::holds_alternative<std::monostate>(e)) {
        notifier::packet(e);
        buffoverlay::packet(e);  // 3.0.4-beta.6
        targetpanel::packet(e);  // 3.0.11-beta.38: the target's card - who the character fights
        partywin::packet(e);     // 3.0.11-beta.39: the party's panel
        chatwin::packet(e);      // 3.0.11-beta.41: the chat's battle log
        castwin::packet(e);      // 3.0.11-beta.48: the cast's label - which skill
    }
    return id;
}

}  // namespace

void installPacketTap() {
    static bool done = false;
    if (done) return;
    done = true;
    g_take = reinterpret_cast<TakeFn>(uintptr_t(kTakeId));
    // not this uaRO.exe's packet loop: none (logged with the notifications' first frame)
    if (!redirect(kCallAt, kCall, kTakeId, kTakeIdCode, sizeof kTakeIdCode, reinterpret_cast<void*>(&take))) return;
    g_on = true;
    g_poll = reinterpret_cast<PollFn>(uintptr_t(kPoll));  // 3.0.7-beta.6: the server gone
    redirect(kPollAt, kPollCall, kPoll, kPollCode, sizeof kPollCode, reinterpret_cast<void*>(&poll));
}

bool packetTapOn() { return g_on; }

}  // namespace uagfx::win
