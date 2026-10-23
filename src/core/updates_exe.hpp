#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::updates_exe {

// 3.0.2-beta.5: uaRO.exe's (63902c46) archive functions the client's graphics updates go in front of
// (core/updates.hpp, win/updates_hook.cpp): CFileMgr::GetData (thiscall: name, &size; a copy VirtualAlloc'd, size + 8
// bytes, freed by the game with VirtualFree), CFileMgr::IsExist (thiscall: name) and CFile::IsExist (cdecl: name).
// Each one's first 9 bytes, whole instructions (push ebp; mov ebp, esp; sub esp, n): another build is left alone.
constexpr uint32_t kGetData = 0x50D120, kHasFile = 0x50D0A0, kFileExists = 0x50D690;
constexpr size_t kPrologue = 9;
extern const uint8_t kGetDataBytes[kPrologue], kHasFileBytes[kPrologue], kFileExistsBytes[kPrologue];

}  // namespace uagfx::updates_exe
