#include "core/bridge_ring.hpp"

#include <cstdlib>
#include <cstring>
#include <new>

namespace uagfx::bridge {
namespace {

uint32_t align16(uint32_t n) { return (n + 15u) & ~15u; }

}  // namespace

Ring::Ring(void* area, size_t bytes) {
    if (!area || bytes < sizeof(RingHeader) + 64) return;
    h_ = static_cast<RingHeader*>(area);
    data_ = static_cast<uint8_t*>(area) + sizeof(RingHeader);
    // A power of two: positions count bytes modulo 2^32, so the place in the area stays right across that wrap.
    const size_t room = bytes - sizeof(RingHeader);
    cap_ = 64;
    while (cap_ <= room / 2 && cap_ < (1u << 30)) cap_ *= 2;
}

void Ring::init() {
    if (!h_) return;
    new (h_) RingHeader;
    h_->write.store(0, std::memory_order_relaxed);
    h_->read.store(0, std::memory_order_relaxed);
    h_->capacity = cap_;
    h_->reserved = 0;
}

bool Ring::tryWrite(uint32_t type, uint32_t total, uint32_t offset, const void* data, uint32_t size) {
    if (!h_ || size > maxChunk()) return false;
    const uint32_t need = align16(uint32_t(sizeof(ChunkHeader)) + size);
    uint32_t w = h_->write.load(std::memory_order_relaxed);  // only this side moves it
    const uint32_t r = h_->read.load(std::memory_order_acquire);
    uint32_t at = w % cap_;
    const uint32_t tail = cap_ - at;
    uint32_t skip = 0;
    if (tail < need) skip = tail;  // the chunk goes to the start; the end is padding
    if ((w - r) + skip + need > cap_) return false;
    if (skip) {
        const ChunkHeader pad{0, skip - uint32_t(sizeof(ChunkHeader)), 0, 0};
        std::memcpy(data_ + at, &pad, sizeof pad);
        w += skip;
        at = 0;
    }
    const ChunkHeader c{type, size, total, offset};
    std::memcpy(data_ + at, &c, sizeof c);
    if (size) std::memcpy(data_ + at + sizeof c, data, size);
    h_->write.store(w + need, std::memory_order_release);
    return true;
}

bool Ring::peek(ChunkHeader& c, const uint8_t*& data) {
    if (!h_) return false;
    for (;;) {
        const uint32_t r = h_->read.load(std::memory_order_relaxed);  // only this side moves it
        const uint32_t w = h_->write.load(std::memory_order_acquire);
        if (r == w) return false;
        const uint32_t at = r % cap_;
        std::memcpy(&c, data_ + at, sizeof c);
        if (c.type != 0) {
            data = data_ + at + sizeof c;
            return true;
        }
        h_->read.store(r + (cap_ - at), std::memory_order_release);  // padding: on to the start
    }
}

void Ring::consume() {
    if (!h_) return;
    const uint32_t r = h_->read.load(std::memory_order_relaxed);
    ChunkHeader c;
    std::memcpy(&c, data_ + r % cap_, sizeof c);
    h_->read.store(r + align16(uint32_t(sizeof(ChunkHeader)) + c.size), std::memory_order_release);
}

bool Ring::empty() const {
    return !h_ || h_->read.load(std::memory_order_acquire) == h_->write.load(std::memory_order_acquire);
}

bool Assembler::feed(const ChunkHeader& h, const uint8_t* data) {
    if (h.offset == 0) {
        reset();
        if (h.total > cap_) {
            void* p = std::realloc(buf_, h.total ? h.total : 1);
            if (!p) return false;
            buf_ = static_cast<uint8_t*>(p);
            cap_ = h.total;
        }
        type_ = h.type;
        total_ = h.total;
        have_ = true;
    } else if (!have_ || h.type != type_ || h.offset != got_ || h.total != total_) {
        reset();
        return false;
    }
    if (uint64_t(got_) + h.size > total_) {
        reset();
        return false;
    }
    if (h.size) std::memcpy(buf_ + got_, data, h.size);
    got_ += h.size;
    return true;
}

void Assembler::reset() {
    have_ = false;
    type_ = total_ = got_ = 0;
}

Assembler::~Assembler() { std::free(buf_); }

}  // namespace uagfx::bridge
