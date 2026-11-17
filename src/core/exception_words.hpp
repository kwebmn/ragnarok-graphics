#pragma once
#include <exception>
#include <string>

namespace uagfx {

// 3.0.21-beta.14 (R-DP7PVN: the game ended with no line in the log - a C++ exception out of the driver's frame meets
// the game's own code, which has no unwinding for it, and std::terminate ends the game): what left, for the log -
// «std::bad_alloc: std::bad_alloc», «an exception of type int», «no exception (std::terminate called by itself)»
std::string exceptionWords(std::exception_ptr e);

}  // namespace uagfx
