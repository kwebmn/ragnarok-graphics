#include <cmath>
#include <cstring>
#include <vector>

#include "pad/core/logic/sony.hpp"
#include "test.hpp"

using namespace uapad;

TEST(sony_models_by_vendor_and_product) {
    CHECK(sony::modelOf(0x054C, 0x05C4) == sony::Model::DualShock4);
    CHECK(sony::modelOf(0x054C, 0x09CC) == sony::Model::DualShock4);
    CHECK(sony::modelOf(0x054C, 0x0CE6) == sony::Model::DualSense);
    CHECK(sony::modelOf(0x054C, 0x0DF2) == sony::Model::DualSense);
    CHECK(sony::modelOf(0x045E, 0x028E) == sony::Model::None);  // an Xbox controller
}

TEST(sony_dualshock4_usb_report) {
    std::vector<uint8_t> r(64, 0);
    r[0] = 0x01;
    r[1] = 255;  // left stick right
    r[2] = 0;    // and up
    r[3] = 128;
    r[4] = 128;
    r[5] = 0x20 | 0x02;  // Cross, the hat right
    r[6] = 0x01 | 0x20;  // L1, Options
    r[7] = 0x02;         // the touchpad's click
    r[8] = 255;          // L2 all the way
    r[9] = 0;
    PadState p;
    CHECK(sony::parse(sony::Model::DualShock4, r.data(), r.size(), kPadBack, p));
    CHECK(p.connected && p.lx > 0.99f && p.ly > 0.99f && std::fabs(p.rx) < 0.01f);
    CHECK((p.buttons & kPadA) && (p.buttons & kPadRight) && (p.buttons & kPadLB) && (p.buttons & kPadStart));
    CHECK((p.buttons & kPadBack) && !(p.buttons & kPadB));
    CHECK(p.lt == 1.0f && p.rt == 0.0f);
}

TEST(sony_dualshock4_bluetooth_and_dualsense_reports) {
    std::vector<uint8_t> bt(78, 0);
    bt[0] = 0x11;
    bt[3] = bt[4] = bt[5] = bt[6] = 128;
    bt[7] = 0x80 | 0x08;  // Triangle, no direction
    PadState p;
    CHECK(sony::parse(sony::Model::DualShock4, bt.data(), bt.size(), 0, p));
    CHECK(p.buttons == kPadY);
    std::vector<uint8_t> usb(64, 0);  // DualSense over USB: the triggers at 5, 6, the buttons at 8-10
    usb[0] = 0x01;
    usb[1] = usb[2] = usb[3] = usb[4] = 128;
    usb[6] = 200;
    usb[8] = 0x40 | 0x08;  // Circle
    usb[9] = 0x10 | 0x80;  // Create, R3
    CHECK(sony::parse(sony::Model::DualSense, usb.data(), usb.size(), 0, p));
    CHECK(p.buttons == (kPadB | kPadBack | kPadR3) && p.rt > 0.78f);
    std::vector<uint8_t> dbt(78, 0);  // over Bluetooth: one byte later
    dbt[0] = 0x31;
    dbt[2] = dbt[3] = dbt[4] = dbt[5] = 128;
    dbt[9] = 0x10 | 0x04;  // Square, the hat down
    CHECK(sony::parse(sony::Model::DualSense, dbt.data(), dbt.size(), 0, p));
    CHECK(p.buttons == (kPadX | kPadDown));
    const uint8_t unknown[12] = {0x05};
    CHECK(!sony::parse(sony::Model::DualSense, unknown, sizeof unknown, 0, p));
}

TEST(sony_output_reports_and_their_crc) {
    // The CRC-32 of "123456789" is CBF43926.
    const char* s = "123456789";
    CHECK(sony::crc32(reinterpret_cast<const uint8_t*>(s), 9) == 0xCBF43926u);
    const std::vector<uint8_t> usb = sony::output(sony::Model::DualShock4, false, 200, 50, 0x112233, 0);
    CHECK(usb.size() == 32 && usb[0] == 0x05 && usb[4] == 50 && usb[5] == 200 && usb[6] == 0x11 && usb[8] == 0x33);
    const std::vector<uint8_t> bt = sony::output(sony::Model::DualShock4, true, 1, 2, 0xFF0000, 0);
    CHECK(bt.size() == 78 && bt[0] == 0x11 && bt[8] == 0xFF);
    const uint8_t header = 0xA2;
    const uint32_t crc = sony::crc32(bt.data(), 74, sony::crc32(&header, 1));
    uint32_t stored = 0;
    std::memcpy(&stored, bt.data() + 74, 4);
    CHECK(stored == crc);
    const std::vector<uint8_t> ds = sony::output(sony::Model::DualSense, false, 9, 8, 0x0000FF, 0);
    CHECK(ds.size() == 48 && ds[0] == 0x02 && ds[3] == 8 && ds[4] == 9 && ds[47] == 0xFF);
    const std::vector<uint8_t> dsbt = sony::output(sony::Model::DualSense, true, 0, 0, 0, 3);
    CHECK(dsbt.size() == 78 && dsbt[0] == 0x31 && dsbt[1] == 0x30);
}

#include "pad/core/config/config.hpp"

TEST(sony_settings_read) {
    const Config d = parseConfig("");
    CHECK(d.padType == Config::PadType::Auto && d.lightbar == 0x0040FF && d.touchpad == 0);
    const Config c = parseConfig("[pad]\ntype = sony\n[sony]\nlightbar = ff8800\ntouchpad = start\n");
    CHECK(c.padType == Config::PadType::Sony && c.lightbar == 0xFF8800 && c.touchpad == kPadStart);
    CHECK(parseConfig("[sony]\nlightbar = purple\n").lightbar == 0x9000FF);
    CHECK(parseConfig("[sony]\nlightbar = nonsense\n").lightbar == 0x0040FF);
    CHECK(parseConfig("[pad]\ntype = xbox\n").padType == Config::PadType::Xbox);
}
