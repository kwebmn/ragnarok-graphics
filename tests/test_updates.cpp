// 3.0.2-beta.5: the client's graphics updates - the admin's GRFs in <client>\updates\ over the game's archives
// (core/updates): how a name is looked up, which archive wins.
#include <string>
#include <vector>

#include "core/updates.hpp"
#include "test.hpp"

using namespace uagfx;

// As the game's names are compared: ASCII letters in either case, '/' or '\'; a CP949 character's second byte (it
// may be an ASCII letter's code) kept as it is
TEST(updates_look_names_up_as_the_game) {
    CHECK_EQ(updates::lookupKey("Data/Texture/ABC.bmp"), std::string("data\\texture\\abc.bmp"));
    CHECK_EQ(updates::lookupKey("data\\sprite\\\xc0\xaf\xc0\xfa\\Poring.SPR"),
             std::string("data\\sprite\\\xc0\xaf\xc0\xfa\\poring.spr"));
    CHECK_EQ(updates::lookupKey("\x81\x41" "B"), std::string("\x81\x41" "b"));  // 'A' a second byte: kept
    CHECK_EQ(updates::lookupKey(""), std::string(""));
    CHECK_EQ(updates::lookupKey("\x81"), std::string("\x81"));  // a first byte at the end: as it is
}

// The folder's GRFs, the later name first (it wins); anything but a GRF left out
TEST(updates_order_the_folder_later_first) {
    const std::vector<std::string> order =
        updates::lookupOrder({"2026-09-01.grf", "readme.txt", "2026-10-25.GRF", "notes", ".grf", "base.grf"});
    CHECK(order.size() == 3);
    if (order.size() == 3) {
        CHECK_EQ(order[0], std::string("base.grf"));  // by name: a letter after the dates
        CHECK_EQ(order[1], std::string("2026-10-25.GRF"));
        CHECK_EQ(order[2], std::string("2026-09-01.grf"));
    }
}

// The first archive added (the later update) wins a name; any case, either slash
TEST(updates_index_finds_the_winner) {
    updates::Index index;
    index.add(0, {"data\\a.bmp", "data\\B.bmp"});
    index.add(1, {"DATA\\A.BMP", "data\\c.bmp"});
    CHECK_EQ(index.size(), size_t(3));
    const updates::Hit* a = index.find("Data/a.BMP");
    CHECK(a && a->archive == 0 && a->name == "data\\a.bmp");
    const updates::Hit* c = index.find("data\\C.bmp");
    CHECK(c && c->archive == 1 && c->name == "data\\c.bmp");
    CHECK(index.find("data\\b.bmp") && index.find("data\\b.bmp")->name == "data\\B.bmp");
    CHECK(index.find("data\\x.bmp") == nullptr);
}

// The driver's own readers (its map loader, the pack builder): updates' first, then DATA.INI's in their order
TEST(updates_go_before_the_games_archives) {
    const std::vector<std::string> all =
        updates::archiveOrder({"2026-09-01.grf", "2026-10-25.grf"}, {"uaro.grf", "Palettes.grf", "data.grf"});
    const std::vector<std::string> want = {"updates\\2026-10-25.grf", "updates\\2026-09-01.grf", "uaro.grf",
                                           "Palettes.grf", "data.grf"};
    CHECK(all == want);
}

// 3.0.18-beta.251 (R-NGSWJM, the castles' crash «не можу попасти в замок», R-FPT4KM): the game's archives read one at
// a time whatever the updates are - the owner had no updates\ GRF, nothing was hooked, and a castle's 3D model read on
// the game's thread lost its archive window to the map's loader thread
TEST(updates_read_the_games_archives_in_turn_without_updates) {
    const updates::HookPlan off = updates::hookPlan(true, false, 0, true);  // [client] updates = 0
    CHECK(off.inTurn && !off.updates);
    const updates::HookPlan none = updates::hookPlan(true, true, 0, true);  // no GRF in updates\ (the owner's)
    CHECK(none.inTurn && !none.updates);
    const updates::HookPlan both = updates::hookPlan(true, true, 2, true);
    CHECK(both.inTurn && both.updates);
    const updates::HookPlan lookups = updates::hookPlan(true, true, 2, false);  // its lookups another build's
    CHECK(lookups.inTurn && !lookups.updates);
    const updates::HookPlan other = updates::hookPlan(false, true, 2, true);  // another uaRO.exe: left alone
    CHECK(!other.inTurn && !other.updates);
}

// A read that waited for another thread's: the first five told in the log, then every hundredth
TEST(updates_tell_the_first_waits_then_every_hundredth) {
    for (unsigned n = 1; n <= 5; ++n) CHECK(updates::tellWait(n));
    CHECK(!updates::tellWait(6));
    CHECK(!updates::tellWait(99));
    CHECK(updates::tellWait(100));
    CHECK(!updates::tellWait(101));
    CHECK(updates::tellWait(300));
    CHECK(!updates::tellWait(0));
}

// 3.0.19-beta.1 (the testers' doc 14.11: the old client «4 акаунта пробую. всі вилітають» - «CPaletteRes :: Cannot
// find File : palette\몸\프리스트_남_4.pal», then uaRO_New.exe at 0x417B94: the character select's sprite drawn
// through no palette): a dye's palette the client has not - the sprite it recolours, whose own palette stands in
// (the sprite's own colours, as the game draws a dye it finds no palette of in the world)
TEST(updates_a_missing_palette_stands_in_by_its_sprite) {
    const std::string priest = "\xC7\xC1\xB8\xAE\xBD\xBA\xC6\xAE";  // 프리스트
    const std::string male = "\xB3\xB2", female = "\xBF\xA9";     // 남, 여
    CHECK_EQ(updates::paletteSprite("data\\palette\\\xB8\xF6\\" + priest + "_" + male + "_4.pal"),
             "data\\sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xF6\xC5\xEB\\" + male + "\\" + priest + "_" + male + ".spr");
    CHECK_EQ(updates::paletteSprite("data\\palette\\\xB8\xD3\xB8\xAE\\\xB8\xD3\xB8\xAE" "12_" + female + "_7.pal"),
             "data\\sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xD3\xB8\xAE\xC5\xEB\\" + female + "\\12_" + female + ".spr");
    CHECK_EQ(updates::paletteSprite("DATA\\PALETTE\\\xB8\xF6\\" + priest + "_" + male + "_4.PAL"),
             "data\\sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xF6\xC5\xEB\\" + male + "\\" + priest + "_" + male + ".spr");
    CHECK_EQ(updates::paletteSprite("data\\sprite\\poring.spr"), std::string());
    CHECK_EQ(updates::paletteSprite("data\\palette\\\xB8\xF6\\noSex_4.pal"), std::string());
    CHECK_EQ(updates::paletteSprite("data\\palette\\\xB8\xF6\\" + priest + "_" + male + ".pal"), std::string());
    // the sprite's own palette: its last 1024 bytes (an SPR 2.x)
    std::vector<uint8_t> spr = {'S', 'P', 0x01, 0x02, 1, 0, 0, 0};
    for (int i = 0; i < 300; ++i) spr.push_back(uint8_t(i));
    for (int i = 0; i < 1024; ++i) spr.push_back(uint8_t(255 - i % 256));
    std::vector<uint8_t> pal;
    CHECK(updates::spritePalette(spr, pal));
    CHECK_EQ(pal.size(), size_t(1024));
    CHECK_EQ(int(pal[0]), 255);
    CHECK_EQ(int(pal[1023]), 0);
    std::vector<uint8_t> old = {'S', 'P', 0x00, 0x01, 1, 0};  // SPR 1.0 keeps no palette
    old.resize(2000);
    CHECK(!updates::spritePalette(old, pal));
    CHECK(!updates::spritePalette({'X', 'X', 1, 2}, pal));
}
