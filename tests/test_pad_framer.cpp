#include <initializer_list>

#include "pad/core/proto/framer.hpp"
#include "test.hpp"

using namespace uapad;

static Bytes pkt(uint16_t id, size_t len) {
    Bytes b(len, 0);
    wr16(b, 0, id);
    return b;
}

static Bytes cat(std::initializer_list<Bytes> parts) {
    Bytes out;
    for (const Bytes& p : parts) out.insert(out.end(), p.begin(), p.end());
    return out;
}

TEST(framer_whole_packets) {
    StreamFramer f(Direction::ServerToClient);
    Bytes s = cat({pkt(0x0080, 7), pkt(0x0088, 10)});
    std::vector<Bytes> out;
    CHECK(f.feed(s.data(), s.size(), out));
    CHECK_EQ(out.size(), 2u);
    CHECK_EQ(out[0].size(), 7u);
    CHECK_EQ(rd16(out[1].data(), 0), 0x0088);
    CHECK(f.atBoundary());
}

TEST(framer_split_at_every_byte) {
    Bytes s = cat({pkt(0x0080, 7), pkt(0x008a, 29), pkt(0x0088, 10)});
    for (size_t cut = 1; cut < s.size(); ++cut) {
        StreamFramer f(Direction::ServerToClient);
        std::vector<Bytes> out;
        CHECK(f.feed(s.data(), cut, out));
        CHECK(f.feed(s.data() + cut, s.size() - cut, out));
        CHECK_EQ(out.size(), 3u);
        CHECK(f.atBoundary());
    }
}

TEST(framer_variable_length) {
    Bytes v = pkt(0x010f, 4 + 37);
    wr16(v, 2, 4 + 37);
    Bytes s = cat({v, pkt(0x0080, 7)});
    StreamFramer f(Direction::ServerToClient);
    std::vector<Bytes> out;
    CHECK(f.feed(s.data(), 3, out));  // id and half of the length field
    CHECK_EQ(out.size(), 0u);
    CHECK(!f.atBoundary());
    CHECK(f.feed(s.data() + 3, s.size() - 3, out));
    CHECK_EQ(out.size(), 2u);
    CHECK_EQ(out[0].size(), 41u);
}

TEST(framer_raw_prefix_is_skipped) {
    Bytes s = {0x40, 0xe2, 0x01, 0x00};  // account id 123456, sent raw by the map server after 0x009b
    Bytes auth = pkt(0x0073, 11);
    s.insert(s.end(), auth.begin(), auth.end());
    StreamFramer f(Direction::ServerToClient, 4);
    std::vector<Bytes> out;
    CHECK(f.feed(s.data(), 2, out));  // the prefix itself arrives split
    CHECK(!f.atBoundary());
    CHECK(f.feed(s.data() + 2, s.size() - 2, out));
    CHECK_EQ(out.size(), 1u);
    CHECK_EQ(rd16(out[0].data(), 0), 0x0073);
    CHECK_EQ(f.prefix().size(), 4u);
    CHECK_EQ(rd32(f.prefix().data(), 0), 123456u);
}

TEST(framer_unknown_id_desyncs) {
    Bytes s = cat({pkt(0x0080, 7), pkt(0x0001, 8)});
    StreamFramer f(Direction::ServerToClient);
    std::vector<Bytes> out;
    CHECK(!f.feed(s.data(), s.size(), out));
    CHECK(f.desynced());
    CHECK_EQ(f.badId(), 0x0001);
    CHECK_EQ(out.size(), 1u);
    CHECK(!f.feed(s.data(), s.size(), out));
    CHECK_EQ(out.size(), 1u);
}

TEST(framer_bad_variable_length_desyncs) {
    Bytes v = pkt(0x010f, 4);
    wr16(v, 2, 2);  // shorter than its own header
    StreamFramer f(Direction::ServerToClient);
    std::vector<Bytes> out;
    CHECK(!f.feed(v.data(), v.size(), out));
    CHECK(f.desynced());
}

TEST(framer_client_direction_uses_client_table) {
    Bytes s = cat({pkt(0x0072, 25), pkt(0x00a7, 8)});
    StreamFramer f(Direction::ClientToServer);
    std::vector<Bytes> out;
    CHECK(f.feed(s.data(), s.size(), out));
    CHECK_EQ(out.size(), 2u);
    CHECK_EQ(out[0].size(), 25u);
}

TEST(framer_id_zero_is_a_desync) {
    StreamFramer f(Direction::ServerToClient);
    std::vector<Bytes> out;
    const Bytes zeros(10, 0);  // clif.c lists 0x0000 as 10 bytes, but the server never sends it
    CHECK(!f.feed(zeros.data(), zeros.size(), out));
    CHECK(f.desynced());
    CHECK(out.empty());
    CHECK(!f.atBoundary());  // a lost stream has no boundary to write at
}
