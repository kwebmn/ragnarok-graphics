#pragma once
#include <string_view>

// 3.0.18-beta.171 (R-UT4N5V: the chat's part 0.7 ms a frame from the start, its rows no longer the cause): a module's
// own marks inside its part of a present - core/part_clock via win/context; the log's «the driver's parts» then
// splits that part ("chat layout", "chat rows", ...). `name` a literal: kept as it is.
namespace uagfx::win {

void partMark(std::string_view name);

}  // namespace uagfx::win
