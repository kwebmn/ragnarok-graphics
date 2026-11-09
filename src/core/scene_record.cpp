#include "core/scene_record.hpp"

#include <cstring>

namespace uagfx::scenerecord {

void entryHead(uint32_t type, uint32_t size, uint8_t out[kEntryHead]) {
    std::memcpy(out, &type, 4);
    std::memcpy(out + 4, &size, 4);
}

bool Recorder::take(uint32_t type, size_t size, bool mapShown, uint64_t nowMs) {
    if (done_) return false;
    if (mapSeen_ && nowMs > mapAt_ + kAfterMapMs) {
        done_ = true;
        return false;
    }
    const uint64_t after = bytes_ + uint64_t(size) + kEntryHead;
    if (after > kMostBytes) {
        done_ = true;
        return false;
    }
    if (mapShown && !mapSeen_) {
        mapSeen_ = true;
        mapAt_ = nowMs;
    }
    bytes_ = after;
    ++messages_;
    if (type == kExecute) ++frames_;
    return true;
}

}  // namespace uagfx::scenerecord
