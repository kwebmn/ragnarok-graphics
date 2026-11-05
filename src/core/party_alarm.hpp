#pragma once
#include <cstdint>
#include <map>

namespace uagfx::partyalarm {

// 3.0.11-beta.282 (the owner's feature 18, the healer's alarm): one of the party on the character's map falling under
// kLow per cent of HP (from kLow or more) - a short chime once and its row pulsing red kPulseMs; the character's own
// HP not counted (the game shows it). A member just seen (or come back here) tells nothing until its next fall.
constexpr int kLow = 30;
constexpr uint64_t kPulseMs = 3000;
class Watch {
public:
    // percents: the members here now, account -> HP per cent (< 0 not known). True: one fell under kLow just now
    bool step(const std::map<uint32_t, int>& percents, uint64_t now);
    float pulse(uint32_t account, uint64_t now) const;  // 0..1 its row's red, waving; 0 none

private:
    std::map<uint32_t, int> last_;
    std::map<uint32_t, uint64_t> fell_;
};

}  // namespace uagfx::partyalarm
