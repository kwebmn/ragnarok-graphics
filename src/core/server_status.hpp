#pragma once
#include <cstdint>
#include <string>

#include "core/settings/strings.hpp"

namespace uagfx::srvstatus {

// 3.0.11-beta.173 (the login concept: the server's state under the window): what the login screen's status line
// says - the login server reached (its ping) and the players online (the watcher's online.json), or not answering
// with the next try's countdown; a login the server could not be reached for is tried again by itself only after the
// player pressed «Увійти», every kProbeSec seconds (no storm of the game's «Failed to Connect to Server.» boxes)
enum class State : uint8_t { Unknown, Up, Down };
constexpr int kProbeSec = 8;
struct Line {
    std::string text;  // UTF-8; "" nothing to say yet
    uint32_t rgb = 0;  // the dot's colour
};
// pingMs < 0: not measured; online < 0: not known; retryIn: seconds to the next try (0: none waiting)
Line line(State s, int pingMs, int online, int retryIn, settings::Lang lang);
bool retryLogin(bool lastFailedToConnect, bool playerPressed, int secsSinceFail);

}  // namespace uagfx::srvstatus
