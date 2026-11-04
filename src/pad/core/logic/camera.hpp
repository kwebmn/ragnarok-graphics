#pragma once
#include <cstddef>
#include <deque>
#include <optional>
#include <utility>

namespace uapad {

// Which way the game camera is turned, learned from the client's own walk requests: the cursor sat
// (sx, sy) px from the centre of the client area and the client asked to walk to a cell (wx, wy) away
// from us. Until a sample arrives the camera counts as not turned: north up, as the client starts.
class CameraYaw {
public:
    // sx, sy: screen offset, +y down. wx, wy: map offset in cells, +y north.
    void addSample(int sx, int sy, int wx, int wy);
    void reset() {
        diffs_.clear();
        yaw_ = 0;
        pairs_.clear();
    }
    // The client's own turn of the camera (read from its memory), radians as the client counts them. Which way
    // and from where it counts is learned from the walk samples taken at two turns at least 30 degrees apart;
    // from then on the directions follow the client's turn at once, without walking.
    void setClientTurn(double radians) { client_ = radians; }
    // 3.0.11-beta.215: the camera's look along the ground (east, north; any length) as the game's view has it - the
    // turn itself, over anything learned
    void setLook(double east, double north) { look_ = std::make_pair(east, north); }
    void clearLook() { look_.reset(); }
    bool hasLook() const { return look_.has_value(); }
    void clearClientTurn() { client_.reset(); }
    bool calibrated() const { return calibrated_; }
    int calibratedSign() const { return sign_; }
    double calibratedOffset() const { return offset_; }
    // Map bearing minus screen bearing (both counter-clockwise from east / from the right), radians.
    double radians() const;
    // Unit map direction (+y north) for a non-zero screen direction (+y down).
    void toWorld(double sx, double sy, double& wx, double& wy) const;
    size_t samples() const { return diffs_.size(); }

private:
    void calibrate();

    std::deque<double> diffs_;  // map minus screen bearing of the latest samples
    double yaw_ = 0;
    std::optional<double> client_;
    std::optional<std::pair<double, double>> look_;
    std::deque<std::pair<double, double>> pairs_;  // a sample's bearing difference and the client's turn then
    bool calibrated_ = false;
    int sign_ = 1;
    double offset_ = 0;
};

}  // namespace uapad
