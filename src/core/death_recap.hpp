#pragma once
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace uagfx::deathrecap {

// 3.0.11-beta.277 (the owner's feature 8: «что меня убило»): the last hits on the character - who, by which skill (0 an
// attack), how much, when - kept 10 s back, 10 at most; at its death the death screen lists them, the last first
struct Hit {
    uint32_t source = 0;
    std::string who;  // its name (UTF-8) as known when it hit; "" not known
    int skill = 0, damage = 0;
    uint64_t at = 0;  // ms
};
constexpr uint64_t kKeepMs = 10000;
constexpr size_t kMost = 10;

class Log {
public:
    void hit(const Hit& h);
    // The hits before `deathAt` (kKeepMs back), the last first
    std::vector<Hit> before(uint64_t deathAt) const;
    void clear() { hits_.clear(); }

private:
    std::deque<Hit> hits_;
};

struct Texts {
    std::string attack, seconds;  // «удар», «{1} с тому»
};
// «Orc Warrior · Bash · 523 · 1,2 с тому»; `skillName` the skill's words ("" - its number)
std::string line(const Hit& h, uint64_t deathAt, const std::string& skillName, const Texts& t, char decimal);
// The sum of the damage of the hits listed
long long total(const std::vector<Hit>& hits);

}  // namespace uagfx::deathrecap
