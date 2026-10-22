#include "pad/core/logic/sony.hpp"

#include <cstring>

namespace uapad::sony {
namespace {

float stick(uint8_t v) { return v >= 128 ? float(v - 128) / 127.0f : float(int(v) - 128) / 128.0f; }

// The four directions of the hat (0 up, clockwise to 7, 8 and more: none).
uint16_t hat(uint8_t v) {
    static const uint16_t kDirs[8] = {kPadUp,
                                      uint16_t(kPadUp | kPadRight),
                                      kPadRight,
                                      uint16_t(kPadDown | kPadRight),
                                      kPadDown,
                                      uint16_t(kPadDown | kPadLeft),
                                      kPadLeft,
                                      uint16_t(kPadUp | kPadLeft)};
    return v < 8 ? kDirs[v] : 0;
}

// The common part: sticks at s[0..3], the hat and the face buttons in f, the shoulders in g, the PS / touchpad byte
// in h, the triggers lt, rt.
void fill(const uint8_t* s, uint8_t f, uint8_t g, uint8_t h, uint8_t lt, uint8_t rt, uint16_t touchpad,
          PadState& out) {
    out.connected = true;
    out.lx = stick(s[0]);
    out.ly = -stick(s[1]);  // up is 0
    out.rx = stick(s[2]);
    out.ry = -stick(s[3]);
    uint16_t b = hat(f & 0x0F);
    if (f & 0x10) b |= kPadX;  // Square
    if (f & 0x20) b |= kPadA;  // Cross
    if (f & 0x40) b |= kPadB;  // Circle
    if (f & 0x80) b |= kPadY;  // Triangle
    if (g & 0x01) b |= kPadLB;
    if (g & 0x02) b |= kPadRB;
    if (g & 0x10) b |= kPadBack;   // Share / Create
    if (g & 0x20) b |= kPadStart;  // Options
    if (g & 0x40) b |= kPadL3;
    if (g & 0x80) b |= kPadR3;
    if (h & 0x02) b |= touchpad;
    out.buttons = b;
    out.lt = float(lt) / 255.0f;
    out.rt = float(rt) / 255.0f;
}

}  // namespace

Model modelOf(uint16_t vendor, uint16_t product) {
    if (vendor != kVendor) return Model::None;
    switch (product) {
        case 0x05C4:  // DualShock 4
        case 0x09CC:  // DualShock 4, second version
        case 0x0BA0:  // its wireless adapter
            return Model::DualShock4;
        case 0x0CE6:  // DualSense
        case 0x0DF2:  // DualSense Edge
            return Model::DualSense;
        default: return Model::None;
    }
}

bool parse(Model m, const uint8_t* r, size_t n, uint16_t touchpad, PadState& out) {
    if (!r || n < 10) return false;
    const uint8_t id = r[0];
    if (m == Model::DualShock4) {
        if (id == 0x01 && n >= 10) {  // USB, and Bluetooth before the full reports
            fill(r + 1, r[5], r[6], r[7], r[8], r[9], touchpad, out);
            return true;
        }
        if (id >= 0x11 && id <= 0x19 && n >= 12) {  // Bluetooth, full: two bytes more first
            fill(r + 3, r[7], r[8], r[9], r[10], r[11], touchpad, out);
            return true;
        }
        return false;
    }
    if (m == Model::DualSense) {
        if (id == 0x01 && n >= 64) {  // USB: the triggers before the buttons
            fill(r + 1, r[8], r[9], r[10], r[5], r[6], touchpad, out);
            return true;
        }
        if (id == 0x01) {  // Bluetooth before the full reports: as the DualShock 4's
            fill(r + 1, r[5], r[6], r[7], r[8], r[9], touchpad, out);
            return true;
        }
        if (id == 0x31 && n >= 12) {  // Bluetooth, full: one byte more first
            fill(r + 2, r[9], r[10], r[11], r[6], r[7], touchpad, out);
            return true;
        }
    }
    return false;
}

uint32_t crc32(const uint8_t* data, size_t size, uint32_t crc) {
    crc = ~crc;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

std::vector<uint8_t> output(Model m, bool bluetooth, uint8_t left, uint8_t right, Colour light, uint8_t sequence) {
    const uint8_t red = uint8_t(light >> 16), green = uint8_t(light >> 8), blue = uint8_t(light);
    std::vector<uint8_t> o;
    if (m == Model::DualShock4) {
        if (!bluetooth) {
            o.assign(32, 0);
            o[0] = 0x05;
            o[1] = 0x07;  // the rumble, the light bar, its flash
            o[4] = right;
            o[5] = left;
            o[6] = red;
            o[7] = green;
            o[8] = blue;
            return o;
        }
        o.assign(78, 0);
        o[0] = 0x11;
        o[1] = 0xC0;  // a HID report with its CRC
        o[3] = 0x07;
        o[6] = right;
        o[7] = left;
        o[8] = red;
        o[9] = green;
        o[10] = blue;
    } else if (m == Model::DualSense) {
        // The common part (after the report id, and on Bluetooth its sequence tag too).
        std::vector<uint8_t> c(47, 0);
        c[0] = 0x03;  // the rumble as the DualShock 4's, not the haptics
        c[1] = 0x04;  // the light bar's colour
        c[2] = right;
        c[3] = left;
        c[38] = 0x02;  // the light bar's own setup: its start-up blue faded out
        c[41] = 0x02;
        c[44] = red;
        c[45] = green;
        c[46] = blue;
        if (!bluetooth) {
            o.assign(48, 0);
            o[0] = 0x02;
            std::memcpy(o.data() + 1, c.data(), c.size());
            return o;
        }
        o.assign(78, 0);
        o[0] = 0x31;
        o[1] = uint8_t((sequence & 0x0F) << 4);
        o[2] = 0x10;  // a HID report
        std::memcpy(o.data() + 3, c.data(), c.size());
    } else {
        return o;
    }
    const uint8_t header = 0xA2;
    const uint32_t crc = crc32(o.data(), 74, crc32(&header, 1));
    o[74] = uint8_t(crc);
    o[75] = uint8_t(crc >> 8);
    o[76] = uint8_t(crc >> 16);
    o[77] = uint8_t(crc >> 24);
    return o;
}

}  // namespace uapad::sony
