#include "app/rebuilds.hpp"

namespace uagfx::app {

bool Rebuilds::request() {
    if (building_) return false;  // the rebuild under way serves it
    if (boxes_ > 0) {
        waiting_ = true;
        return false;
    }
    if (posted_) return false;  // the message on its way serves it
    posted_ = true;
    return true;
}

bool Rebuilds::arrived() {
    if (!posted_) return false;  // a rebuild since it was posted served it
    posted_ = false;
    if (boxes_ > 0) {  // the box's own loop took the message
        waiting_ = true;
        return false;
    }
    return true;
}

void Rebuilds::begin() {
    building_ = true;
    posted_ = false;
    waiting_ = false;
}

void Rebuilds::end() { building_ = false; }

void Rebuilds::boxOpened() { ++boxes_; }

bool Rebuilds::boxClosed() {
    if (boxes_ > 0) --boxes_;
    if (boxes_ > 0 || !waiting_) return false;
    waiting_ = false;
    posted_ = true;
    return true;
}

}  // namespace uagfx::app
