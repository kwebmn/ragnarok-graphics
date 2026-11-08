#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::combo {

// 3.0.16-beta.13 (the owner's pick 5 of «20 идей»: the Monk's combo window): uAthena skill.c - a skill that can be
// chained sends 0x01d2 <unit>.L <wait>.L to the area BEFORE its own damage (clif_combo_delay, then the skill's
// 0x01de): Triple Attack -> Chain Combo -> Combo Finish -> Tiger Fist / Chain Crush -> Asura (Fury on), and the
// Archer's Double Strafe -> Beast Strafing. The window: which skill opened it and how long it lasts.
std::vector<uint16_t> nextOf(uint16_t skill);  // the skills that chain after it

class Window {
public:
    static constexpr uint64_t kPairMs = 300;  // the skill's own damage this close after 0x01d2
    void opened(uint32_t waitMs, uint64_t now);
    void skillDone(uint16_t skill, uint64_t now);
    uint16_t after(uint64_t now) const;  // the skill that opened it (0 none open, or not known yet)
    float left(uint64_t now) const;      // its share left, 1 .. 0 (0 closed)

private:
    uint64_t openedAt_ = 0, until_ = 0, lastAt_ = 0;
    uint32_t total_ = 0;
    uint16_t after_ = 0, last_ = 0;
};

}  // namespace uagfx::combo
