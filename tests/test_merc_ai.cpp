// 3.0.11-beta.217 (the owner hiring a mercenary: «AI.lua error», «cannot read ./AI/AI_M.lua», then «attempt to call a
// nil value»): uaRO.exe runs the mercenary's AI from ./AI/AI_M.lua (/merai: ./AI/USER_AI/AI_M.lua), calling its global
// AI every tick; the uaRO client has no such file and the server leads mercenaries itself - an AI doing nothing is
// written where the file is missing or defines no AI; a file with its own AI stays as it is
#include <string>

#include "core/merc_ai.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(merc_ai_file_written_only_when_it_has_no_ai) {
    CHECK(mercai::needsOurs(""));                                   // empty: «attempt to call a nil value»
    CHECK(mercai::needsOurs("-- nothing here\nlocal x = 1\n"));
    CHECK(!mercai::needsOurs("require \"./AI/Const.lua\"\nfunction AI(myid)\n  Move(myid, 1, 1)\nend\n"));
    CHECK(!mercai::needsOurs("function AI (myid) end"));
    CHECK(!mercai::needsOurs("AI = function(myid) end"));
    CHECK(mercai::needsOurs("function AIx(myid) end"));
    const std::string ours = mercai::text();
    CHECK(!mercai::needsOurs(ours));
    CHECK(ours.find("function AI(myid)") != std::string::npos && ours.find("end") != std::string::npos);
    CHECK(ours.find("Move(") == std::string::npos);  // it orders nothing
}
