// 3.0.20-beta.20 (the owner's «делай 1 и 2», 1 - the long frames' dearest part: textures' uploads, 16-27 ms; a picture's
// new textures each read six or seven times on the renderer's thread): a whole texture's pixels kept for the work on
// them after its upload - in the order they came, a newer upload of the same texture in place of the older one's, a
// texture let go taking its work with it, so much kept at most
#include <vector>

#include "core/upload_later.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

UploadCmd wholeOf(uint32_t texture, int w, int h) {
    UploadCmd u;
    u.texture = texture;
    u.rect = Rect{0, 0, w, h};
    u.pixels = 40;  // an offset into the record's pixels: the job's own from 0
    u.fullFrom = 3;
    return u;
}

}  // namespace

TEST(upload_later_keeps_the_order_and_the_newest) {
    uploadlater::Queue q;
    const std::vector<uint32_t> a(4, 0xFF0000FFu), b(4, 0xFF00FF00u), c(4, 0xFFFF0000u);
    CHECK(q.push(wholeOf(1, 2, 2), a.data(), false));
    CHECK(q.push(wholeOf(2, 2, 2), b.data(), true));
    CHECK(q.push(wholeOf(1, 2, 2), c.data(), false));  // texture 1 again: its older work gone
    CHECK(q.size() == 2 && q.bytes() == 2 * 4 * 4);
    uploadlater::Job j;
    CHECK(q.take(j) && j.upload.texture == 2 && j.spriteCache && j.pixels == b);
    CHECK(j.upload.pixels == 0 && j.upload.fullFrom == 3 && j.upload.rect.right == 2);
    CHECK(q.take(j) && j.upload.texture == 1 && j.pixels == c);
    CHECK(!q.take(j) && q.size() == 0 && q.bytes() == 0);
}

TEST(upload_later_lets_go_and_holds_so_much) {
    uploadlater::Queue q;
    const std::vector<uint32_t> px(16, 1u);
    CHECK(q.push(wholeOf(5, 4, 4), px.data(), false));
    q.drop(5);  // the texture let go
    uploadlater::Job j;
    CHECK(!q.take(j) && q.bytes() == 0);
    const int side = 2048;  // 16 MB a picture: four fill the queue
    const std::vector<uint32_t> big(size_t(side) * side, 7u);
    int kept = 0;
    for (uint32_t t = 10; t < 20; ++t) kept += q.push(wholeOf(t, side, side), big.data(), false) ? 1 : 0;
    CHECK(kept == int(uploadlater::kMostBytes / (size_t(side) * side * 4)) && q.bytes() <= uploadlater::kMostBytes);
}
