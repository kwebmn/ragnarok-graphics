#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "core/reference_index.hpp"
#include "core/settings/strings.hpp"

// 3.0.22-beta.4 (the owner's «ок» to idea 3; R-JGEFVM: «в тейквон місію назначило реневал моба, якого немає на
// сервері - Solid Antique Book (C2_ANTIQUE_BOOK) Mob-ID#2898»): the Taekwon mission's monster - the server's 0x020E
// (clif_mission_info: <name>.24B <monster>.L <progress>.B <kind>.B, kind 20) - told in the chat with the maps it lives
// on (the reference's spawns), or that it lives nowhere here: the server picks it among the random monsters
// (mob_get_random_id) and keeps it, so such a mission cannot be done.
namespace uagfx::tkmission {

constexpr uint16_t kPacketId = 0x020E;
constexpr size_t kPacketLen = 32;
constexpr uint8_t kMissionKind = 20;  // the others: the Star Gladiator's places and monsters
constexpr size_t kMostMaps = 4;

struct Mission {
    int mob = 0, progress = 0;
    std::string name;  // as the server names it (its code page)
};
bool parse(const uint8_t* p, size_t n, Mission& out);
// «Місія таеквона: Poring — водиться на prt_fild08 (75), pay_fild01 (40).»; "" with no reference
std::string line(const Mission& m, const refindex::Index* index, settings::Lang l);

}  // namespace uagfx::tkmission
