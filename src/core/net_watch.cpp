#include "core/net_watch.hpp"

#include <algorithm>
#include <cstdio>

namespace uagfx::netwatch {
namespace {

constexpr uint64_t kPeriodMs = 60000, kResentMs = 10000, kStallMs = 5000;
constexpr uint32_t kFairUs = 120000, kBadUs = 250000;
constexpr int kHeldMs = 250;               // 3.0.11-beta.128: a server's packet this late was held
constexpr uint64_t kHoldGapMs = 1000;      // late ones within it: the same hold

}  // namespace

void Watch::tcp(const Tcp& t, uint64_t nowMs) {
    // 3.0.11-beta.127: the graph's reading - red after a time-out of ours or (3.0.11-beta.128) a hold of the server's
    const bool stall = (known_ && t.timeouts > last_.timeouts) || (holdAt_ && holdAt_ > readAt_);
    readAt_ = nowMs;
    graph_[graphNext_] = stall ? -1 : int(t.rttUs / 1000);
    graphNext_ = (graphNext_ + 1) % kGraph;
    graphCount_ = std::min(graphCount_ + 1, kGraph);
    if (!known_) {  // the first: the baseline (what was sent again before it is not this period's)
        known_ = true;
        periodAt_ = nowMs;
    } else {
        if (t.bytesRetrans > last_.bytesRetrans) {
            resent_ += t.bytesRetrans - last_.bytesRetrans;
            resentAt_ = nowMs;
        }
        if (t.timeouts > last_.timeouts) {
            timeouts_ += t.timeouts - last_.timeouts;
            timeoutAt_ = nowMs;
            stallTold_ = false;
        }
    }
    last_ = t;
    rttSum_ += t.rttUs;
    ++rttCount_;
    rttMost_ = std::max(rttMost_, t.rttUs);
}

void Watch::packet(uint64_t nowMs) {
    if (packetAt_ && nowMs > packetAt_) longestGap_ = std::max(longestGap_, nowMs - packetAt_);
    packetAt_ = nowMs;
}

void Watch::fix(bool own, int cells, uint64_t nowMs) {
    (void)nowMs;
    ++fixes_;
    if (own) ++ownFixes_;
    fixMost_ = std::max(fixMost_, cells);
}

Level Watch::level(uint64_t nowMs) const {
    if (!known_) return Level::Unknown;
    if (timeoutAt_ && nowMs - timeoutAt_ < kStallMs) return Level::Stall;
    if (holdAt_ && nowMs - holdAt_ < kStallMs) return Level::Stall;  // 3.0.11-beta.128
    if ((resentAt_ && nowMs - resentAt_ < kResentMs) || last_.rttUs >= kBadUs) return Level::Bad;
    return last_.rttUs >= kFairUs ? Level::Fair : Level::Good;
}

void Watch::late(int ms, uint64_t nowMs) {
    if (ms < kHeldMs) return;
    if (!lateAt_ || nowMs - lateAt_ > kHoldGapMs) {  // a new hold
        ++holds_;
        holdAt_ = nowMs ? nowMs : 1;
        holdMs_ = 0;
        holdTold_ = false;
    }
    lateAt_ = nowMs;
    holdMs_ = std::max(holdMs_, ms);
    holdMost_ = std::max(holdMost_, ms);
}

bool Watch::held() {
    if (holdTold_) return false;
    holdTold_ = true;
    return true;
}

std::vector<int> Watch::graph() const {
    std::vector<int> out;
    out.reserve(size_t(graphCount_));
    for (int i = 0; i < graphCount_; ++i) out.push_back(graph_[(graphNext_ - graphCount_ + i + kGraph) % kGraph]);
    return out;
}

bool Watch::stalled() {
    if (stallTold_) return false;
    stallTold_ = true;
    return true;
}

bool Watch::due(uint64_t nowMs) const { return known_ && nowMs - periodAt_ >= kPeriodMs; }

std::string Watch::line(uint64_t nowMs) {
    const uint32_t average = rttCount_ ? uint32_t(rttSum_ / rttCount_) : last_.rttUs;
    const uint32_t most = rttCount_ ? rttMost_ : last_.rttUs;
    char text[448];
    std::snprintf(text, sizeof text,
                  "the map server - round trip %u ms (at most %u, least %u), %llu bytes sent again, %u time-out%s; "
                  "the longest pause between its packets %.1f s; %d position fix%s (yours %d), %d cells at most; "
                  "%d hold%s by the server's lost packets (at most %d ms)",
                  average / 1000, most / 1000, last_.minRttUs / 1000, static_cast<unsigned long long>(resent_),
                  timeouts_, timeouts_ == 1 ? "" : "s", double(longestGap_) / 1000.0, fixes_,
                  fixes_ == 1 ? "" : "es", ownFixes_, fixMost_, holds_, holds_ == 1 ? "" : "s", holdMost_);
    periodAt_ = nowMs;  // a new period
    rttSum_ = 0;
    rttCount_ = 0;
    rttMost_ = 0;
    resent_ = 0;
    timeouts_ = 0;
    longestGap_ = 0;
    fixes_ = ownFixes_ = fixMost_ = 0;
    holds_ = holdMost_ = 0;
    return text;
}

}  // namespace uagfx::netwatch
