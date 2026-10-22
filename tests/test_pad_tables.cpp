#include "pad/core/proto/packet_tables.hpp"
#include "test.hpp"

using namespace uapad;

TEST(tables_server_to_client_lengths) {
    CHECK_EQ(s2cLength(0x0073), 11);
    CHECK_EQ(s2cLength(0x0078), 54);
    CHECK_EQ(s2cLength(0x007c), 41);
    CHECK_EQ(s2cLength(0x0080), 7);
    CHECK_EQ(s2cLength(0x0086), 16);
    CHECK_EQ(s2cLength(0x0087), 12);
    CHECK_EQ(s2cLength(0x008a), 29);
    CHECK_EQ(s2cLength(0x0091), 22);
    CHECK_EQ(s2cLength(0x0092), 28);
    CHECK_EQ(s2cLength(0x009d), 17);
    CHECK_EQ(s2cLength(0x009e), 17);
    CHECK_EQ(s2cLength(0x00a0), 23);
    CHECK_EQ(s2cLength(0x010f), -1);
    CHECK_EQ(s2cLength(0x0115), 35);
    CHECK_EQ(s2cLength(0x01de), 33);
    CHECK_EQ(s2cLength(0x01ee), -1);
    CHECK_EQ(s2cLength(0x022c), 64);
    CHECK_EQ(s2cLength(0x02b9), 191);
}

TEST(tables_client_to_server_lengths) {
    CHECK_EQ(c2sLength(0x009b), 26);
    CHECK_EQ(c2sLength(0x00a7), 8);
    CHECK_EQ(c2sLength(0x0190), 19);
    CHECK_EQ(c2sLength(0x0072), 25);
    CHECK_EQ(c2sLength(0x0113), 22);
    CHECK_EQ(c2sLength(0x00f5), 8);
    CHECK_EQ(c2sLength(0x009f), 14);
    CHECK_EQ(c2sLength(0x02ba), 11);
}

TEST(tables_unknown_ids_are_zero) {
    CHECK_EQ(s2cLength(0x0001), 0);
    CHECK_EQ(s2cLength(0x0300), 0);
    CHECK_EQ(s2cLength(0xffff), 0);
    CHECK_EQ(c2sLength(0x0300), 0);
}
