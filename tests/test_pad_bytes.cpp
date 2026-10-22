#include "pad/core/bytes.hpp"
#include "test.hpp"

using namespace uapad;

TEST(bytes_little_endian) {
    const uint8_t p[] = {0x34, 0x12, 0x78, 0x56, 0x34, 0x12};
    CHECK_EQ(rd16(p, 0), 0x1234);
    CHECK_EQ(rd32(p, 2), 0x12345678u);
    Bytes b(6, 0);
    wr16(b, 0, 0xbeef);
    wr32(b, 2, 0xdeadc0de);
    CHECK_EQ(b[0], 0xef);
    CHECK_EQ(b[1], 0xbe);
    CHECK_EQ(rd32(b.data(), 2), 0xdeadc0deu);
}

TEST(bytes_pos3_matches_wbufpos) {
    Bytes b(3, 0);
    encodePos3(b, 0, Cell{150, 287}, 5);
    // WBUFPOS: x>>2 = 37; (x<<6)|((y>>4)&0x3f) = 0x80|0x11; (y<<4)|dir = 0xf0|5
    CHECK_EQ(b[0], 37);
    CHECK_EQ(b[1], 0x91);
    CHECK_EQ(b[2], 0xf5);
    Cell c = decodePos3(b.data(), 0);
    CHECK_EQ(c.x, 150);
    CHECK_EQ(c.y, 287);
}

TEST(bytes_pos6_roundtrip) {
    Bytes b(6, 0);
    encodePos6(b, 0, Cell{100, 200}, Cell{105, 190});
    CHECK_EQ(b[0], 25);
    CHECK_EQ(b[2], 0x81);
    CHECK_EQ(b[5], 0x88);
    Move m = decodePos6(b.data(), 0);
    CHECK(m.from == (Cell{100, 200}));
    CHECK(m.to == (Cell{105, 190}));
}

TEST(bytes_cell_distance_is_chebyshev) {
    CHECK_EQ(cellDistance(Cell{10, 10}, Cell{13, 11}), 3);
    CHECK_EQ(cellDistance(Cell{10, 10}, Cell{7, 16}), 6);
    CHECK_EQ(cellDistance(Cell{5, 5}, Cell{5, 5}), 0);
    CHECK((Cell{1, 2} != Cell{2, 1}));
}
