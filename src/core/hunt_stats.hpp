#pragma once
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace uagfx::hunt {

// 3.0.11-beta.278 (the owner's features 6 and 10): the hunt's numbers since it began (or «↺») - the character's damage
// a second over the last 10 s and the last minute and its skills' shares, the experience a level's per cents an hour,
// the time to the next level at that pace, the zeny an hour, the kills by monster
struct View {
    double dps10 = 0, dps60 = 0;
    std::vector<std::pair<int, int>> skills;  // skill (0 an attack) and its share of the last minute's damage, %
    double levelPerHour = 0;                  // per cent of a level
    long long secondsToLevel = -1;            // -1 not known
    long long zenyPerHour = 0;
    long long seconds = 0;  // since it began
    int kills = 0;
    std::vector<std::pair<std::string, int>> killed;  // the most killed first
    // 3.0.13-beta.14 (the owner's pick 46 of «50 идей»: «Урон каждого в группе»): each one's share of the damage the
    // party dealt since it began (the character's own named ""), the most first; empty without a party's hit
    std::vector<std::pair<std::string, int>> party;
};

class Stats {
public:
    void reset(uint64_t now);
    void hit(int damage, int skill, uint64_t now);  // the character's
    void partyHit(uint32_t account, const std::string& name, int damage, uint64_t now);  // 3.0.13-beta.14: a mate's
    void status(int level, long long exp, long long next, long long zeny, uint64_t now);
    void kill(const std::string& name);
    View view(uint64_t now) const;
    bool started() const { return began_ != 0; }

private:
    struct Dealt {
        uint64_t at;
        int damage, skill;
    };
    uint64_t began_ = 0;
    std::deque<Dealt> dealt_;  // the last minute's
    int level_ = 0;
    long long exp_ = 0, next_ = 0, zeny_ = 0, zenyFrom_ = 0;
    double gained_ = 0;  // per cent of a level since it began
    bool known_ = false;
    std::map<std::string, int> killed_;
    long long own_ = 0;                                          // 3.0.13-beta.14: all the character dealt
    std::map<uint32_t, std::pair<std::string, long long>> party_;  // and each mate
    int kills_ = 0;
};

struct Texts {
    std::string title, dps, attack, exp, toLevel, zeny, kills, none;
    std::string party, me;  // 3.0.13-beta.14: «Група: {1}», «ви»
    // «Полювання · {1}», «DPS {1} · за хв {2}», «удар», «Досвід {1} %/год», «до рівня {1}», «Зені {1}/год»,
    // «Убито {1}: {2}», «—»
};
std::string duration(long long seconds);  // «1:05:20», «12:34»
std::string grouped(long long v);         // «45 000», «−1 200»
// The panel's lines, top down, the title first; `skillName` a skill's words by its number
std::vector<std::string> lines(const View& v, const Texts& t, const std::vector<std::string>& skillNames);

}  // namespace uagfx::hunt
