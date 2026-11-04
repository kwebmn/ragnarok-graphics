#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace uagfx::expgain {

// 3.0.11-beta.237 (the owner: «когда убиваешь моба там где полоски временно показывалось на сколько добавилось опыта и
// над мобом было типа + столько то опыта»): the experience a kill gave - over the monster that died, rising and fading,
// and by the bars at the screen's bottom for a while. The server sends the character's experience whole (0x00b1, type
// 1 base, 2 job) and the monster's death (0x0080 type 1) in either order: a gain and a kill of the character's (a
// monster it hit) close in time are one; a gain with no kill (the party's share, a quest) shows by the bars only.
constexpr uint64_t kPairMs = 1500;   // a kill and a gain this close are one
constexpr uint64_t kGainWaitMs = 400;  // a gain waits for its kill this long before it shows alone
constexpr uint64_t kFloatMs = 1600, kFadeMs = 600;  // over the monster: rising this long, fading its last part
constexpr uint64_t kLabelMs = 3000;  // by the bars
constexpr float kRise = 36;          // game pixels risen over kFloatMs

struct Floater {
    float x = 0, y = 0;  // where it started: over the monster's head, the game's pixels
    long long base = 0, job = 0;
    uint64_t ms = 0;
};
struct Label {
    long long base = 0, job = 0;
    uint64_t ms = 0;  // 0 none
};

class Tracker {
public:
    void hit(uint32_t target);                           // the character hit it
    bool died(uint32_t id);                              // true: one it hit (its place next, by killedAt)
    void killedAt(uint32_t id, float x, float y, uint64_t ms);  // where it stood dying (the game's pixels)
    void experience(int type, uint32_t value, uint64_t ms);    // 0x00b1 1 base, 2 job: the whole, as sent
    void step(uint64_t ms);  // pairs the gains with the kills; the floaters and the label from them
    const std::vector<Floater>& floaters() const { return floaters_; }
    const Label& label() const { return label_; }
    void reset();  // another character

private:
    struct Kill {
        float x = 0, y = 0;
        uint64_t ms = 0;
    };
    struct Gain {
        long long base = 0, job = 0;
        uint64_t ms = 0;
    };
    std::map<uint32_t, bool> hit_;
    std::vector<Kill> kills_;
    Gain gain_;  // gathering (a kill's base and job come as two packets)
    bool known_[3] = {};
    uint32_t last_[3] = {};
    std::vector<Floater> floaters_;
    Label label_;
    void show(const Gain& g, const Kill* k);
};

// Rising and fading: its offset up (game pixels) and its opacity at `ms`; opacity 0 gone
float rise(const Floater& f, uint64_t ms);
float opacity(uint64_t at, uint64_t ms, uint64_t whole, uint64_t fade);
// «+1 234 Base · +567 Job» (a part without a gain left out); `share` with each one's part of its level («(0,05 %)»)
std::string text(long long base, long long job, char decimal, double baseOfLevel = -1, double jobOfLevel = -1);

}  // namespace uagfx::expgain
