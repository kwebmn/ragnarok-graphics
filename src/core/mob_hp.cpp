#include "core/mob_hp.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace uagfx::mobhp {

void Table::seen(uint32_t id, int hp, uint32_t nowMs) {
    if (!id || hp < 0 || hp > 100) return;
    auto it = known_.find(id);
    if (it == known_.end())
        known_.emplace(id, Known{hp, nowMs});
    else if (it->second.hp != hp)
        it->second = Known{hp, nowMs};
}

void Table::gone(uint32_t id) { known_.erase(id); }

void Table::clear() { known_.clear(); }

int Table::hp(uint32_t id) const {
    const auto it = known_.find(id);
    return it == known_.end() ? -1 : it->second.hp;
}

std::vector<Shown> Table::shown(MobHp mode, uint32_t target, uint32_t nowMs) const {
    std::vector<Shown> out;
    if (mode == MobHp::Off) return out;
    const auto t = target ? known_.find(target) : known_.end();
    if (t != known_.end()) out.push_back(Shown{target, t->second.hp, true});
    if (mode == MobHp::Target) return out;
    const size_t first = out.size();
    for (const auto& [id, k] : known_) {
        if (id == target) continue;
        if (mode == MobHp::All || (k.hp < 100 && nowMs - k.ms < kWoundedMs)) out.push_back(Shown{id, k.hp, false});
    }
    std::sort(out.begin() + std::ptrdiff_t(first), out.end(),
              [](const Shown& a, const Shown& b) { return a.id < b.id; });  // a steady order between frames
    return out;
}

std::vector<uint32_t> Asker::due(const std::vector<uint32_t>& onScreen, const Table& t, uint32_t nowMs) {
    std::vector<uint32_t> out;
    if (nowMs - secondAt_ >= 1000) {
        secondAt_ = nowMs;
        inSecond_ = 0;
    }
    for (const uint32_t id : onScreen) {
        if (inSecond_ >= kAsksPerSecond) break;
        if (!id || t.hp(id) >= 0) continue;
        const auto it = asked_.find(id);
        if (it != asked_.end() && nowMs - it->second < kAskAgainMs) continue;
        asked_[id] = nowMs;
        ++inSecond_;
        out.push_back(id);
    }
    if (asked_.size() > 4096)  // the long gone ones
        for (auto it = asked_.begin(); it != asked_.end();)
            it = nowMs - it->second >= kAskAgainMs ? asked_.erase(it) : std::next(it);
    return out;
}

uint32_t pointed(const Table& t, bool pad, uint32_t padTarget, uint32_t underCursor, uint32_t fighting) {
    if (pad) return padTarget ? padTarget : fighting;
    return underCursor && t.hp(underCursor) >= 0 ? underCursor : fighting;
}

uint32_t fillColour(int hp) { return hp < 25 ? 0xFFE5534Bu : 0xFF5BD36Cu; }

Rank rankOf(bool boss, bool mvpReward) { return !boss ? Rank::Normal : mvpReward ? Rank::Mvp : Rank::MiniBoss; }

uint32_t fillColour(int hp, Rank rank) {
    if (rank == Rank::Normal || hp < 25) return fillColour(hp);
    return rank == Rank::Mvp ? 0xFFF2C14Eu : 0xFFB07CFFu;  // the interface's gold; a violet
}

Bar ranked(Bar b, Rank rank) {
    if (rank == Rank::Normal) return b;
    const float wide = rank == Rank::Mvp ? 1.7f : 1.35f, tall = rank == Rank::Mvp ? 1.6f : 1.3f;
    const float mid = b.x + b.w / 2, bottom = b.y + b.h;
    b.w *= wide;
    b.h *= tall;
    b.x = mid - b.w / 2;
    b.y = bottom - b.h;
    return b;
}

Bar barOver(const float box[4], bool target, int screenHeight) {
    const float k = std::max(0.6f, float(screenHeight) / 1080.0f);
    Bar b;
    b.w = (target ? 54.0f : 38.0f) * k;
    b.h = (target ? 5.0f : 4.0f) * k;
    b.x = (box[0] + box[2]) / 2 - b.w / 2;
    b.y = box[1] - 5.0f * k - b.h;
    return b;
}

void keepTall(Tall& t, float now, uint32_t nowMs) {
    constexpr float kMet = 0.5f;  // pixels: a frame as tall as the kept height, near enough
    if (!t.fedMs || now >= t.px - kMet) {
        t.px = t.fedMs ? std::max(t.px, now) : now;
        t.metMs = nowMs;
    } else if (nowMs - t.metMs >= kHoldMs) {
        t.px = now + (t.px - now) * float(std::exp(-double(nowMs - t.fedMs) / double(kSettleMs)));
    }
    t.fedMs = nowMs ? nowMs : 1;
}

Bar barOverFeet(float feetX, float feetY, float tall, bool target, int screenHeight) {
    const float box[4] = {feetX, feetY - tall, feetX, feetY};
    return barOver(box, target, screenHeight);
}

std::string percentText(int hp) { return std::to_string(hp) + "%"; }

}  // namespace uagfx::mobhp
