#pragma once
#include <string>

#include "core/settings/game_registry.hpp"

namespace uagfx::settings {

// A copy of the game's key as a .reg file (Windows Registry Editor Version 5.00) under `root`, the key's full path
// (e.g. "HKEY_CURRENT_USER\\Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\WOW6432Node\\Gravity Soft\\Ragnarok").
// UTF-8 with CRLF; the Windows layer stores it as UTF-16 with a BOM, as regedit does (stage 2 spec §4.6).
std::string toRegFile(const RegCopy& copy, const std::string& root);
// Reads what toRegFile writes, and regedit's own files: the keys under `root` (others are skipped), "name"=value lines
// with "text", dword:, hex: and hex(N): values and '\' line continuations. false when it is not a .reg file.
bool fromRegFile(const std::string& text, const std::string& root, RegCopy& out);

}  // namespace uagfx::settings
