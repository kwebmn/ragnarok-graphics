#include "core/map_load.hpp"

namespace uagfx::map_load {
namespace {

constexpr double kShortest = 300.0;     // ms: shorter in the same map - no load
constexpr double kLongest = 120000.0;  // ms: longer - dropped

}  // namespace

Watch::Event Watch::frame(double ms, const std::string& map, bool world, bool picture, bool starting) {
    if (starting) world = false;  // 3.0.2-beta.15: a camera's matrix from before the map's start
    if (loading_) {
        if (world) {
            loading_ = false;
            loadMs_ = ms - began_;
            const bool load = loadMs_ > kShortest || map != worldMap_ || pictured_ || started_;
            hadWorld_ = true;
            worldMs_ = ms;
            worldMap_ = map;
            why_ = "";
            return load ? Event::Ended : Event::Dropped;
        }
        if (map.empty() || ms - began_ > kLongest) {
            loading_ = false;
            blocked_ = !map.empty();
            why_ = map.empty() ? "back to the character list" : "no world for two minutes";
            return Event::Dropped;
        }
        map_ = map;
        pictured_ = pictured_ || picture;
        started_ = started_ || starting;
        return Event::None;
    }
    if (world) {
        blocked_ = false;
        hadWorld_ = true;
        worldMs_ = ms;
        worldMap_ = map;
        return Event::None;
    }
    if (blocked_ || map.empty()) return Event::None;
    loading_ = true;
    pictured_ = picture;
    started_ = starting;
    began_ = ms;
    map_ = map;
    loadMs_ = 0.0;
    beforeMs_ = hadWorld_ ? ms - worldMs_ : -1.0;
    return Event::Began;
}

}  // namespace uagfx::map_load
