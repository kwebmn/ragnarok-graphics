#include "win/ground_stage_patch.hpp"

#include <windows.h>

#include <cstring>

#include "core/d3d7.hpp"
#include "core/ground_stage_exe.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

using GetStage = HRESULT(__stdcall*)(void* device, DWORD stage, DWORD type, DWORD* value);
using SetStage = HRESULT(__stdcall*)(void* device, DWORD stage, DWORD type, DWORD value);
constexpr size_t kGetSlot = 0x90 / 4, kSetSlot = 0x94 / 4;  // IDirect3DDevice7's Get/SetTextureStageState

unsigned g_mended = 0;  // the game's thread's

// The game's frame's call (core/ground_stage_exe): its device given, the op's answer its
HRESULT __stdcall groundStage(void* device) {
    void** table = *static_cast<void***>(device);
    DWORD was = d3d7::TA_TEXTURE;
    reinterpret_cast<GetStage>(table[kGetSlot])(device, 1, d3d7::TSS_COLORARG1, &was);
    if (was != d3d7::TA_TEXTURE && g_mended++ < 3)
        LOG_INFO("ground stage: the light map's argument 0x%lX before the ground (another draw's, left over) - the "
                 "texture again", static_cast<unsigned long>(was));
    HRESULT hr = S_OK;
    for (const groundstage_exe::Step& s : groundstage_exe::kSteps)
        hr = reinterpret_cast<SetStage>(table[kSetSlot])(device, s.stage, s.type, s.value);
    return hr;
}

}  // namespace

bool installGroundStageFix() {
    static bool done = false;
    if (done) return true;
    uint8_t at[groundstage_exe::kSize] = {};
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(groundstage_exe::kAt)), at,
                           sizeof at, &got) ||
        got != sizeof at || std::memcmp(at, groundstage_exe::kOriginal, sizeof at) != 0) {
        LOG_INFO("ground stage: the game's own (its call is not where this build of uaRO.exe has it)");
        return false;
    }
    uint8_t code[groundstage_exe::kSize];
    groundstage_exe::patch(uint32_t(reinterpret_cast<uintptr_t>(&groundStage)), code);
    auto* site = reinterpret_cast<uint8_t*>(uintptr_t(groundstage_exe::kAt));
    DWORD was = 0;
    if (!VirtualProtect(site, sizeof code, PAGE_EXECUTE_READWRITE, &was)) return false;
    std::memcpy(site, code, sizeof code);
    DWORD unused = 0;
    VirtualProtect(site, sizeof code, was, &unused);
    FlushInstructionCache(GetCurrentProcess(), site, sizeof code);
    done = true;
    LOG_INFO("ground stage: the game's ground's light map stage set whole before its draws (uaRO.exe 0x%X) - an "
             "argument another draw left no longer whitens the ground", groundstage_exe::kAt);
    return true;
}

}  // namespace uagfx::win
