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
