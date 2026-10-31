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

// 3.0.11-beta.99 (the owner: «hp для мобов должно быть доступно и в обычном режиме»): whose bar is the target's -
// played with the controller (`pad`) its target, else the one the character fights; with the mouse the monster under
// the cursor while its per cent is known, else the one the character fights
uint32_t pointed(const Table& t, bool pad, uint32_t padTarget, uint32_t underCursor, uint32_t fighting);

uint32_t fillColour(int hp);  // ARGB: green, red below a quarter

// The bar over a sprite's box (left, top, right, bottom in the game's pixels, the game `screenHeight` pixels high)
struct Bar {
    float x = 0, y = 0, w = 0, h = 0;
};
Bar barOver(const float box[4], bool target, int screenHeight);
std::string percentText(int hp);  // «57%»

}  // namespace uagfx::mobhp
