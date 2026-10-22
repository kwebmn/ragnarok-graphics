#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "pad/core/logic/pad.hpp"

namespace uapad::sony {

// 0.6: Sony's controllers read as HID devices (they are not XInput): the DualShock 4 (both versions and its wireless
// adapter) and the DualSense (and its Edge), over USB or Bluetooth - their input reports into the pad's state, and
// their output reports for the rumble and the light bar.

constexpr uint16_t kVendor = 0x054C;
enum class Model { None, DualShock4, DualSense };
Model modelOf(uint16_t vendor, uint16_t product);

// A light bar's colour, 0xRRGGBB.
using Colour = uint32_t;

// One input report as it was read (its first byte the report id) into `out`; false for a report this does not read.
// Cross = A, Circle = B, Square = X, Triangle = Y, L1/R1 = LB/RB, L2/R2 = the triggers, Share/Create = Back,
// Options = Start, the touchpad's click as `touchpad` (a PadButton bit, or 0).
bool parse(Model m, const uint8_t* report, size_t size, uint16_t touchpad, PadState& out);

// The output report setting the motors (0-255 each: the strong left, the weak right) and the light bar, for USB or
// Bluetooth (with its CRC). `sequence` counts the DualSense's Bluetooth reports (0-15).
std::vector<uint8_t> output(Model m, bool bluetooth, uint8_t left, uint8_t right, Colour light, uint8_t sequence);

// The CRC-32 Bluetooth reports carry (over the 0xA2 header byte and the report).
uint32_t crc32(const uint8_t* data, size_t size, uint32_t crc = 0);

}  // namespace uapad::sony
