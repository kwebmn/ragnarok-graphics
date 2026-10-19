#include <map>
#include <string>
#include <vector>

#include "core/ai/bosses.hpp"
#include "core/ai/file_groups.hpp"
#include "core/ai/pack_models.hpp"
#include "sample_files.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::ai;

namespace {

// The game's folders and names (CP949, as its archives have them)
const std::string kHuman = "\xC0\xCE\xB0\xA3\xC1\xB7";          // 인간족
const std::string kBody = "\xB8\xF6\xC5\xEB";                    // 몸통
const std::string kHead = "\xB8\xD3\xB8\xAE\xC5\xEB";            // 머리통
const std::string kHeadgear = "\xBE\xC7\xBC\xBC\xBB\xE7\xB8\xAE";  // 악세사리
const std::string kShield = "\xB9\xE6\xC6\xD0";                  // 방패
const std::string kGarment = "\xB7\xCE\xBA\xEA";                 // 로브
const std::string kMonster = "\xB8\xF3\xBD\xBA\xC5\xCD";         // 몬스터
const std::string kItem = "\xBE\xC6\xC0\xCC\xC5\xDB";            // 아이템
const std::string kEffect = "\xC0\xCC\xC6\xD1\xC6\xAE";          // 이팩트
const std::string kUi = "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA";  // 유저인터페이스
const std::string kWater = "\xBF\xF6\xC5\xCD";                   // 워터
const std::string kPlants = "\xB3\xAA\xB9\xAB\xC0\xE2\xC3\xCA\xB2\xC9";  // 나무잡초꽃
const std::string kFloor = "\xC7\xCA\xB5\xE5\xB9\xD9\xB4\xDA";   // 필드바닥
const std::string kIndoor = "\xB3\xBB\xBA\xCE\xBC\xD2\xC7\xB0";  // 내부소품
const std::string kSword = "\xB0\xCB\xBB\xE7";                   // 검사
const std::string kMale = "\xB3\xB2";                            // 남

std::string sprite(const std::string& path) { return "data\\sprite\\" + path + ".spr"; }

}  // namespace

TEST(file_groups_kinds_of_maps_by_their_names) {
    CHECK(mapKindOf("prontera") == MapKind::Town && mapKindOf("izlude") == MapKind::Town);
    CHECK(mapKindOf("dicastes01") == MapKind::Town);
    CHECK(mapKindOf("prt_fild08") == MapKind::Field && mapKindOf("mjolnir_01") == MapKind::Field);
    CHECK(mapKindOf("gef_dun01") == MapKind::Dungeon && mapKindOf("c_tower1") == MapKind::Dungeon);
    CHECK(mapKindOf("gl_in01") == MapKind::Dungeon);  // Glast Heim's inside is a dungeon
    CHECK(mapKindOf("alberta_in") == MapKind::Indoor && mapKindOf("ama_in01") == MapKind::Indoor);
    CHECK(mapKindOf("1@gef_in") == MapKind::Indoor && mapKindOf("1@tower") == MapKind::Dungeon);  // instances
    CHECK(mapKindOf("aldeg_cas01") == MapKind::Other && mapKindOf("pvp_y_1-1") == MapKind::Other);
}

TEST(file_groups_of_sprites_by_their_folders) {
    const auto g = [](const std::string& path) { return spriteGroupOf(sprite(path)); };
    CHECK(g(kHuman + "\\" + kBody + "\\" + kMale + "\\" + kSword + "_" + kMale) == Group::Bodies);
    CHECK(g(kHuman + "\\" + kBody + "\\" + kMale + "\\arch_mage_riding_" + kMale) == Group::Mounted);
    // 구페코크루세이더_남: a crusader on a Peco Peco
    CHECK(g(kHuman + "\\" + kBody + "\\" + kMale +
            "\\\xB1\xB8\xC6\xE4\xC4\xDA\xC5\xA9\xB7\xE7\xBC\xBC\xC0\xCC\xB4\xF5_" + kMale) == Group::Mounted);
    CHECK(g(kHuman + "\\" + kBody + "\\" + kMale + "\\dragon_knight_" + kMale) == Group::Bodies);  // a job, no mount
    CHECK(g(kHuman + "\\" + kHead + "\\" + kMale + "\\1_" + kMale) == Group::Heads);
    CHECK(g(kHuman + "\\" + kSword + "\\" + kSword + "_" + kMale + "_" + kSword) == Group::Weapons);
    CHECK(g(kHeadgear + "\\" + kMale + "\\" + kMale + "_ribbon") == Group::Headgear);
    CHECK(g("accessory\\" + kMale + "\\x") == Group::Headgear);
    CHECK(g(kShield + "\\" + kSword + "\\" + kSword + "_" + kMale + "_guard") == Group::Shields);
    CHECK(g(kGarment + "\\wings\\" + kMale + "\\" + kSword + "_" + kMale) == Group::Garments);
    CHECK(g(kMonster + "\\poring") == Group::Monsters && g(kMonster + "\\Baphomet") == Group::Bosses);
    CHECK(g("homun\\lif") == Group::Homunculi && g("npc\\4_f_kafra1") == Group::Npcs);
    CHECK(g(kItem + "\\apple") == Group::ItemsGround);
    CHECK(g(kEffect + "\\fire") == Group::EffectSprites && g(kEffect + "\\emotion") == Group::Emotions);
    CHECK(g("cursors") == Group::Cursor && g("unknown\\thing") == Group::Monsters);
}

TEST(file_groups_of_the_interface_by_its_folders) {
    const auto g = [](const std::string& name, int w = 24, int h = 24) {
        return interfaceGroupOf("data\\texture\\" + kUi + "\\" + name, w, h);
    };
    CHECK(g("item\\apple.bmp") == Group::ItemIcons && g("collection\\apple.bmp", 75, 100) == Group::ItemPictures);
    CHECK(g("colletion\\x.bmp") == Group::ItemPictures && g("cardbmp\\poring.bmp", 150, 200) == Group::CardPictures);
    CHECK(g("illust\\kafra.bmp", 300, 400) == Group::Portraits && g("map\\prontera.bmp", 128, 128) == Group::Maps);
    CHECK(g("minimap\\boss_1.bmp") == Group::Maps && g("worldmap.bmp", 512, 384) == Group::Maps);
    CHECK(g("loading03.jpg", 1024, 768) == Group::Loading && g("t_login.bmp", 280, 120) == Group::Login);
    CHECK(g("select_character_ver3\\bg_back1.bmp", 512, 512) == Group::Login);
    CHECK(g("menu_icon\\bag.bmp") == Group::MenuIcons && g("basic_interface\\btn_ok.bmp") == Group::Windows);
    CHECK(g("btn_ok.bmp") == Group::Windows);
}

TEST(file_groups_of_the_world_by_its_maps) {
    // which maps lay a texture on their ground and whose models take it: the kind of map that has it most
    WorldUses uses;
    uses.kinds = {MapKind::Town, MapKind::Town, MapKind::Indoor, MapKind::Dungeon};
    uses.textures["data\\texture\\a\\stone.bmp"].ground = {0, 1, 3};
    uses.textures["data\\texture\\a\\chair.bmp"].models = {2};
    uses.textures["data\\texture\\a\\wall.bmp"].models = {3};
    uses.textures["data\\texture\\a\\roof.bmp"].models = {0};
    uses.textures["data\\texture\\a\\both.bmp"].ground = {3};
    uses.textures["data\\texture\\a\\both.bmp"].models = {0, 1};
    const auto g = [&](const std::string& path) { return worldGroupOf("data\\texture\\" + path, &uses); };
    CHECK(g("a\\stone.bmp") == Group::GroundTown && g("a\\chair.bmp") == Group::Furniture);
    CHECK(g("a\\wall.bmp") == Group::ModelsDungeon && g("a\\roof.bmp") == Group::BuildingsTown);
    CHECK(g("a\\both.bmp") == Group::BuildingsTown);  // taken by more of them as a model
    // other maps (arenas, instances, castles) only when no other kind takes it
    uses.kinds.push_back(MapKind::Other);
    uses.kinds.push_back(MapKind::Other);
    uses.textures["data\\texture\\a\\copied.bmp"].ground = {1, 4, 5};
    uses.textures["data\\texture\\a\\arena.bmp"].ground = {4, 5};
    CHECK(g("a\\copied.bmp") == Group::GroundTown && g("a\\arena.bmp") == Group::GroundOther);
    CHECK(g(kWater + "\\water001.jpg") == Group::Water);
    CHECK(g(kPlants + "\\prt-fie-tree1.bmp") == Group::Trees && g(kPlants + "\\tur_flower.bmp") == Group::Plants);
    CHECK(g(kPlants + "\\myo-grs1.bmp") == Group::Plants && g(kPlants + "\\reed.bmp") == Group::Plants);
    CHECK(g("effect\\fire.bmp") == Group::SkillAnimations);
    // no map takes it: by its folder
    CHECK(g(kFloor + "\\sand.bmp") == Group::GroundField && g(kIndoor + "\\table.bmp") == Group::Furniture);
    CHECK(worldGroupOf("data\\texture\\x\\y.bmp", nullptr) == Group::ModelsOther);
}

TEST(file_groups_world_uses_read_from_the_maps) {
    // the ground's and the models' textures of every map, a model read once
    const std::map<std::string, std::vector<uint8_t>> files = {
        {"data\\prontera.gnd", testfiles::gnd({"Town\\Stone.BMP"}, 1, 1, {{}})},
        {"data\\prontera.rsw", testfiles::rsw()},       // one model: tree.rsm
        {"data\\prt_in.rsw", testfiles::rsw()},         // the same model, indoors
        {"data\\model\\tree.rsm", testfiles::rsm()}};  // bark.bmp, leaf.bmp
    int reads = 0;
    const map_textures::Read read = [&](const std::string& name, std::vector<uint8_t>& out) {
        const auto it = files.find(name);
        if (it == files.end()) return false;
        if (name.find(".rsm") != std::string::npos) ++reads;
        out = it->second;
        return true;
    };
    const WorldUses u = worldUsesOf({"prontera", "prt_in", "nowhere"}, read);
    CHECK(u.kinds.size() == 3 && u.kinds[0] == MapKind::Town && u.kinds[1] == MapKind::Indoor);
    CHECK_EQ(reads, 1);
    const auto& stone = u.textures.at("data\\texture\\town\\stone.bmp");
    CHECK(stone.ground == std::vector<uint32_t>({0}) && stone.models.empty());
    const auto& bark = u.textures.at("data\\texture\\bark.bmp");
    CHECK(bark.models == std::vector<uint32_t>({0, 1}) && bark.ground.empty());
    CHECK(worldGroupOf("data\\texture\\town\\stone.bmp", &u) == Group::GroundTown);
}

TEST(file_groups_bosses_by_the_servers_monsters) {
    // uAthena's monster database (tools/boss_sprites.py): the MVPs and the bosses that move or fight
    CHECK(bossSprite("baphomet") && bossSprite("BAPHOMET") && !bossSprite("poring"));
    CHECK(!bossSprite("empelium") && !bossSprite("treasure_box1") && !bossSprite("blue_flower"));
    CHECK(bossSprite("angeling"));  // a boss that is no MVP
}

TEST(file_groups_a_pictures_group_by_its_file) {
    pack::Source sprite{pack::Source::Sprite, Kind::Sprites};
    CHECK(groupOfPicture("data\\sprite\\npc\\4_f_kafra1.spr", sprite, 37, 95) == Group::Npcs);
    pack::Source ui{pack::Source::Interface, Kind::Icons};
    CHECK(groupOfPicture("data\\texture\\" + kUi + "\\item\\apple.bmp", ui, 24, 24) == Group::ItemIcons);
    pack::Source jpeg{pack::Source::Jpeg, Kind::Icons};
    CHECK(groupOfPicture("data\\texture\\" + kUi + "\\loading01.jpg", jpeg, 1024, 768) == Group::Loading);
    pack::Source water{pack::Source::Jpeg, Kind::Objects};
    CHECK(groupOfPicture("data\\texture\\" + kWater + "\\water000.jpg", water, 128, 128) == Group::Water);
    pack::Source effect{pack::Source::Tga, Kind::Effects};
    CHECK(groupOfPicture("data\\texture\\effect\\a.tga", effect, 64, 64) == Group::SkillAnimations);
}
