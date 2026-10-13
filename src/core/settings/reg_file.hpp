#pragma once
#include <string>
#include <vector>

#include "core/settings/game_registry.hpp"

namespace uagfx::settings {

// A copy of the game's key as a .reg file (Windows Registry Editor Version 5.00) under `root`, the key's full path
// (e.g. "HKEY_CURRENT_USER\\Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\WOW6432Node\\Gravity Soft\\Ragnarok").
// UTF-8 with CRLF; the Windows layer stores it as UTF-16 with a BOM, as regedit does (stage 2 spec §4.6).
std::string toRegFile(const RegCopy& copy, const std::string& root);
// Reads what toRegFile writes, and regedit's own files: the keys under `root` (others are skipped), "name"=value lines
// with "text", dword:, hex: and hex(N): values and '\' line continuations. false when it is not a .reg file.
bool fromRegFile(const std::string& text, const std::string& root, RegCopy& out);
// The roots a file of the game's settings may have (spec §8): the personal copy and the common one, with and without
// WOW6432Node - a file from another PC, or exported with regedit.
const std::vector<std::string>& gameKeyRoots();
// The keys of a settings file under the first of `roots` it holds them for; false when it holds none.
bool fromRegFileAny(const std::string& text, const std::vector<std::string>& roots, RegCopy& out);

}  // namespace uagfx::settings
