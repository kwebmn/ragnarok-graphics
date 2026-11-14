// 3.0.11-beta.14: the client's sprite files (core/sprite_file) - .spr pictures (palette with RLE, true colour), .act
// frames of layers - and the monsters' sprite names (refindex::spriteNames) for «Довідник»'s pictures.
#include <algorithm>
#include <cstring>
#include <string>

#include "core/reference_index.hpp"
#include "core/sprite_file.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

void put(std::string& s, uint32_t v, int bytes) {
    for (int i = 0; i < bytes; ++i) s.push_back(char((v >> (8 * i)) & 0xFF));
}

// SP 2.1: one palette picture 3 x 2 (0 1 2 / 2 0 1, packed), one true colour 1 x 1 (red); palette 1 red, 2 green
std::string spr() {
    std::string s = "SP";
    put(s, 1, 1);  // minor
    put(s, 2, 1);  // major
    put(s, 1, 2);
    put(s, 1, 2);
    put(s, 3, 2);
    put(s, 2, 2);
    const unsigned char rle[] = {0, 1, 1, 2, 2, 0, 1, 1};
    put(s, sizeof rle, 2);
    s.append(reinterpret_cast<const char*>(rle), sizeof rle);
    put(s, 1, 2);
    put(s, 1, 2);
    const unsigned char abgr[] = {255, 0, 0, 255};
    s.append(reinterpret_cast<const char*>(abgr), 4);
    std::string pal(1024, '\0');
    pal[4] = char(255);      // 1: red
    pal[2 * 4 + 1] = char(255);  // 2: green
    return s + pal;
}

// AC 2.5: one action, one frame, `layers` layers at (x, 0), the first mirrored when asked; an anchor when ax/ay given
std::string act(int layers, bool mirror, int x2 = 0, bool anchor = false, int ax = 0, int ay = 0) {
    std::string s = "AC";
    put(s, 5, 1);
    put(s, 2, 1);
    put(s, 1, 2);
    s.append(10, '\0');
    put(s, 1, 4);  // frames
    s.append(32, '\0');
    put(s, uint32_t(layers), 4);
    for (int l = 0; l < layers; ++l) {
        put(s, uint32_t(l == 0 ? 0 : x2), 4);
        put(s, 0, 4);
        put(s, 0, 4);  // the palette picture
        put(s, l == 0 && mirror ? 1 : 0, 4);
        put(s, 0xFFFFFFFF, 4);  // white, opaque: no tint
        float one = 1;
        uint32_t f = 0;
        std::memcpy(&f, &one, 4);
        put(s, f, 4);
        put(s, f, 4);
        put(s, 0, 4);  // rotation
        put(s, 0, 4);  // palette type
        put(s, 3, 4);
        put(s, 2, 4);
    }
    put(s, 0xFFFFFFFF, 4);  // no sound
    put(s, anchor ? 1 : 0, 4);
    if (anchor) {  // 3.0.18-beta.111: an anchor - 4 unknown bytes, x, y, 4 more
        put(s, 0, 4);
        put(s, uint32_t(ax), 4);
        put(s, uint32_t(ay), 4);
        put(s, 0, 4);
    }
    return s;
}

}  // namespace

TEST(sprite_file_reads_pictures_and_frames) {
    spritefile::Sprite sp;
    CHECK(spritefile::readSpr(spr(), sp));
    CHECK_EQ(sp.palette.size(), size_t(1));
    CHECK_EQ(sp.truecolour.size(), size_t(1));
    const spritefile::Image& p = sp.palette[0];
    CHECK(p.w == 3 && p.h == 2);
    CHECK_EQ(p.argb[0], 0u);            // index 0: see-through
    CHECK_EQ(p.argb[1], 0xFFFF0000u);   // red
    CHECK_EQ(p.argb[2], 0xFF00FF00u);   // green
    CHECK_EQ(p.argb[4], 0u);
    CHECK_EQ(sp.truecolour[0].argb[0], 0xFFFF0000u);
    CHECK(!spritefile::readSpr("SP", sp));
    CHECK(!spritefile::readSpr(spr().substr(0, 20), sp));

    spritefile::Act a;
    CHECK(spritefile::readAct(act(1, false), a));
    CHECK_EQ(a.actions.size(), size_t(1));
    CHECK_EQ(a.actions[0][0].layers.size(), size_t(1));
    spritefile::Image im;
    CHECK(spritefile::frameImage(sp, a, 0, 0, im));
    CHECK(im.w == 3 && im.h == 2);
    CHECK_EQ(im.argb[1], 0xFFFF0000u);
    CHECK(spritefile::readAct(act(1, true), a));  // mirrored: the row read right to left
    CHECK(spritefile::frameImage(sp, a, 0, 0, im));
    CHECK_EQ(im.argb[0], 0xFF00FF00u);
    CHECK_EQ(im.argb[2], 0u);
    CHECK(spritefile::readAct(act(2, false, 3), a));  // two layers side by side: 6 wide
    CHECK(spritefile::frameImage(sp, a, 0, 0, im));
    CHECK_EQ(im.w, 6);
    CHECK_EQ(im.argb[4], 0xFFFF0000u);
    CHECK(!spritefile::frameImage(sp, a, 1, 0, im));  // no such action
    CHECK(!spritefile::readAct("AC\x05\x02", a));

    const spritefile::Image big = spritefile::fitted(im, 18, 18);  // grown by whole steps, at most 3 times
    CHECK(big.w == 18 && big.h == 18);
    CHECK_EQ(big.argb[size_t(6 * 18 + 3)], 0xFFFF0000u);  // red, sharp (premultiplied: opaque as is)
    const spritefile::Image small = spritefile::fitted(im, 3, 3);  // shrunk: averaged
    CHECK(small.w == 3 && (small.argb[size_t(1 * 3 + 0)] >> 24) > 0);
}

TEST(reference_sprite_names_of_a_monster) {
    refindex::Mob m;
    m.name = "Fabre";
    m.sprite = "fabre_";
    CHECK(refindex::spriteNames(m) == std::vector<std::string>({"fabre_", "fabre"}));
    m.name = "Desert Wolf";
    m.sprite = "m_desert_wolf";
    CHECK(refindex::spriteNames(m) == std::vector<std::string>({"m_desert_wolf", "desert_wolf"}));
    m.name = "Archer Skeleton";
    m.sprite = "";  // a pack before 3.0.11-beta.14
    CHECK(refindex::spriteNames(m) == std::vector<std::string>({"archer_skeleton", "skel_archer"}));
    m.name = "Goblin (Dagger)";
    CHECK(refindex::spriteNames(m) == std::vector<std::string>({"goblin_dagger"}));
}

// 3.0.19-beta.1 (the testers' doc 14.11: pictures missing in «Довідник» - players' logs «no sprite for Egnigem Cenia»
// and 132 more): the client's own files of the server's variants - an MVP's «b_», an instance's «i_», «a_», «r_», «p_»
// (kept with its «_»: r_baphomet_ is Baphomet Jr., baphomet_), the client's «skel_archer» of «archer_skeleton», the
// first of kobold_1..5 for «l_kobold»
TEST(reference_sprite_names_the_clients_own_files) {
    auto has = [](const refindex::Mob& m, const std::string& name) {
        const std::vector<std::string> n = refindex::spriteNames(m);
        return std::find(n.begin(), n.end(), name) != n.end();
    };
    auto before = [](const refindex::Mob& m, const std::string& a, const std::string& b) {
        const std::vector<std::string> n = refindex::spriteNames(m);
        const auto ia = std::find(n.begin(), n.end(), a), ib = std::find(n.begin(), n.end(), b);
        return ia != n.end() && ib != n.end() && ia < ib;
    };
    refindex::Mob m;
    m.name = "Egnigem Cenia";
    m.sprite = "b_ygnizem";
    CHECK(has(m, "ygnizem"));
    m.name = "Lord Knight Seyren";
    m.sprite = "b_seyren_";
    CHECK(has(m, "seyren"));
    m.name = "Baphomet Jr.";
    m.sprite = "r_baphomet_";
    CHECK(before(m, "baphomet_", "baphomet"));
    m.name = "Archer Skeleton";
    m.sprite = "g_archer_skeleton";
    CHECK(has(m, "skel_archer"));
    m.name = "Kobold";
    m.sprite = "l_kobold";
    CHECK(has(m, "kobold_1"));
    m.name = "Safeguard Chief";
    m.sprite = "i_high_orc";
    CHECK(has(m, "high_orc"));
    m.name = "New Year Doll";
    m.sprite = "p_chung_e";
    CHECK(has(m, "chung_e"));
    m.name = "Gangster";
    m.sprite = "a_mobster";
    CHECK(has(m, "mobster"));
    m.name = "Crystal";
    m.sprite = "crystal_6";
    CHECK(!has(m, "crystal_1"));  // another crystal's colour: never a guess by its name
}

// 3.0.19-beta.2 (the owner: «да есть в игре такие монстры, посмотри в чем проблема»): the game's own name of a
// monster's sprite (uaRO.exe's job name table - kRO's names: CHOCHO for the server's CHONCHON, Vorit for VERIT,
// ATROS for ATROCE) as a file's name; a 3D model's (the Emperium's Empelium90_0.gr2) or no name - none
TEST(reference_sprite_name_from_the_games_table) {
    CHECK_EQ(refindex::gameSpriteName("CHOCHO"), std::string("chocho"));
    CHECK_EQ(refindex::gameSpriteName("Steel_chocho"), std::string("steel_chocho"));
    CHECK_EQ(refindex::gameSpriteName("Vorit"), std::string("vorit"));
    CHECK_EQ(refindex::gameSpriteName("Empelium90_0.gr2"), std::string());
    CHECK_EQ(refindex::gameSpriteName("TREASUREBOX_2.gr2"), std::string());
    CHECK_EQ(refindex::gameSpriteName(""), std::string());
    CHECK_EQ(refindex::gameSpriteName("bad\\name"), std::string());
    CHECK_EQ(refindex::gameSpriteName("\x01\x02"), std::string());
    refindex::Mob m;
    m.name = "Chonchon";
    m.sprite = "chonchon";
    const std::vector<std::string> n = refindex::spriteNames(m, "CHOCHO");  // the game's first
    CHECK(!n.empty() && n.front() == "chocho");
    CHECK(std::find(n.begin(), n.end(), "chonchon") != n.end());
}

// 3.0.11-beta.76 (the owner: «иконки ... и всего остального ... грузить в текстур которые обработаны»): a frame put
// together 4 times bigger - a palette picture from the AI pack's frame when there is one, else stretched
TEST(sprite_file_frame_bigger) {
    spritefile::Sprite sp;
    CHECK(spritefile::readSpr(spr(), sp));
    spritefile::Act a;
    CHECK(spritefile::readAct(act(1, false), a));
    spritefile::Image big;  // the pack's picture of palette picture 0: 12 x 8, blue left, white right
    big.w = 12;
    big.h = 8;
    big.argb.assign(96, 0xFF0000FFu);
    for (int y = 0; y < 8; ++y)
        for (int x = 6; x < 12; ++x) big.argb[size_t(y * 12 + x)] = 0xFFFFFFFFu;
    spritefile::Image im;
    CHECK(spritefile::frameImageScaled(sp, a, 0, 0, 4, [&](int i) { return i == 0 ? &big : nullptr; }, im));
    CHECK(im.w == 12 && im.h == 8);
    CHECK_EQ(im.argb[size_t(3 * 12 + 2)], 0xFF0000FFu);
    CHECK_EQ(im.argb[size_t(3 * 12 + 9)], 0xFFFFFFFFu);
    CHECK(spritefile::readAct(act(1, true), a));  // mirrored: white on the left now
    CHECK(spritefile::frameImageScaled(sp, a, 0, 0, 4, [&](int i) { return i == 0 ? &big : nullptr; }, im));
    CHECK_EQ(im.argb[size_t(3 * 12 + 2)], 0xFFFFFFFFu);
    // none from the pack: the frame's own, 4 times (red at x 1: 4..7)
    CHECK(spritefile::readAct(act(1, false), a));
    CHECK(spritefile::frameImageScaled(sp, a, 0, 0, 4, [](int) { return nullptr; }, im));
    CHECK(im.w == 12 && im.h == 8);
    CHECK_EQ(im.argb[size_t(1 * 12 + 5)], 0xFFFF0000u);
    CHECK_EQ(im.argb[size_t(1 * 12 + 1)], 0u);
}

// 3.0.11-beta.196 (the tester: «треба додати портрет кафри якийсь як іконку»): a face from an NPC's whole picture -
// the square at its top, as wide as the picture or `share` of its height (the narrower), in its middle
TEST(sprite_file_top_square) {
    spritefile::Image tall;  // 40 x 100: the whole width, the top 40 rows
    tall.w = 40;
    tall.h = 100;
    tall.argb.resize(4000);
    for (int y = 0; y < 100; ++y)
        for (int x = 0; x < 40; ++x) tall.argb[size_t(y * 40 + x)] = 0xFF000000u | uint32_t(y << 8) | uint32_t(x);
    const spritefile::Image a = spritefile::topSquare(tall, 0.42f);
    CHECK(a.w == 40 && a.h == 40 && a.argb.size() == 1600);
    CHECK(a.argb[0] == tall.argb[0] && a.argb[size_t(39 * 40 + 39)] == tall.argb[size_t(39 * 40 + 39)]);
    spritefile::Image wide = tall;  // 80 x 100: 42 wide (0.42 of its height), from x 19
    wide.w = 80;
    wide.argb.assign(8000, 0);
    for (int y = 0; y < 100; ++y)
        for (int x = 0; x < 80; ++x) wide.argb[size_t(y * 80 + x)] = 0xFF000000u | uint32_t(y << 8) | uint32_t(x);
    const spritefile::Image b = spritefile::topSquare(wide, 0.42f);
    CHECK(b.w == 42 && b.h == 42);
    CHECK(b.argb[0] == wide.argb[19] && b.argb[size_t(41 * 42 + 41)] == wide.argb[size_t(41 * 80 + 60)]);
    CHECK(spritefile::topSquare(spritefile::Image{}, 0.42f).w == 0);
}

// 3.0.18-beta.111 (the owner: the character in «Спорядження» from the game's sprites): the anchors read; parts put
// together as the game does - each part moved by the body's anchor less its own (the head on the neck)
TEST(sprite_file_anchors_put_parts_together) {
    spritefile::Sprite sp;
    CHECK(spritefile::readSpr(spr(), sp));
    spritefile::Act body, head;
    CHECK(spritefile::readAct(act(1, false, 0, true, 0, -10), body));
    CHECK(spritefile::readAct(act(1, false, 0, true, 0, 0), head));
    CHECK_EQ(body.actions[0][0].anchors.size(), size_t(1));
    CHECK_EQ(body.actions[0][0].anchors[0].second, -10);
    std::vector<spritefile::Part> parts{{&sp, &body, 0, 0}, {&sp, &head, 0, 0}};
    spritefile::Image out;
    int footX = 0, footY = 0;
    CHECK(spritefile::compose(parts, out, footX, footY));
    CHECK_EQ(out.w, 3);
    CHECK_EQ(out.h, 12);  // the body (2 high) and the head 10 above it
    CHECK(footY > 9);    // the feet at the body's centre, low in the picture
    CHECK((out.argb[size_t(1)] >> 24) != 0);  // the head's top row drawn at the picture's top
}

// 3.0.18-beta.114 (the owner: «картинка взята не с апскейленой версии»): parts put together `factor` times bigger -
// each layer's picture from its part's `bigger` (the AI pack's frame) when given, its place and the anchors scaled
TEST(sprite_file_compose_scaled_takes_bigger_frames) {
    spritefile::Sprite sp;
    CHECK(spritefile::readSpr(spr(), sp));
    spritefile::Act body, head;
    CHECK(spritefile::readAct(act(1, false, 0, true, 0, -10), body));
    CHECK(spritefile::readAct(act(1, false, 0, true, 0, 0), head));
    spritefile::Image big;  // the frame 4x, all of it one colour
    big.w = sp.palette[0].w * 4;
    big.h = sp.palette[0].h * 4;
    big.argb.assign(size_t(big.w) * size_t(big.h), 0xFF00FF00u);
    std::vector<spritefile::Part> parts{{&sp, &body, 0, 0}, {&sp, &head, 0, 0}};
    parts[0].bigger = [&](int) -> const spritefile::Image* { return &big; };
    spritefile::Image one, four;
    int fx = 0, fy = 0, fx4 = 0, fy4 = 0;
    CHECK(spritefile::compose(parts, one, fx, fy));
    CHECK(spritefile::compose(parts, four, fx4, fy4, 4));
    CHECK_EQ(four.w, one.w * 4);
    CHECK_EQ(four.h, one.h * 4);
    CHECK_EQ(fy4, fy * 4);
    CHECK_EQ(four.argb[size_t(four.h - 1) * size_t(four.w) + size_t(four.w / 2)], 0xFF00FF00u);  // the body: the pack's
}

// 3.0.18-beta.115 (R-RJBU55 «спрайт маленький»): a picture cut to what shows (alpha past `least`) - the parts' empty
// room round it never shrinks the character in its box
TEST(sprite_file_cut_to_what_shows) {
    spritefile::Image im;
    im.w = 10;
    im.h = 8;
    im.argb.assign(80, 0);
    im.argb[size_t(2 * 10 + 3)] = 0xFF112233u;
    im.argb[size_t(5 * 10 + 6)] = 0xFF445566u;
    im.argb[size_t(7 * 10 + 9)] = 0x05000000u;  // all but clear: left out
    const spritefile::Image c = spritefile::cutToShown(im, 8);
    CHECK_EQ(c.w, 4);
    CHECK_EQ(c.h, 4);
    CHECK_EQ(c.argb[0], 0xFF112233u);
    CHECK_EQ(c.argb[size_t(3 * 4 + 3)], 0xFF445566u);
    CHECK_EQ(spritefile::cutToShown(spritefile::Image{}, 8).w, 0);
}

// 3.0.18-beta.126 (item 3: «Спорядження»'s character): a job's .imf (data\imf\, FUN_0062e920 "%s%s.imf") - version
// 1.01, a checksum, the last layer's number, each layer's actions' frames' priority and a point
TEST(sprite_file_imf_priorities) {
    std::string b;
    const auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(char(v >> (8 * i))); };
    const float version = 1.01f;
    uint32_t vbits = 0;
    std::memcpy(&vbits, &version, 4);
    u32(vbits);
    u32(0);
    u32(1);  // layers 0 and 1
    for (int layer = 0; layer < 2; ++layer) {
        u32(2);  // two actions
        for (int a = 0; a < 2; ++a) {
            u32(1);  // one frame
            u32(layer == 0 && a == 1 ? 1 : 0);
            u32(0);
            u32(0);
        }
    }
    spritefile::Imf imf;
    CHECK(spritefile::readImf(b, imf));
    CHECK_EQ(spritefile::imfPriority(imf, 0, 1, 0), 1);
    CHECK_EQ(spritefile::imfPriority(imf, 1, 1, 0), 0);
    CHECK_EQ(spritefile::imfPriority(imf, 0, 0, 0), 0);
    CHECK_EQ(spritefile::imfPriority(imf, 0, 5, 0), -1);  // past its actions: none
    CHECK(!spritefile::readImf("xx", imf));
}

// 3.0.18-beta.227 (R-UC9NT7 «тіло "пустою шиєю" перекриває "рот" персонажа», the novice, the rogue): the world
// (uaRO.exe FUN_005ae540) draws the layer of priority 1 first and the one of 0 over it (FUN_0042e430 finds the layer by
// its priority) - the usual standing frame (the body 1, the head 0) has the head over the body; .126 had it the other
// way round
TEST(sprite_file_imf_body_over_head) {
    spritefile::Imf imf;
    imf.priority = {{{1}, {0}}, {{0}, {1}}};  // [layer][action][frame]: action 0 the usual, action 1 the body over
    CHECK(!spritefile::bodyOverHead(imf, 0, 0));
    CHECK(spritefile::bodyOverHead(imf, 1, 0));
    CHECK(!spritefile::bodyOverHead(imf, 2, 0));  // no such action: the usual order
    CHECK(!spritefile::bodyOverHead(spritefile::Imf{}, 0, 0));  // no .imf
    spritefile::Imf same;
    same.priority = {{{0}}, {{0}}};
    CHECK(!spritefile::bodyOverHead(same, 0, 0));
}

// .227: the order of the character's parts under to over - with the body over the head the low headgear goes between
// them, as the world does it (the body's turn given to layer 2, the body drawn in its place)
TEST(sprite_file_hero_layers) {
    using L = spritefile::HeroLayer;
    CHECK(spritefile::heroLayers(false) ==
          (std::vector<L>{L::Cart, L::Body, L::Head, L::Low, L::Middle, L::Top, L::Weapon, L::Shield, L::Falcon}));
    CHECK(spritefile::heroLayers(true) ==
          (std::vector<L>{L::Cart, L::Head, L::Low, L::Body, L::Middle, L::Top, L::Weapon, L::Shield, L::Falcon}));
}
