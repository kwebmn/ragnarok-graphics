// 3.0.18-beta.230 (the owner: wings on the characters - concept https://claude.ai/artifact/S1rYPnLYM9yByk6wz5FTok,
// spec docs/superpowers/specs/2026-11-13-wings-design.md): big wings on whoever wears one of nine GM-only headgears,
// drawn from the uaRO HD client's garment sets and their recolours
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "core/sprite_file.hpp"
#include "core/wings.hpp"
#include "test.hpp"

using namespace uagfx;
using wings::Look;

namespace {

bool near(float a, float b) { return std::fabs(a - b) < 0.001f; }

}  // namespace

// 3.0.18-beta.232 (the owner's «давай»: the items' list changed - .230's views past the 2007 client's names showed
// «Cannot find File» to every plain client near the wearer): views the game knows, items only the GM has
TEST(wings_looks_by_view) {
    CHECK(wings::lookOfView(260) == Look::Angel);     // Angel Blessing
    CHECK(wings::lookOfView(233) == Look::Fallen);    // Helm of Darkness
    CHECK(wings::lookOfView(129) == Look::Demon);     // Wings of Diablo and its refined one
    CHECK(wings::lookOfView(221) == Look::Fire);      // Phoenix Crown (both)
    CHECK(wings::lookOfView(301) == Look::Ice);       // Soul Wing, Soul Ring
    CHECK(wings::lookOfView(300) == Look::Valkyrie);  // Valkyrie Feather Band
    CHECK(wings::lookOfView(261) == Look::Gold);      // Emperor's Laurel Crown, Sacred Palma, Emperor Wreath
    CHECK(wings::lookOfView(325) == Look::Sakura);    // Flower Crown
    CHECK(wings::lookOfView(271) == Look::Crimson);   // Red Deviruchi Hat
    for (int old : {365, 382, 613, 595, 568, 477, 438, 562}) CHECK(wings::lookOfView(old) == Look::None);  // .230's
    CHECK(wings::lookOfView(0) == Look::None);
    CHECK(wings::lookOfView(38) == Look::None);  // Angel Wing: anyone may have it, no big wings
    CHECK(wings::lookOfView(130) == Look::None);
    const int views[3] = {0, 221, 260};
    CHECK(wings::lookOfViews(views) == Look::Fire);  // the first of the three with a look
    const int none[3] = {0, 38, 39};
    CHECK(wings::lookOfViews(none) == Look::None);
}

TEST(wings_sets) {
    using wings::Set;
    CHECK(wings::setOf(Look::Angel) == Set::Angel);
    CHECK(wings::setOf(Look::Fallen) == Set::Fallen);
    CHECK(wings::setOf(Look::Demon) == Set::Dark);
    for (Look l : {Look::Fire, Look::Ice, Look::Valkyrie, Look::Gold, Look::Sakura})
        CHECK(wings::setOf(l) == Set::Angel);
    CHECK(wings::setOf(Look::Crimson) == Set::Dark);
    CHECK(std::string(wings::lookName(Look::Crimson)) == "crimson");
}

TEST(wings_recolour) {
    spritefile::Sprite s;
    spritefile::Image im;
    im.w = 4;
    im.h = 1;
    im.argb = {0xFF282828u, 0xFF808080u, 0xFFDCDCDCu, 0x00000000u};  // greys 40, 128, 220 and a see-through one
    s.palette.push_back(im);
    spritefile::Sprite fire = s;
    wings::recolour(fire, Look::Fire);
    CHECK_EQ(fire.palette[0].argb[0], 0xFF5A0A00u);  // the darkest: the first stop (90, 10, 0)
    CHECK_EQ(fire.palette[0].argb[2], 0xFFFFF096u);  // the lightest: the last (255, 240, 150)
    CHECK_EQ(fire.palette[0].argb[3], 0x00000000u);  // see-through stays
    spritefile::Sprite angel = s;
    wings::recolour(angel, Look::Angel);  // the set's own colours
    CHECK(angel.palette[0].argb == s.palette[0].argb);
}

TEST(wings_garment_names) {
    using wings::Set;
    // CP949: 인간족\몸통\ (the bodies' folder), 남 (male), 기사_남 (a knight's body)
    const std::string bodies = "\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xF6\xC5\xEB\\", male = "\xB3\xB2",
                      knight = "\xB1\xE2\xBB\xE7_\xB3\xB2";
    const std::string path = "data\\sprite\\" + bodies + male + "\\" + knight + ".spr";
    CHECK(wings::garmentEntry(Set::Angel, path, "act") == "angel\\" + male + "\\" + knight + ".act");
    CHECK(wings::garmentEntry(Set::Fallen, path, "spr") == "fallen\\" + male + "\\" + knight + ".spr");
    CHECK(wings::garmentEntry(Set::Dark, path, "act") == "dark.act");
    // the game's names whatever their case
    CHECK(wings::garmentEntry(Set::Angel, "DATA\\SPRITE\\" + bodies + male + "\\a_b.SPR", "act") ==
          "angel\\" + male + "\\a_b.act");
    CHECK(wings::garmentEntry(Set::Angel, "data\\sprite\\monster\\poring.spr", "act").empty());  // not a body
    CHECK(wings::garmentEntry(Set::Dark, "data\\sprite\\monster\\poring.spr", "act").empty());
    // 3.0.18-beta.235 (R-TT95RZ: Angel Blessing on, no wings, no line of why): the game's resource keeps its name
    // without data\ («sprite\악세사리\...» in its error boxes) - the body found by its folder wherever it starts
    const std::string relative = "sprite\\" + bodies + male + "\\" + knight + ".spr";
    CHECK(wings::garmentEntry(Set::Angel, relative, "act") == "angel\\" + male + "\\" + knight + ".act");
    CHECK(wings::garmentEntry(Set::Angel, bodies + male + "\\" + knight + ".spr", "act") ==
          "angel\\" + male + "\\" + knight + ".act");
}

// .235: a resource's name as the game's files know it (gamefiles::Reader: "data\...")
TEST(wings_data_paths) {
    CHECK(wings::dataPath("sprite\\a\\b.act") == "data\\sprite\\a\\b.act");
    CHECK(wings::dataPath("data\\sprite\\a\\b.act") == "data\\sprite\\a\\b.act");
    CHECK(wings::dataPath("DATA\\Sprite\\a.act") == "DATA\\Sprite\\a.act");
    CHECK(wings::dataPath("a\\b.act") == "data\\sprite\\a\\b.act");  // a sprite's resource, relative to its folder
    CHECK(wings::dataPath("").empty());
}

namespace {

spritefile::Frame frameOf(std::vector<spritefile::Layer> layers, int ax, int ay) {
    spritefile::Frame f;
    f.layers = std::move(layers);
    f.anchors.push_back({ax, ay});
    return f;
}

spritefile::Layer layerAt(int x, int y, int index) {
    spritefile::Layer l;
    l.x = x;
    l.y = y;
    l.index = index;
    return l;
}

}  // namespace

// The garment's frame on the body as the game puts a part on it: by the body frame's anchor less the garment frame's,
// a layer's picture round its centre, times the character's scale, from its screen point
TEST(wings_quads_by_anchors) {
    spritefile::Act body, garment;
    body.actions = {{frameOf({}, 0, -60)}};
    garment.actions = {{frameOf({layerAt(4, 6, 0)}, 2, -50)}};
    spritefile::Sprite s;
    spritefile::Image im;
    im.w = 20;
    im.h = 10;
    im.argb.assign(200, 0xFFFFFFFFu);
    s.palette.push_back(im);
    const std::vector<wings::Quad> q = wings::quads(garment, body, s, 0, 0, wings::Transform{100, 200, 2});
    CHECK_EQ(int(q.size()), 1);
    CHECK(near(q[0].x0, 84) && near(q[0].x1, 124));  // 100 + ((4 - 2) - 10) * 2, its 20 wide times 2
    CHECK(near(q[0].y0, 182) && near(q[0].y1, 202));  // 200 + ((6 - 10) - 5) * 2
    CHECK_EQ(q[0].image, 0);
    CHECK(!q[0].mirror);
    const std::vector<wings::Quad> later = wings::quads(garment, body, s, 0, 5, wings::Transform{100, 200, 2});
    CHECK_EQ(int(later.size()), 1);  // a frame past the action's: round
}

TEST(wings_quads_mirror_scale) {
    spritefile::Act body, garment;
    body.actions = {{frameOf({}, 0, 0)}};
    spritefile::Layer l = layerAt(0, 0, 0);
    l.mirror = true;
    l.scaleX = 2;
    garment.actions = {{frameOf({l, layerAt(0, 0, -1)}, 0, 0)}};  // a layer with no picture: none
    spritefile::Sprite s;
    spritefile::Image im;
    im.w = 10;
    im.h = 4;
    im.argb.assign(40, 0xFFFFFFFFu);
    s.palette.push_back(im);
    const std::vector<wings::Quad> q = wings::quads(garment, body, s, 0, 0, wings::Transform{0, 0, 1});
    CHECK_EQ(int(q.size()), 1);
    CHECK(q[0].mirror);
    CHECK(near(q[0].x1 - q[0].x0, 20));  // 10 wide, its layer twice as wide
    CHECK(near(q[0].x0, -10));
}

TEST(wings_quads_past_the_actions) {
    spritefile::Act body, garment;
    body.actions = {{frameOf({}, 0, 0)}, {frameOf({}, 0, 0)}};
    garment.actions = {{frameOf({layerAt(0, 0, 0)}, 0, 0)}};
    spritefile::Sprite s;
    spritefile::Image im;
    im.w = im.h = 2;
    im.argb.assign(4, 0xFFFFFFFFu);
    s.palette.push_back(im);
    CHECK(wings::quads(garment, body, s, 1, 0, wings::Transform{}).empty());  // the garment has no such action
    CHECK_EQ(int(wings::quads(garment, body, s, 0, 0, wings::Transform{}).size()), 1);
}

// Facing us the wings are behind the body, facing away over it (sides 3, 4, 5 of the 8)
TEST(wings_faces_away) {
    for (size_t a : {0u, 1u, 2u, 6u, 7u, 8u, 15u}) CHECK(!wings::facesAway(a));
    for (size_t a : {3u, 4u, 5u, 12u, 100u}) CHECK(wings::facesAway(a));
}

// A frame's bookkeeping: a character's faces noted as the game sorts them; its wings drawn right before its first
// face (facing us) or right after its last (facing away); a character with no faces gets none
TEST(wings_frame_front_and_back) {
    wings::Frame f;
    const int front = f.begin(0x1000, false);
    f.face(front, 0xA);
    f.face(front, 0xB);
    const int back = f.begin(0x2000, true);
    f.face(back, 0xC);
    f.face(back, 0xD);
    const int hidden = f.begin(0x3000, false);  // its sprites never came
    (void)hidden;
    f.face(-1, 0xF);  // nobody's
    const wings::Frame::Emit c = f.drawn(0xC);  // each call is one draw
    CHECK(c.before == -1 && c.after == -1);
    CHECK_EQ(f.drawn(0xA).before, front);  // interleaved: each its own
    CHECK_EQ(f.drawn(0xD).after, back);
    CHECK_EQ(f.drawn(0xB).before, -1);
    CHECK_EQ(f.drawn(0xE).before, -1);
    CHECK_EQ(f.drawn(0xF).after, -1);
    f.clear();
    CHECK_EQ(f.drawn(0xA).before, -1);  // a new frame: nothing known
}

// 3.0.18-beta.237 (R-FAFJWS «мигает тело персонажа»): uaRO.exe sorts a player's body and head into one list that
// writes the depth (flags 0x101, FUN_00412020), by their top's rhw - the head now and then first. The wings took the
// first drawn face's depth: the head's, nearer than the body, hid the body under them, and as the order flipped from
// frame to frame the body blinked. Their depth now: a little behind the farthest of the wearer's faces (facing us),
// a little nearer than its nearest (facing away) - whichever is drawn first
namespace {

bool close(float a, float b, float eps) { return std::fabs(a - b) < eps; }

// a sprite's face as the game makes it (FUN_00547E20): TL1 vertices, the top two at one rhw, the bottom two at another,
// z from the rhw
std::vector<uint8_t> faceQuad(float x0, float y0, float x1, float y1, float rhwTop, float rhwBottom) {
    std::vector<uint8_t> v(4 * 32, 0);
    const float xs[4] = {x0, x1, x0, x1}, ys[4] = {y0, y0, y1, y1};
    const float ws[4] = {rhwTop, rhwTop, rhwBottom, rhwBottom};
    for (int i = 0; i < 4; ++i) {
        const float f[4] = {xs[i], ys[i], wings::zOf(ws[i]), ws[i]};
        std::memcpy(v.data() + size_t(i) * 32, f, sizeof f);
    }
    return v;
}

}  // namespace

TEST(wings_depth_of_a_face) {
    const std::vector<uint8_t> v = faceQuad(10, 100, 50, 200, 0.0040f, 0.0050f);
    const wings::Depth d = wings::faceDepth(v.data(), 4);
    CHECK(d.known());
    CHECK(near(d.yTop, 100) && near(d.yBottom, 200));
    CHECK(close(d.rhwTop, 0.0040f, 1e-7f) && close(d.rhwBottom, 0.0050f, 1e-7f));
    CHECK(close(d.at(150), 0.0045f, 1e-7f));
    CHECK(!wings::faceDepth(v.data(), 0).known());  // no vertices
    CHECK(!wings::Depth{}.known());
    CHECK(close(wings::zOf(1.0f / 10.0f), 0.0f, 1e-6f));    // uaRO.exe FUN_00412E30: the near plane's 0
    CHECK(close(wings::zOf(1.0f / 1500.0f), 1.0f, 1e-6f));  // the far one's 1
    const wings::Depth farther = wings::moved(d, 0.5f);  // half a unit farther in view space
    CHECK(close(1.0f / farther.rhwTop, 1.0f / 0.0040f + 0.5f, 1e-3f));
    CHECK(close(1.0f / farther.rhwBottom, 1.0f / 0.0050f + 0.5f, 1e-3f));
    CHECK(near(farther.yTop, 100) && near(farther.yBottom, 200));
}

TEST(wings_depth_behind_every_face) {
    wings::Frame f;
    const int job = f.begin(0x1000, false, 300.0f);  // facing us, its feet at row 300
    // the head (rows 160-220) nearer than the body (rows 200-300) - and drawn first
    const std::vector<uint8_t> head = faceQuad(0, 160, 40, 220, 0.00340f, 0.00341f);
    const std::vector<uint8_t> body = faceQuad(0, 200, 40, 300, 0.00335f, 0.00336f);
    f.face(job, 0xA, wings::faceDepth(head.data(), 4));
    f.face(job, 0xB, wings::faceDepth(body.data(), 4));
    const wings::Frame::Emit e = f.drawn(0xA);
    CHECK_EQ(e.before, job);  // right before the first drawn, as before
    CHECK(e.depth.known());
    const wings::Depth b = wings::faceDepth(body.data(), 4);
    for (const float y : {160.0f, 220.0f, 260.0f, 300.0f}) CHECK(e.depth.at(y) < b.at(y));  // behind the body
    const float behind = 1.0f / e.depth.at(250) - 1.0f / b.at(250);
    CHECK(behind > 0.1f && behind < 1.0f);  // a little: not into the ground behind its feet
    CHECK(!f.drawn(0xB).depth.known());     // its other faces: nothing to draw
}

TEST(wings_depth_over_every_face) {
    wings::Frame f;
    const int job = f.begin(0x1000, true, 300.0f);  // facing away: over everything of it
    const std::vector<uint8_t> body = faceQuad(0, 200, 40, 300, 0.00335f, 0.00336f);
    const std::vector<uint8_t> head = faceQuad(0, 160, 40, 220, 0.00340f, 0.00341f);
    f.face(job, 0xB, wings::faceDepth(body.data(), 4));
    f.face(job, 0xA, wings::faceDepth(head.data(), 4));
    CHECK_EQ(f.drawn(0xA).after, -1);
    const wings::Frame::Emit e = f.drawn(0xB);  // the body drawn last
    CHECK_EQ(e.after, job);
    const wings::Depth h = wings::faceDepth(head.data(), 4);
    for (const float y : {160.0f, 220.0f, 300.0f}) CHECK(e.depth.at(y) > h.at(y));  // nearer than the head
}

TEST(wings_depth_of_faces_drawn_elsewhere) {
    // a layer the game sorts into its opaque list (flags without bit 0, drawn before the ground): its depth counts,
    // the wings never go to its draw
    wings::Frame f;
    const int job = f.begin(0x1000, false, 300.0f);
    const std::vector<uint8_t> layer = faceQuad(0, 200, 40, 300, 0.00330f, 0.00331f);  // the farthest
    const std::vector<uint8_t> body = faceQuad(0, 200, 40, 300, 0.00335f, 0.00336f);
    f.face(job, 0xC, wings::faceDepth(layer.data(), 4), false);
    f.face(job, 0xB, wings::faceDepth(body.data(), 4));
    CHECK_EQ(f.drawn(0xC).before, -1);
    const wings::Frame::Emit e = f.drawn(0xB);
    CHECK_EQ(e.before, job);
    CHECK(e.depth.at(250) < wings::faceDepth(layer.data(), 4).at(250));  // behind that one too
    wings::Frame g;  // faces with no depth known: none told (the draw's own face then)
    const int other = g.begin(0x2000, false);
    g.face(other, 0xD);
    CHECK(!g.drawn(0xD).depth.known());
}

TEST(wings_lerp) {
    CHECK(near(wings::lerpAt(15, 10, 0.5f, 20, 1.0f), 0.75f));
    CHECK(near(wings::lerpAt(30, 10, 0.5f, 20, 1.0f), 1.5f));  // past the face: on along the same line
    CHECK(near(wings::lerpAt(15, 10, 0.5f, 10, 1.0f), 0.5f));  // a flat face: its own
}

// 3.0.18-beta.231 (the owner's «Cannot find File : sprite\악세사리\여\여검사.act»): the 2007 client's headgear names
// end near view 330; a view past them read another table's word ("검사", a job's) - its sprite asked for, an error box
// for each file. Such a view gets no picture (FUN_0062E970 / FUN_0062EA20 asked for none)
TEST(wings_views_past_the_table) {
    const uint32_t first = 0x1000, last = first + 4 * 327;  // 327 names: views 0..326
    CHECK(!wings::pastTable(1, first, last));
    CHECK(!wings::pastTable(326, first, last));
    CHECK(wings::pastTable(327, first, last));
    CHECK(wings::pastTable(365, first, last));  // Wings Of Victory's
    CHECK(!wings::pastTable(0, first, last));   // none worn: the game's own way
    CHECK(wings::pastTable(5, 0, 0));           // no table: nothing to name it by
    CHECK(wings::pastTable(5, first, first - 4));  // a broken one
}

// 3.0.18-beta.236 (R-4U3RD7: «the body's sprite\인간족\몸통\여\궁수_여.spr not read» - the list taken for the acts held
// the sprites): the body's act named after its sprite
TEST(wings_body_act_of_its_sprite) {
    CHECK(wings::actOf("sprite\\a\\b.spr") == "sprite\\a\\b.act");
    CHECK(wings::actOf("sprite\\a\\b.SPR") == "sprite\\a\\b.act");
    CHECK(wings::actOf("x.act") == "x.act");
    CHECK(wings::actOf("sprite\\a.b\\c").empty());  // no extension of its own
    CHECK(wings::actOf("").empty());
}
