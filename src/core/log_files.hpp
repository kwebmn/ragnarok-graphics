#pragma once
#include <cstdint>
#include <string>

namespace uagfx::logfiles {

// 2.8: a log file at most this big - then the start of the game's log is set aside (part) and a new part goes on.
constexpr uint64_t kCap = 32ull << 20;

// 3.0.3-beta.1: a game started while another runs writes logs of its own - the first one no running game writes of
// "x\a.log", "x\a-2.log" ... "x\a-<kSlots>.log" (win/log.cpp openFree)
constexpr int kSlots = 8;
std::string slotted(const std::string& path, int slot);  // slot 1 (or less): the path itself
std::string previous(const std::string& path);  // the previous game's log: "x\a.log" -> "x\a.prev.log"
std::string part(const std::string& path);      // the start of a long one: "x\a.log" -> "x\a.1.log"
std::string folderOf(const std::string& path);  // "x\y\a.log" -> "x\y"; "" with no backslash

}  // namespace uagfx::logfiles
