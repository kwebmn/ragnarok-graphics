#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::scenerecord {

// 3.0.17-beta.7 (the concept «простой старт і налаштування»: the video card checked without the game - the settings
// app plays a recorded scene through the renderer): [debug] record_scene - every message the driver sends the
// renderer (core/bridge_protocol.hpp), from the start to kAfterMapMs after the first map, into
// uaro-graphics\scene-record.bin: kMagic, then each message as kEntryHead (u32 type, u32 size) and its bytes. Step one
// measures how big a scene is; the replay comes after.

constexpr char kMagic[9] = "UAROSCN1";
constexpr size_t kEntryHead = 8;
constexpr uint64_t kAfterMapMs = 15000;
constexpr uint64_t kMostBytes = 1536ull << 20;  // 1.5 GB: a scene past it is not worth replaying
constexpr uint32_t kExecute = 8;                 // Msg::Execute - a frame's draws

void entryHead(uint32_t type, uint32_t size, uint8_t out[kEntryHead]);

// Which messages go into the file: true - write it; false - not, and the record is over (done)
class Recorder {
public:
    bool take(uint32_t type, size_t size, bool mapShown, uint64_t nowMs);
    bool done() const { return done_; }
    int messages() const { return messages_; }
    int frames() const { return frames_; }
    uint64_t bytes() const { return bytes_; }
    bool mapSeen() const { return mapAt_ != 0 || mapSeen_; }
    // 3.0.17-beta.8 (R-MANK7P: 1192 MB in 24 s): what the recorded frames carry - texture pixels (Upload), pixels the
    // game wrote into the picture (WriteTarget), the draws' vertices - before the first map and after it
    struct Parts {
        uint64_t uploads = 0, writes = 0, vertices = 0;
    };
    void noteFrame(uint64_t uploads, uint64_t writes, uint64_t vertices);
    const Parts& before() const { return before_; }
    const Parts& after() const { return after_; }

private:
    bool done_ = false, mapSeen_ = false;
    uint64_t mapAt_ = 0, bytes_ = 0;
    int messages_ = 0, frames_ = 0;
    Parts before_, after_;
};

}  // namespace uagfx::scenerecord
