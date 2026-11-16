// 3.0.20-beta.20 (the owner's «делай 1 и 2», 1 - the long frames' dearest part: textures' uploads, 16-27 ms; a picture's
// new textures each read six or seven times on the renderer's thread): a whole texture's pixels kept for the work on
// them after its upload - in the order they came, a newer upload of the same texture in place of the older one's, a
// texture let go taking its work with it, so much kept at most
#include <vector>

#include "core/ai/texture_cache.hpp"
#include "core/effect_light.hpp"
#include "core/foliage.hpp"
#include "core/full_colour.hpp"
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

// 3.0.20-beta.36 (the owner's «6,7,8,9», 9 - the frame's work on more threads): a job carries its upload's serial - what
// a helper thread made of older pixels is not taken once a newer upload of the texture came
TEST(upload_later_carries_its_uploads_serial) {
    uploadlater::Queue q;
    const std::vector<uint32_t> a(4, 0xFF0000FFu), b(4, 0xFF00FF00u);
    CHECK(q.push(wholeOf(1, 2, 2), a.data(), false, 7));
    CHECK(q.push(wholeOf(1, 2, 2), b.data(), false, 9));  // the newer one in its place
    uploadlater::Job j;
    CHECK(q.take(j) && j.serial == 9 && j.pixels == b);
}

// its looks and the AI pack's key worked out from the job alone (a helper thread's), as the renderers worked them out
// themselves: the same functions over its pixels; the pack's view of a 16-bit texture's full colours its texels
TEST(upload_later_looks_as_the_textures_own_functions_give_them) {
    std::vector<uint32_t> px(256);  // 16 x 16: green leaves with clear holes, an orange glow along the top
    for (int i = 0; i < 256; ++i) px[size_t(i)] = i % 5 == 0 ? 0x00000000u : i < 40 ? 0xFFFF8020u : 0xFF208030u;
    UploadCmd u = wholeOf(3, 16, 16);
    u.fullFrom = 0;
    uploadlater::Queue q;
    CHECK(q.push(u, px.data(), false, 5));
    uploadlater::Job j;
    CHECK(q.take(j));
    uploadlater::Looks l;
    uploadlater::lookAt(std::move(j), true, l);
    float fire[3] = {}, light[3] = {};
    const float fireAmount = effect_light::fireOf(px.data(), 16, 16, fire);
    const float lightAmount = effect_light::lightOf(px.data(), 16, 16, light);
    CHECK(l.texture == 3 && l.serial == 5 && l.width == 16 && l.height == 16 && !l.spriteCache);
    CHECK(l.fire == fireAmount && l.fireColour[0] == fire[0] && l.fireColour[2] == fire[2]);
    CHECK(l.light == lightAmount && l.lightColour[1] == light[1]);
    CHECK(l.leafy == foliage::leafyTexture(px.data(), 16, 16, false));
    CHECK(l.pixels == px && l.game.empty() && l.texels() == l.pixels.data());
    CHECK(l.aiKey == ai::keyName(ai::textureKey(px.data(), 16, 16)));
    u.fullFrom = uint8_t(PixelFormat::A4R4G4B4);  // the game's 16 bits: the pack knows it by those texels
    CHECK(q.push(u, px.data(), false, 6) && q.take(j));
    uploadlater::lookAt(std::move(j), true, l);
    std::vector<uint32_t> view;
    full_colour::gameView(PixelFormat::A4R4G4B4, px.data(), px.size(), view);
    CHECK(l.serial == 6 && l.game == view && l.texels() == l.game.data());
    CHECK(l.aiKey == ai::keyName(ai::textureKey(view.data(), 16, 16)));
    CHECK(q.push(u, px.data(), false, 8) && q.take(j));
    uploadlater::lookAt(std::move(j), false, l);  // no AI pack: no view, no key
    CHECK(l.serial == 8 && l.game.empty() && l.aiKey.empty() && l.pixels == px);
}

// 3.0.20-beta.45 (the audit, from a tester's log: «1 waiting» in every line - a texture the game writes every frame):
// the looks' thread answers a picture later, and by then the texture was written again - beta.36 took only the looks
// of its newest upload, so such a texture never got any (its fire, its light, its leaves, its AI key). The looks of
// any upload of this texture are taken - one write behind at worst; those of the texture that had this number before
// it (let go, the number given again) are not
TEST(upload_later_takes_the_looks_of_an_upload_a_write_behind) {
    CHECK(uploadlater::takes(5, 9, 9));    // its newest
    CHECK(uploadlater::takes(5, 9, 8));    // a write behind: still this texture's
    CHECK(uploadlater::takes(5, 9, 5));    // its first
    CHECK(!uploadlater::takes(5, 9, 4));   // the texture before it under the same number
    CHECK(!uploadlater::takes(5, 9, 10));  // none such yet
    CHECK(!uploadlater::takes(0, 0, 0));   // never written whole
    CHECK(!uploadlater::takes(0, 0, 3));
}
