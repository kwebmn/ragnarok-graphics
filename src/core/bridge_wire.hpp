#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/frame.hpp"
#include "core/image.hpp"

namespace uagfx::bridge {

// Part 3: the 32-bit driver and the 64-bit renderer process exchange plain bytes, little-endian, field by field -
// never a struct copied whole: i686 and x86_64 lay out structs with doubles and bools differently.
// 2.5.20: the small writes inline - a frame's record is tens of thousands of them, on the game's thread.
class WireWriter {
public:
    explicit WireWriter(std::vector<uint8_t>& out) : out_(out) {}
    void u8(uint8_t v) { out_.push_back(v); }
    void u16(uint16_t v) { bytes(&v, 2); }  // 2.7.21
    void u32(uint32_t v) { bytes(&v, 4); }  // x86 on both sides: little-endian as it is
    void i32(int32_t v) { u32(uint32_t(v)); }
    void u64(uint64_t v) { bytes(&v, 8); }
    void f32(float v) { bytes(&v, 4); }
    void f64(double v) { bytes(&v, 8); }
    void bytes(const void* p, size_t n) {  // resize and copy: GCC 12 warns falsely of insert's inlined memcpy
        const size_t at = out_.size();
        out_.resize(at + n);
        if (n) std::memcpy(out_.data() + at, p, n);
    }
    void str(const std::string& s);
    void rect(const Rect& r) {
        i32(r.left);
        i32(r.top);
        i32(r.right);
        i32(r.bottom);
    }

private:
    std::vector<uint8_t>& out_;
};

class WireReader {
public:
    WireReader(const uint8_t* p, size_t n) : p_(p), end_(p + n) {}
    uint8_t u8();
    uint16_t u16();  // 2.7.21
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

// 2.3, uaRO Studio's keys in the game (Msg::SetStudio): bit 0 the X-ray on; bit 1 a pick at (x, y), the cursor in the
// game's picture; bit 2 the picked texture's picture off, or on again (A/B); 2.7.8: bits 3-4 the ground seams' debug
// view ([debug] seams_view: 1 the strips coloured, 2 and over everything).
struct StudioKeys {
    uint8_t flags = 0;
    int32_t x = 0, y = 0;
};
void writeStudioKeys(WireWriter& w, const StudioKeys& k);
bool readStudioKeys(WireReader& rd, StudioKeys& k);  // false: cut short

}  // namespace uagfx::bridge
