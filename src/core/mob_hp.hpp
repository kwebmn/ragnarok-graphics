#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/config.hpp"

namespace uagfx::mobhp {

// 3.0.11-beta.95 (the owner: «Полоски HP мобов - всем показывай»; the concept «Контроллер в интерфейсе uaRO»,
// «Полоски HP монстров»): the monsters' HP bars over their heads from their name lines' per cent (0x0195, which the
// server sends to everyone near on each hit, heal and spawn). Shown: the target's; the wounded's for kWoundedMs after
// their last change; every one known - by [interface] mob_hp. Out of sight (0x0080) a per cent is forgotten: the
// server sends none on the way back, and no bar is better than a stale one.
constexpr uint32_t kWoundedMs = 8000;

struct Shown {
    uint32_t id = 0;
    int hp = 0;  // per cent
    bool target = false;
};

class Table {
public:
    void seen(uint32_t id, int hp, uint32_t nowMs);  // a name line's per cent
    void gone(uint32_t id);                           // out of sight, or dead
    void clear();                                     // a new map
    int hp(uint32_t id) const;                        // -1 not known
    std::vector<Shown> shown(MobHp mode, uint32_t target, uint32_t nowMs) const;  // the target first
    size_t size() const { return known_.size(); }

private:
    struct Known {
        int hp = 0;
        uint32_t ms = 0;  // its last change
    };
    std::unordered_map<uint32_t, Known> known_;
};

// 3.0.11-beta.100 (the owner: «а можно чтобы над мобами всегда хп висело?»): «Усі» asks the server for the per cent of
// each monster on the screen it does not know (the game's own name request, as its mouse asks on a hover; the answer is
// the same name line) - at most kAsksPerSecond a second, the same one again after kAskAgainMs
constexpr int kAsksPerSecond = 8;
constexpr uint32_t kAskAgainMs = 15000;
class Asker {
public:
    // Of the monsters on the screen (the nearest first) those to ask now
    std::vector<uint32_t> due(const std::vector<uint32_t>& onScreen, const Table& t, uint32_t nowMs);
    void forget(uint32_t id) { asked_.erase(id); }  // out of sight: asked again when it is back
    void clear() { asked_.clear(); }

private:
    std::unordered_map<uint32_t, uint32_t> asked_;  // when each was asked
    uint32_t secondAt_ = 0;
    int inSecond_ = 0;
};

// 3.0.11-beta.99 (the owner: «hp для мобов должно быть доступно и в обычном режиме»): whose bar is the target's -
// played with the controller (`pad`) its target, else the one the character fights; with the mouse the monster under
// the cursor while its per cent is known, else the one the character fights
uint32_t pointed(const Table& t, bool pad, uint32_t padTarget, uint32_t underCursor, uint32_t fighting);

uint32_t fillColour(int hp);  // ARGB: green, red below a quarter

// 3.0.13-beta.3 (the owner's pick 10 of «50 идей»: «Строка HP у боссов и мини-боссов шире и своего цвета»): [interface]
// boss_bars - an MVP's bar (a boss with an MVP's reward) wider and gold, another boss's wider and violet; red below a
// quarter all the same
enum class Rank : uint8_t { Normal, MiniBoss, Mvp };
Rank rankOf(bool boss, bool mvpReward);
uint32_t fillColour(int hp, Rank rank);

// The bar over a sprite's box (left, top, right, bottom in the game's pixels, the game `screenHeight` pixels high)
struct Bar {
    float x = 0, y = 0, w = 0, h = 0;
};
Bar barOver(const float box[4], bool target, int screenHeight);
// 3.0.13-beta.3: a boss's bar - wider and taller about the same middle, its bottom where it was
Bar ranked(Bar b, Rank rank);
// 3.0.11-beta.148 (the owner: «хп над мобом дергается когда тот ходит», «когда моба атакую, хп дергается туда сюда»):
// a bar steady over its monster - across the middle of its feet on the screen (its place in the world as the game
// projects it), at its tallest frame's height of late: the sprite's box (the bar's place before) changes with each
// frame of a walk, an attack or a hit, and the bar jumped with it. A taller frame raises it at once; a lower height
// (a zoom out) is taken only after kHoldMs without the old one, gliding there over about kSettleMs
constexpr uint32_t kHoldMs = 1500, kSettleMs = 300;
struct Tall {
    float px = 0;         // the height kept: its top's pixels over the feet
    uint32_t metMs = 0;   // when a frame last reached it
    uint32_t fedMs = 0;   // the last frame's time (0 none yet)
};
void keepTall(Tall& t, float now, uint32_t nowMs);
Bar barOverFeet(float feetX, float feetY, float tall, bool target, int screenHeight);
std::string percentText(int hp);  // «57%»

}  // namespace uagfx::mobhp
