#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace uagfx::party {

// 3.0.7-beta.6 (the user's notification 6): the party's members by account - its list (0x00fb), one joining
// (0x01e9), one gone (0x0105) - and their HP as the server sends it to those near (0x0106; the character's own it
// never sends): a death, and a fall under the threshold, told once each - again once the member is up (a death) or
// kRearm per cent over the threshold (low HP)
struct News {
    enum Kind { Dead, Low } kind = Dead;
    std::string name;  // in the game's code page, as it came
    int percent = 0;   // HP left (Low)
};

class Watch {
public:
    static constexpr int kRearm = 10;
    void members(const std::vector<std::pair<uint32_t, std::string>>& list, uint32_t own);  // the whole list anew
    void joined(uint32_t account, const std::string& name);
    void left(uint32_t account);
    void hp(uint32_t account, uint16_t hp, uint16_t most, int threshold, std::vector<News>& out);
    void died(uint32_t account, std::vector<News>& out);  // its unit seen dying (0x0080)

private:
    struct Member {
        std::string name;
        bool dead = false, low = false;
    };
    std::map<uint32_t, Member> members_;
    uint32_t own_ = 0;
};

}  // namespace uagfx::party
