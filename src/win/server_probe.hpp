#pragma once
#include <string>

#include "core/server_status.hpp"

namespace uagfx::win::serverprobe {

// 3.0.11-beta.173 (the login concept: the server's state under the window): the login server reached or not - a
// thread of its own connects to it (TCP, nothing sent) every srvstatus::kProbeSec seconds while the login screens
// ask for it, and times that. ws2_32's calls taken from the game's own copy (the driver imports none of it).
struct Result {
    srvstatus::State state = srvstatus::State::Unknown;
    int pingMs = -1;
};
// Called each frame while the login screens show: the server to try (the game's chosen connection)
void want(const std::string& host, int port);
Result result();

}  // namespace uagfx::win::serverprobe
