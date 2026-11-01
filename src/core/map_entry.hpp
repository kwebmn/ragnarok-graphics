#pragma once
#include <cstdint>

namespace uagfx::map_entry {

// 2.6.17, [connection]: the legacy client's way onto a map. Once a character is chosen the char server tells the map
// server to expect it (an auth node) and the client where the map is (0x71); the client connects - not waiting, so a
// connect that fails shows only in select()'s exceptions, which it never asks: a lost SYN left it waiting for nothing
// - sends CZ_ENTER and gives up after 15 s (CLoginMode state 0xc), then logs in again from the start. The uAthena map
// server loses the next entry's node while the old one lingers (chrif_authok): one slow entry turned into a loop of
// failed ones until the old node was gone - 90 s for a client that never came, 270 s for one that came and left,
// each with a sweep every 30 s ("works after several tries or ten minutes", the server left as it is).
constexpr uint32_t kPatientWaitMs = 60000;              // the client's wait for the map, patient (was 15000)
constexpr int kMaxReconnects = 4;                        // a failed connect made again, an entry's most
constexpr uint64_t kNodeGoneMs = 125000;                 // a node whose client never came: 90 s + 30 s + 5 s
constexpr uint64_t kNodeOfLeftClientGoneMs = 305000;     // one whose client came and left: 270 s + 30 s + 5 s

enum class Step { None, Reconnect };

struct Seen {  // the client as the driver sees it this frame
    bool loginMode = false;   // its login and character screens are the game's mode
    int state = -1;           // their state
    bool connected = false;   // the map's connection made
    bool failed = false;      // its connect failed
};

class Guard {
public:
    // Once a frame (now in milliseconds): Reconnect - patient, the connect failed, tries left.
    Step frame(const Seen& s, uint64_t now, bool patient);
    // After an entry that failed: the next character's selection waits until its node is gone (0: none).
    uint64_t holdUntil() const { return holdUntil_; }
    bool holding(uint64_t now, bool pause) const { return pause && now < holdUntil_; }

private:
    bool entering_ = false;
    uint64_t since_ = 0;   // the map's address had: its node made about then
    bool came_ = false;    // the client came to the map
    int reconnects_ = 0;
    uint64_t holdUntil_ = 0;
};

}  // namespace uagfx::map_entry
