#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace uagfx::bridge {

// Part 3: a one-way queue of messages in memory both processes map - one writer, one reader, of either bitness. The
// area starts with RingHeader; chunks follow, each 16-aligned and never split across the end. A message larger than
// maxChunk() goes as several chunks (total, offset) the reader puts together again.
struct alignas(16) RingHeader {
    std::atomic<uint32_t> write;  // bytes written since the start, wrapping at 2^32
    std::atomic<uint32_t> read;   // bytes consumed
    uint32_t capacity;            // of the chunks' part, a power of two
    uint32_t reserved;
};
static_assert(sizeof(RingHeader) == 16, "the same layout in both processes");
static_assert(std::atomic<uint32_t>::is_always_lock_free, "shared between processes");

struct ChunkHeader {
    uint32_t type;    // 0: padding up to the end of the area
    uint32_t size;    // of this chunk's bytes
    uint32_t total;   // of the whole message
    uint32_t offset;  // of this chunk in the message
};
static_assert(sizeof(ChunkHeader) == 16, "the same layout in both processes");

class Ring {
public:
    Ring() = default;
    Ring(void* area, size_t bytes);  // the area: RingHeader and the chunks (the largest power of two that fits)
    void init();                     // once, by the side that created the memory
    bool valid() const { return h_ != nullptr; }
    uint32_t maxChunk() const { return cap_ / 2 - uint32_t(sizeof(ChunkHeader)); }  // always fits an empty ring

    // Writes one chunk; false when there is no room now (the reader has not caught up) or size > maxChunk().
    bool tryWrite(uint32_t type, uint32_t total, uint32_t offset, const void* data, uint32_t size);
    // The next chunk in place, or false when there is none; consume() frees it.
    bool peek(ChunkHeader& h, const uint8_t*& data);
    void consume();
    bool empty() const;

private:
    RingHeader* h_ = nullptr;
    uint8_t* data_ = nullptr;
    uint32_t cap_ = 0;
};

// Puts chunks together: feed() each one in order; complete() when the last chunk of a message has come.
class Assembler {
public:
    // False for a chunk that does not continue the message being assembled (damaged stream).
    bool feed(const ChunkHeader& h, const uint8_t* data);
    bool complete() const { return have_ && got_ == total_; }
    uint32_t type() const { return type_; }
    const uint8_t* data() const { return buf_; }
    uint32_t size() const { return total_; }
    void reset();
    ~Assembler();

private:
    uint8_t* buf_ = nullptr;
    uint32_t cap_ = 0;
    uint32_t type_ = 0;
    uint32_t total_ = 0;
    uint32_t got_ = 0;
    bool have_ = false;
};

}  // namespace uagfx::bridge
