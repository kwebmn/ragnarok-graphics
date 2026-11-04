#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::guildtabs {

// 3.0.11-beta.220 (the owner «1-3 делай»: the guild's tabs 2-5 in the new look): what those tabs show, read from the
// server's packets as the game asks for them on a tab's switch (0x014f) - the positions (0x0166 their names, 0x0160
// their rights: 0x01 invite, 0x10 expel, and tax), the guild's skills (0x0162: the points left, each one's id, level,
// whether it can rise), the expelled (0x0163: name, account, reason, 88 bytes each) and the notice (0x016f: subject 60,
// body 120). Names in the game's code page.
struct Position {
    int id = 0;
    std::string name;
    bool invite = false, expel = false;
    int tax = 0;
};
struct Skill {
    int id = 0, lv = 0;
    bool up = false;
};
struct Expelled {
    std::string name, account, reason;
};
struct Data {
    std::vector<Position> positions;
    std::vector<Skill> skills;
    int points = 0;
    std::vector<Expelled> expelled;
    std::string subject, body;
};
// One of those packets taken in; false for another
bool read(Data& d, const uint8_t* packet, size_t size);
std::string skillName(int id);  // the guild's skills (10000-10014) as the server's skill_db names them; "" else

}  // namespace uagfx::guildtabs
