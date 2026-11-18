#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "core/net_trail.hpp"

namespace uagfx::win::nettrailwin {

// 3.0.20-beta.60 (the owner: «есть какой-то глюк, в котором клиент выбрасывает с дисконнектом, надо его отловить ...
// отправлять отчёт на сервер»): the trail of the game's connections (core/net_trail.hpp) kept as uaRO.exe works - its
// connection's connects (the packet tap's call sites: which server each is for), its reads and writes (the two
// functions it takes from ws2_32.dll by their names, kept at 0x6EE66C and 0x6EE6E4: ours in their place once the
// game has them), every closesocket (its import, by who called), the packets it takes in both of its modes' loops
// and each loop's question to its connection. A connection lost - or the server's notice of it - written to
// uaro-graphics\logs\uaro-disconnect.txt and handed to the settings app, which sends it by itself
// ([report] disconnects). All on the game's thread.
void install();  // closesocket's import slot, at the DLL's load; none on another uaRO.exe
void sitesTaken(int taken, int all);  // the packet tap's call sites for it (the login mode's loop, the connects)
// the game's connect returned (FUN_00418af0): its connection's socket at +4, the address it was given
void connected(void* connection, const char* address, nettrail::Role role, bool ok);
void packet(nettrail::Mode mode, uint16_t id, const uint8_t* data, size_t readable);
void beforePoll();  // a mode's loop is about to ask its connection: the reads and writes through us from now
void polled(nettrail::Mode mode, bool fine);
// each present: [report] disconnects, the map and whether the game's window is in front - a drop told then
void frame(bool on, const std::string& map, bool foreground);
// 3.0.22-beta.3 (ideas 1, 2): the last drop told, if within `withinMs` of now - the game's «Disconnected» box says it
bool lastDrop(unsigned long long withinMs, nettrail::Drop& out);

}  // namespace uagfx::win::nettrailwin
