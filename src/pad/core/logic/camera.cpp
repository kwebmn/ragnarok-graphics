#include "pad/core/logic/camera.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace uapad {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr size_t kKeep = 8;
constexpr double kTrim = 30.0 * kPi / 180.0;
constexpr double kMinScreenPx = 10.0;
constexpr int kMinCells = 2;
constexpr size_t kKeepPairs = 8;
constexpr double kSpread = 30.0 * kPi / 180.0;  // the client's turns the samples were taken at differ this much
constexpr double kFits = 15.0 * kPi / 180.0;    // every sample within this of the mapping
constexpr double kMisfits = 30.0 * kPi / 180.0; // and the other sign off by more

double wrap(double a) { return std::remainder(a, 2.0 * kPi); }  // to [-pi, pi]

}  // namespace

void CameraYaw::addSample(int sx, int sy, int wx, int wy) {
    if (std::hypot(double(sx), double(sy)) < kMinScreenPx || std::max(std::abs(wx), std::abs(wy)) < kMinCells) return;
    const double diff = wrap(std::atan2(double(wy), double(wx)) - std::atan2(-double(sy), double(sx)));
    diffs_.push_back(diff);
    if (diffs_.size() > kKeep) diffs_.pop_front();
    if (client_ && !calibrated_) {
        pairs_.emplace_back(diff, *client_);
        if (pairs_.size() > kKeepPairs) pairs_.pop_front();
        calibrate();
    }
    double s = 0, c = 0;
    for (double d : diffs_) {
        s += std::sin(d);
        c += std::cos(d);
    }
    const double all = std::atan2(s, c);
    // Drop samples far from the mean (a cursor that had already moved on, a camera mid-turn) and average again.
    double ks = 0, kc = 0;
    int kept = 0;
    for (double d : diffs_) {
        if (std::fabs(wrap(d - all)) > kTrim) continue;
        ks += std::sin(d);
        kc += std::cos(d);
        ++kept;
    }
    yaw_ = kept ? std::atan2(ks, kc) : all;
}

// diff = sign * client + offset, for the sign whose offsets agree while the other sign's do not.
void CameraYaw::calibrate() {
    double hi = 0;
    for (size_t i = 0; i < pairs_.size(); ++i)
        for (size_t j = i + 1; j < pairs_.size(); ++j)
            hi = std::max(hi, std::fabs(wrap(pairs_[i].second - pairs_[j].second)));
    if (hi < kSpread) return;
    double worst[2] = {0, 0};
    double mean[2] = {0, 0};
    for (int k = 0; k < 2; ++k) {
        const int sign = k == 0 ? 1 : -1;
        double s = 0, c = 0;
        for (const auto& p : pairs_) {
            const double o = wrap(p.first - sign * p.second);
            s += std::sin(o);
            c += std::cos(o);
        }
        mean[k] = std::atan2(s, c);
        for (const auto& p : pairs_)
            worst[k] = std::max(worst[k], std::fabs(wrap(p.first - sign * p.second - mean[k])));
    }
    for (int k = 0; k < 2; ++k) {
        if (worst[k] < kFits && worst[1 - k] > kMisfits) {
            calibrated_ = true;
            sign_ = k == 0 ? 1 : -1;
            offset_ = mean[k];
            return;
        }
    }
}

double CameraYaw::radians() const {
    if (calibrated_ && client_) return wrap(sign_ * *client_ + offset_);
    return yaw_;
}

void CameraYaw::toWorld(double sx, double sy, double& wx, double& wy) const {
    const double a = std::atan2(-sy, sx) + radians();
    wx = std::cos(a);
    wy = std::sin(a);
}

}  // namespace uapad
