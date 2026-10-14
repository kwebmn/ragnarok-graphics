#include <cstring>
#include <vector>

#include "core/bridge_ring.hpp"
#include "core/bridge_wire.hpp"
#include "core/d3d7.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::bridge;

namespace {

FrameRecord sampleRecord() {
    FrameRecord r;
    r.clear(ClearCmd{3, 0xFF102030, 0.5f, Rect{1, 2, 30, 40}});
    r.viewport(ViewportCmd{Rect{0, 0, 640, 480}, 0.25f, 0.75f});
    DrawSetup s;
    s.zEnable = true;
    s.blend = true;
    s.srcBlend = d3d7::BLEND_SRCALPHA;
    s.fogColor = 0x00ABCDEF;
    s.stage[1].borderColor = 0x80112233;
    s.ui = true;
    DrawCmd d;
    d.state = r.addState(s);
    d.texture[0] = 0x40000001;
    d.keyed[1] = true;
    d.primitive = d3d7::PT_TRIANGLESTRIP;
    d.fvf = d3d7::FVF_TL2;
    d.twin = 7;
    d.twinTile[0] = -3;
    d.twinTile[1] = 9;
    d.twinScale = 2.625;
    std::vector<uint8_t> vertices(3 * FrameRecord::stride(d3d7::FVF_TL2));
    for (size_t i = 0; i < vertices.size(); ++i) vertices[i] = uint8_t(i * 7);
    const uint16_t indices[3] = {2, 0, 1};
    r.draw(d, vertices.data(), 3, indices, 3);
    const uint32_t pixels[6] = {1, 2, 3, 4, 5, 6};
    r.upload(0x40000002, 1, Rect{4, 4, 7, 6}, pixels);
    r.writeTarget(Rect{0, 0, 2, 1}, pixels + 4);
    r.upload(0x40000003, 0, Rect{0, 0, 3, 2}, pixels, {Rect{0, 0, 1, 1}, Rect{2, 0, 3, 2}});  // 0.13: cells
    return r;
}

std::vector<uint8_t> bytesOf(const FrameRecord& r) {
    std::vector<uint8_t> out;
    WireWriter w(out);
    writeRecord(w, r);
    return out;
}

}  // namespace

TEST(bridge_a_record_crosses_field_by_field) {
    // Part 3: the 64-bit renderer gets exactly what the driver recorded.
    const FrameRecord a = sampleRecord();
    const std::vector<uint8_t> bytes = bytesOf(a);
    FrameRecord b;
    WireReader rd(bytes.data(), bytes.size());
    CHECK(readRecord(rd, b));
    CHECK_EQ(rd.left(), size_t(0));
    CHECK(bytesOf(b) == bytes);  // every part, written again, gives the same bytes
    CHECK_EQ(b.draws().size(), size_t(1));
    const DrawCmd& d = b.draws()[0];
    CHECK_EQ(d.twinScale, 2.625);
    CHECK_EQ(d.twinTile[0], -3);
    CHECK(d.keyed[1] && !d.keyed[0]);
    CHECK(b.states()[0] == a.states()[0]);
    CHECK(b.vertexBytes() == a.vertexBytes());
    CHECK(b.indices() == a.indices());
    CHECK(b.pixels() == a.pixels());
    CHECK(b.uploads()[0].rect == (Rect{4, 4, 7, 6}));
    CHECK_EQ(b.uploads()[0].slotCount, 0u);
    CHECK_EQ(b.uploads()[1].firstSlot, 0u);
    CHECK_EQ(b.uploads()[1].slotCount, 2u);
    CHECK(b.slots() == a.slots());
    CHECK(b.slots()[1] == (Rect{2, 0, 3, 2}));
}

TEST(bridge_a_damaged_record_is_refused) {
    const std::vector<uint8_t> bytes = bytesOf(sampleRecord());
    FrameRecord r;
    for (size_t cut : {size_t(0), size_t(3), bytes.size() / 2, bytes.size() - 1}) {
        WireReader rd(bytes.data(), cut);
        CHECK(!readRecord(rd, r));
        CHECK(r.empty());
    }
    std::vector<uint8_t> bad = bytes;
    bad[4 + 0] = 9;  // the first command's type: past WriteTarget
    WireReader rd(bad.data(), bad.size());
    CHECK(!readRecord(rd, r));
    bad = bytes;
    bad[5] = 200;  // the first command's index: no such clear
    WireReader rd2(bad.data(), bad.size());
    CHECK(!readRecord(rd2, r));
}

TEST(bridge_the_ring_keeps_order_across_its_end_and_2_to_the_32) {
    std::vector<uint8_t> area(sizeof(RingHeader) + 4096 + 48);
    Ring ring(area.data(), area.size());
    ring.init();
    CHECK_EQ(ring.maxChunk(), uint32_t(4096 / 2 - 16));
    // Start just below 2^32: the positions wrap during the test.
    auto* h = reinterpret_cast<RingHeader*>(area.data());
    h->write.store(0xFFFFF000u);
    h->read.store(0xFFFFF000u);
    uint32_t next = 0;  // the next message number to read
    uint32_t sent = 0;
    for (int round = 0; round <= 3000; ++round) {
        const uint32_t size = (round * 97u) % 900u;
        std::vector<uint8_t> msg(size, uint8_t(round));
        if (round < 3000 && ring.tryWrite(1 + sent % 5, size, 0, msg.data(), size)) ++sent;
        if (round % 3 != 0) continue;  // the reader runs a little slower: the ring fills up; the last round drains
        ChunkHeader c;
        const uint8_t* data = nullptr;
        while (ring.peek(c, data)) {
            CHECK_EQ(c.type, 1 + next % 5);
            CHECK_EQ(c.size, c.total);
            bool same = true;
            for (uint32_t i = 0; i < c.size; ++i) same = same && data[i] == data[0];
            CHECK(same);
            ring.consume();
            ++next;
        }
    }
    CHECK(sent > 1000);
    CHECK_EQ(next, sent);
    CHECK(ring.empty());
    std::vector<uint8_t> big(ring.maxChunk() + 1);
    CHECK(!ring.tryWrite(1, uint32_t(big.size()), 0, big.data(), uint32_t(big.size())));  // never past maxChunk
}

TEST(bridge_chunks_make_the_message_again) {
    Assembler a;
    const uint8_t part1[3] = {1, 2, 3};
    const uint8_t part2[2] = {4, 5};
    CHECK(a.feed(ChunkHeader{9, 3, 5, 0}, part1));
    CHECK(!a.complete());
    CHECK(a.feed(ChunkHeader{9, 2, 5, 3}, part2));
    CHECK(a.complete());
    CHECK_EQ(a.size(), uint32_t(5));
    CHECK(std::memcmp(a.data(), "\x01\x02\x03\x04\x05", 5) == 0);
    CHECK(!a.feed(ChunkHeader{9, 2, 5, 4}, part2));  // not where the last one ended
    CHECK(!a.complete());
}
