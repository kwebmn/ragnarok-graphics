#include <string>
#include <vector>

#include "core/pack_overview.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(pack_overview_counts_and_unread_bytes) {
    // 2.3 (P1): the pack's line - its pictures by group, model and kind, and the space pictures replaced or taken out
    // still hold (Compact gives it back)
    std::vector<pack::ZipEntry> entries(3);
    entries[0].name = "world/2x2/a.webp";
    entries[0].header = 0;
    entries[0].compressed = 100;
    entries[1].name = "sprites/2x2/b.webp";
    entries[1].header = 300;  // 130 bytes between: a picture replaced (its old bytes unread)
    entries[1].compressed = 50;
    entries[2].name = "index.txt";
    entries[2].header = 398;
    entries[2].compressed = 20;
    pack::Index index;
    index.pictures["world/2x2/a.webp"] = pack::IndexEntry{ai::Kind::Ground, {}, "anime", "world"};
    index.pictures["sprites/2x2/b.webp"] = pack::IndexEntry{ai::Kind::Sprites, {}, "", "monsters"};
    const pack::Overview o = pack::overviewOf(entries, 1000, index, 7, 12);
    CHECK_EQ(o.pictures, 2u);
    CHECK(o.byGroup.at("world") == 1 && o.byGroup.at("sprites") == 1);
    CHECK(o.byModel.at("anime") == 1 && o.byModel.at("?") == 1);
    CHECK(o.byKind.at("ground") == 1 && o.byKind.at("sprites") == 1);
    CHECK(o.orphans == 7 && o.missing == 12 && o.bytes == 1000);
    // read: each entry's local header (30 bytes and its name) and its data, up to the last one's end (the central
    // directory after it); the rest of that is unread - here the gap of 154 bytes after the first picture
    CHECK_EQ(o.unread, uint64_t(154));
}
