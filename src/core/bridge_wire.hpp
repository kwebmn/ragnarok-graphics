#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/frame.hpp"
#include "core/image.hpp"

namespace uagfx::bridge {

// Part 3: the 32-bit driver and the 64-bit renderer process exchange plain bytes, little-endian, field by field -
// never a struct copied whole: i686 and x86_64 lay out structs with doubles and bools differently.
class WireWriter {
public:
    explicit WireWriter(std::vector<uint8_t>& out) : out_(out) {}
    void u8(uint8_t v) { out_.push_back(v); }
    void u32(uint32_t v);
    void i32(int32_t v) { u32(uint32_t(v)); }
    void u64(uint64_t v);
    void f32(float v);
    void f64(double v);
    void bytes(const void* p, size_t n);
    void str(const std::string& s);
    void rect(const Rect& r);

private:
    std::vector<uint8_t>& out_;
};

class WireReader {
public:
    WireReader(const uint8_t* p, size_t n) : p_(p), end_(p + n) {}
    uint8_t u8();
    uint32_t u32();
    int32_t i32() { return int32_t(u32()); }
    uint64_t u64();
    float f32();
    double f64();
    const uint8_t* take(size_t n);  // n bytes in place; null when fewer are left (then !ok())
    std::string str();
    Rect rect();
    bool ok() const { return ok_; }
    size_t left() const { return size_t(end_ - p_); }

private:
    const uint8_t* p_;
    const uint8_t* end_;
    bool ok_ = true;
};

void writeRecord(WireWriter& w, const FrameRecord& r);
// False for bytes that are not a whole, consistent record (an index or offset out of its range); r is empty then.
bool readRecord(WireReader& rd, FrameRecord& r);

}  // namespace uagfx::bridge
