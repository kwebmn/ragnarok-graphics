#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "pad/core/bytes.hpp"

namespace uapad {

enum class Direction { ServerToClient, ClientToServer };

// Cuts a TCP byte stream into whole packets using the uAthena length tables.
class StreamFramer {
public:
    // rawPrefix: bytes at the very start of the stream that are not a packet
    // (the map server sends the 4-byte account id right after 0x009b).
    explicit StreamFramer(Direction dir, size_t rawPrefix = 0);

    // Appends complete packets to `out`. Returns false once the stream is desynchronized;
    // after that all input is ignored.
    bool feed(const uint8_t* data, size_t n, std::vector<Bytes>& out);

    bool desynced() const { return desynced_; }
    bool atBoundary() const { return !desynced_ && buf_.empty() && prefixLeft_ == 0; }
    const Bytes& prefix() const { return prefix_; }
    uint16_t badId() const { return badId_; }

private:
    int lengthOf(uint16_t id) const;

    Direction dir_;
    size_t prefixLeft_;
    Bytes prefix_;
    Bytes buf_;
    bool desynced_ = false;
    uint16_t badId_ = 0;
};

}  // namespace uapad
