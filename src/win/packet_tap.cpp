#include "win/packet_tap.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "core/game_packets.hpp"
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

uint16_t __attribute__((thiscall)) take(void* connection, const uint8_t* packet) {
    const uint16_t id = g_take(connection, packet);
    const packets::Event e = packets::parse(packet, kReadable);
    if (!std::holds_alternative<std::monostate>(e)) notifier::packet(e);
    return id;
}

}  // namespace

void installPacketTap() {
    static bool done = false;
    if (done) return;
    done = true;
    auto* at = reinterpret_cast<uint8_t*>(uintptr_t(kCallAt));
    if (std::memcmp(at, kCall, sizeof kCall) != 0 ||
        std::memcmp(reinterpret_cast<const void*>(uintptr_t(kTakeId)), kTakeIdCode, sizeof kTakeIdCode) != 0)
        return;  // not this uaRO.exe's packet loop (logged with the notifications' first frame)
    g_take = reinterpret_cast<TakeFn>(uintptr_t(kTakeId));
    DWORD was = 0;
    if (!VirtualProtect(at + 1, 4, PAGE_EXECUTE_READWRITE, &was)) return;
    const int32_t to = int32_t(reinterpret_cast<uintptr_t>(&take) - (uintptr_t(kCallAt) + 5));
    std::memcpy(at + 1, &to, 4);
    DWORD unused = 0;
    VirtualProtect(at + 1, 4, was, &unused);
    FlushInstructionCache(GetCurrentProcess(), at, 5);
    g_on = true;
}

bool packetTapOn() { return g_on; }

}  // namespace uagfx::win
