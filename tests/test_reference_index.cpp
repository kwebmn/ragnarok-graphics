// 3.0.11-beta.6: the game's reference pack read for the new minimap's hints (core/reference_index) - gzip, the pack's
// maps (NPCs, warps to other maps, monsters with counts), the site's NPC kinds, the client's map names.
#include <string>

#include "core/reference_index.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// {"v":1,"x":[1,2,3]} by Python's gzip (mtime 0, no name)
const std::string kGz("\x1F\x8B\x08\x00\x00\x00\x00\x00\x02\xFF\xAB\x56\x2A\x53\xB2\x32\xD4\x51\xAA\x50\xB2\x8A\x36"
                      "\xD4\x31\xD2\x31\x8E\xAD\x05\x00\xD2\xE9\xEA\xB7\x13\x00\x00\x00",
                      39);

// A pack of format 1 cut down: two maps, a boss, a monster the site does not list, warps of each kind
const char* kPack = R"({"v":1,"data":1793227520,"commit":"d68ea17ea5b8",
"mobs":{"1002":["Poring",1,50,2,1,1,3,1,1,0,[],[],null],"1063":["Lunatic",3,60,6,2,0,2,0,1,0,[],[],null],
        "1917":["Satan Morroc",99,60000000,0,0,2,6,7,4,1,[],[],null],"1113":["Drops",2,55,4,3,0,3,3,1,0,[],[],null]},
"spawns":{"prt_fild08":[[1063,40,0,0,0,0,0],[1002,70,0,0,0,0,0],[1002,30,0,0,0,0,5000],[9999,5,0,0,0,0,0],
                        [1113,40,0,0,0,0,0]],
          "moc_fild20":[[1917,1,200,200,0,0,7200000]]},
"routes":[["prt_fild08",170,375,"prt_fild05","w"],["prt_fild08",20,30,"prt_fild08",1,1,"w","inside",null,0],
          ["prt_fild08",170,375,"prt_fild05",170,15,"w","prt_fild08",null,0],
          ["prontera",151,29,"izlude",91,105,"k","Kafra",600,0]],
"npcs":[["prt_fild08",100,120,"Kafra Employee",113],["prt_fild08",50,60,"Tool Dealer",83],["",1,1,"Nobody",1]]})";

}  // namespace

TEST(reference_index_gunzips_a_gzip_member) {
    std::string out;
    CHECK(refindex::gunzip(kGz, out));
    CHECK_EQ(out, std::string(R"({"v":1,"x":[1,2,3]})"));
    std::string bad = kGz;
    bad[bad.size() - 6] ^= 1;  // the CRC
    CHECK(!refindex::gunzip(bad, out));
    CHECK(!refindex::gunzip("not gzip at all, but long enough", out));
    CHECK(!refindex::gunzip(kGz.substr(0, 20), out));
}

TEST(reference_index_reads_a_maps_hints) {
    refindex::Index ix;
    CHECK(refindex::build(kPack, ix));
    CHECK_EQ(ix.data, 1793227520LL);
    CHECK_EQ(ix.mobs.size(), size_t(4));
    CHECK(ix.mobs[1917].boss && !ix.mobs[1002].boss);
    const refindex::MapHints& f = ix.maps["prt_fild08"];
    // monsters: the same one's lines summed (Poring 70 + 30), the longest respawn kept, one the site does not list
    // left out; by count, then by name (Drops before Lunatic at 40)
    CHECK_EQ(f.mobs.size(), size_t(3));
    CHECK(f.mobs[0].id == 1002 && f.mobs[0].count == 100 && f.mobs[0].respawnMs == 5000);
    CHECK(f.mobs[1].id == 1113 && f.mobs[2].id == 1063);
    // warps: only to another map, only plain ones (a broken row - too short - gives nothing)
    CHECK_EQ(f.warps.size(), size_t(1));
    CHECK(f.warps[0].dest == "prt_fild05" && f.warps[0].x == 170 && f.warps[0].y == 375 && f.warps[0].dy == 15);
    CHECK(ix.maps["prontera"].warps.empty());  // a Kafra's teleport is no warp on the map
    CHECK_EQ(f.npcs.size(), size_t(2));
    CHECK(f.npcs[0].kind == refindex::NpcKind::Kafra && f.npcs[0].x == 100 && f.npcs[0].y == 120);
    CHECK(f.npcs[1].kind == refindex::NpcKind::Tool);
    const refindex::MapHints& m = ix.maps["moc_fild20"];
    CHECK(m.mobs.size() == 1 && m.mobs[0].respawnMs == 7200000);
    CHECK(!refindex::build(R"({"v":2})", ix));
    CHECK(!refindex::build("<html>", ix));
}

TEST(reference_index_bosses_come_first) {
    refindex::Index ix;
    CHECK(refindex::build(R"({"v":1,"data":1,"mobs":{"1":["A",1,1,0,0,0,0,0,1,0],"2":["B",1,1,0,0,0,0,0,1,1]},)"
                          R"("spawns":{"m":[[1,50,0,0,0,0,0],[2,1,0,0,0,0,3600000]]},"routes":[],"npcs":[]})",
                          ix));
    CHECK(ix.maps["m"].mobs[0].id == 2);  // the boss of one before fifty others
}

TEST(reference_index_npc_kinds_as_the_site) {
    CHECK(refindex::kindOf("Kafra Employee") == refindex::NpcKind::Kafra);
    CHECK(refindex::kindOf("Nurse") == refindex::NpcKind::Healer);
    CHECK(refindex::kindOf("Item Collector") == refindex::NpcKind::Tool);
    CHECK(refindex::kindOf("Blacksmith Dropp") == refindex::NpcKind::Weapon);
    CHECK(refindex::kindOf("Armor Dealer") == refindex::NpcKind::Tool);  // the site's order: «dealer» before «armor»
    CHECK(refindex::kindOf("Innkeeper") == refindex::NpcKind::Inn);
    CHECK(refindex::kindOf("Prontera Guard") == refindex::NpcKind::Guide);
    CHECK(refindex::kindOf("Swordman Guildsman Master") == refindex::NpcKind::Job);
    CHECK(refindex::kindOf("Sailor") == refindex::NpcKind::Warp);
    CHECK(refindex::kindOf("Bard") == refindex::NpcKind::Other);
    CHECK_EQ(refindex::colorOf(refindex::NpcKind::Kafra), 0xD6336Cu);
    CHECK_EQ(refindex::colorOf(refindex::NpcKind::Other), 0x868E96u);
}

TEST(reference_index_reads_the_clients_map_names) {
    const auto names = refindex::mapNames(
        "prt_fild07.rsw#Prontera Field #\r\nPRONTERA.RSW#Prontera City, Capitol of Rune-Midgard #\r\n"
        "// nothing\r\nbroken line\r\ntur_dun01.rsw#Turtle Island#");
    CHECK_EQ(names.size(), size_t(3));
    CHECK_EQ(names.at("prt_fild07"), std::string("Prontera Field"));
    CHECK_EQ(names.at("prontera"), std::string("Prontera City, Capitol of Rune-Midgard"));
    CHECK_EQ(names.at("tur_dun01"), std::string("Turtle Island"));
    CHECK_EQ(refindex::shortName(names.at("prontera")), std::string("Prontera City"));  // one line under the map
    CHECK_EQ(refindex::shortName("Turtle Island"), std::string("Turtle Island"));
}

// 3.0.11-beta.200 (the tester: «в паті лісті виводиться назва файла мапи, треба прибрати» - «pay_dun03.rsw»): a map's
// words for a list - the client's name, its number added where the name is more than one map's («Payon Cave 3»), the
// code where the client names none
TEST(reference_index_map_words) {
    const std::map<std::string, std::string> names = refindex::mapNames(
        "pay_dun00.rsw#Payon Cave #\npay_dun03.rsw#Payon Cave #\nprt_fild08.rsw#Prontera Field #\n"
        "prt_fild10.rsw#Prontera Field #\nyuno_fild08.rsw#Kiel Hyre's Academy#\n");
    CHECK(refindex::mapWords(names, "pay_dun03") == "Payon Cave 3");
    CHECK(refindex::mapWords(names, "pay_dun00") == "Payon Cave 0");
    CHECK(refindex::mapWords(names, "prt_fild08") == "Prontera Field 8");
    CHECK(refindex::mapWords(names, "yuno_fild08") == "Kiel Hyre's Academy");  // the only one of its name
    CHECK(refindex::mapWords(names, "new_map") == "new_map");
}

// 3.0.18-beta.239 (the reference's rewrite, the owner's review of 13.11): a monster's entries of one thing - mob_db
// lists Apple eight times for Hugeling - one drop: its chance 1 - (1 - a)(1 - b)..., the entries kept for the note;
// the MVP's reward of the same thing apart; one source a monster for the thing's droppers
TEST(reference_index_merges_a_monsters_entries_of_one_thing) {
    refindex::Index ix;
    CHECK(refindex::build(R"({"v":1,"data":1,"mobs":{"1284":["Hugeling",1,1,0,0,0,0,0,1,0,)"
                          R"([[512,100],[512,100],[512,100],[512,100],[512,100],[512,100],[512,100],[512,100],)"
                          R"([909,8000],[909,8000],[601,150]],[[909,5000]],null]},"spawns":{},"routes":[],"npcs":[]})",
                          ix));
    const refindex::Mob& m = ix.mobs.at(1284);
    CHECK_EQ(m.drops.size(), size_t(4));
    int apples = 0, jellopies = 0;
    for (const refindex::Drop& d : m.drops) {
        if (d.item == 512) {
            ++apples;
            CHECK_EQ(d.rate, 773);  // 1 - 0.99^8 = 7.73 %
            CHECK_EQ(d.parts.size(), size_t(8));
        }
        if (d.item == 909 && d.kind == 0) {
            ++jellopies;
            CHECK_EQ(d.rate, 9600);
            CHECK(d.parts == std::vector<int>({8000, 8000}));
        }
        if (d.item == 909 && d.kind == 1) CHECK(d.rate == 5000 && d.parts.empty());
        if (d.item == 601) CHECK(d.rate == 150 && d.parts.empty());
    }
    CHECK(apples == 1 && jellopies == 1);
    const auto by = ix.droppedBy.find(512);
    CHECK(by != ix.droppedBy.end() && by->second.size() == 1);
    if (by != ix.droppedBy.end() && !by->second.empty()) CHECK(by->second[0].rate == 773 && by->second[0].parts.size() == 8);
}
