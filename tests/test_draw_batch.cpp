#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "core/d3d7.hpp"
#include "core/draw_batch.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

using Tri = std::array<uint32_t, 3>;

// A triangle by its vertices in their turning order, from the smallest: the same triangle facing the same way.
Tri canonical(Tri t) {
    while (t[0] != std::min({t[0], t[1], t[2]})) t = {t[1], t[2], t[0]};
    return t;
}

// The triangles Direct3D draws for a strip: (i, i+1, i+2), every second one turned (i+1, i, i+2).
std::vector<Tri> stripTriangles(uint32_t base, uint32_t vertices) {
    std::vector<Tri> out;
    for (uint32_t i = 0; i + 2 < vertices; ++i) {
        const uint32_t a = base + i, b = base + i + 1, c = base + i + 2;
        out.push_back(canonical(i % 2 ? Tri{b, a, c} : Tri{a, b, c}));
    }
    return out;
}

}  // namespace

TEST(draw_batch_quads_are_the_strips_triangles) {
    // 0.90: quads drawn through the index pattern are the very triangles, facing the same way, their strips drew
    const std::vector<uint16_t> idx = batch::quadIndices(3);
    CHECK(idx.size() == 18);
    CHECK(idx[0] == 0 && idx[1] == 1 && idx[2] == 2 && idx[3] == 2 && idx[4] == 1 && idx[5] == 3);
    CHECK(idx[6] == 4 && idx[11] == 7 && idx[17] == 11);
    for (uint32_t q = 0; q < 3; ++q) {
        const std::vector<Tri> strip = stripTriangles(q * 4, 4);
        CHECK(strip.size() == 2);
        for (size_t t = 0; t < 2; ++t) {
            const Tri listed = canonical({idx[q * 6 + t * 3], idx[q * 6 + t * 3 + 1], idx[q * 6 + t * 3 + 2]});
            CHECK(listed == strip[t]);  // in the same order, too
        }
    }
    const std::vector<uint16_t> most = batch::quadIndices(batch::kMaxQuads);
    CHECK(most.back() == 65535);  // the last quad's last vertex: 16 bits are enough
}

TEST(draw_batch_gathers_draws_one_after_another) {
    batch::Batch b;
    CHECK(!b.open());
    const batch::Draw q0{batch::Kind::Quads, 0, 4, 32}, q1{batch::Kind::Quads, 128, 4, 32};
    CHECK(!b.joins(q0));
    b.start(q0);
    CHECK(b.open() && b.joins(q1));
    b.add(q1);
    CHECK(b.vertices() == 8 && b.draws() == 2 && b.firstByte() == 0);
    CHECK(!b.joins(batch::Draw{batch::Kind::Quads, 512, 4, 32}));  // not right after it
    CHECK(!b.joins(batch::Draw{batch::Kind::List, 256, 6, 32}));   // another kind
    CHECK(!b.joins(batch::Draw{batch::Kind::Quads, 256, 4, 40}));  // the other vertex format
    CHECK(b.joins(batch::Draw{batch::Kind::Quads, 256, 4, 32}));
    b.close();
    CHECK(!b.open());
    // A strip of other than four vertices, the game's own indices: by themselves.
    b.start(batch::Draw{batch::Kind::Single, 0, 6, 32});
    CHECK(!b.joins(batch::Draw{batch::Kind::Single, 192, 6, 32}));
    // Lists go on as long as they come.
    b.start(batch::Draw{batch::Kind::List, 0, 3, 40});
    for (uint32_t i = 1; i < 100000; ++i) {
        const batch::Draw d{batch::Kind::List, i * 120, 3, 40};
        CHECK(b.joins(d));
        b.add(d);
    }
    CHECK(b.vertices() == 300000);
}

TEST(draw_batch_quads_up_to_what_16_bits_index) {
    batch::Batch b;
    b.start(batch::Draw{batch::Kind::Quads, 0, 4, 32});
    for (uint32_t i = 1; i < batch::kMaxQuads; ++i) b.add(batch::Draw{batch::Kind::Quads, i * 128, 4, 32});
    CHECK(b.vertices() == 65536);
    CHECK(!b.joins(batch::Draw{batch::Kind::Quads, batch::kMaxQuads * 128, 4, 32}));
}

TEST(draw_batch_kind_of_a_draw) {
    CHECK(batch::kindOf(d3d7::PT_TRIANGLESTRIP, 4, false) == batch::Kind::Quads);
    CHECK(batch::kindOf(d3d7::PT_TRIANGLESTRIP, 6, false) == batch::Kind::Single);
    CHECK(batch::kindOf(d3d7::PT_TRIANGLELIST, 9, false) == batch::Kind::List);
    CHECK(batch::kindOf(d3d7::PT_TRIANGLELIST, 8, false) == batch::Kind::Single);  // a broken list
    CHECK(batch::kindOf(d3d7::PT_TRIANGLESTRIP, 4, true) == batch::Kind::Single);
    CHECK(batch::kindOf(d3d7::PT_TRIANGLELIST, 6, true) == batch::Kind::Single);
}
