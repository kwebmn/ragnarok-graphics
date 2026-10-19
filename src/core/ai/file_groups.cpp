#include "core/ai/file_groups.hpp"

#include <algorithm>
#include <cctype>
#include <set>

#include "core/ai/bosses.hpp"
#include "core/rsm.hpp"
#include "core/rsw.hpp"
#include "core/sprite_exe.hpp"
#include "core/zip.hpp"

namespace uagfx::ai {
namespace {

// The game's folders and words (CP949)
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
const std::string kTownIndoor = "\xB1\xE2\xC5\xB8\xB8\xB6\xC0\xBB\xB3\xBB\xBA\xCE";  // 기타마을내부
const std::string kOutdoor = "\xBF\xDC\xBA\xCE\xBC\xD2\xC7\xB0";  // 외부소품

// Where `word` stands in `s` on a character's start (a CP949 byte from 0x81 leads two); npos when nowhere.
size_t findWord(const std::string& s, const std::string& word, size_t from = 0) {
    for (size_t i = from; i + word.size() <= s.size();) {
        if (s.compare(i, word.size(), word) == 0) return i;
        i += static_cast<unsigned char>(s[i]) >= 0x81 ? 2 : 1;
    }
    return std::string::npos;
}
bool has(const std::string& s, const std::string& word) { return findWord(s, word) != std::string::npos; }

// A folder of the path: at its start or after a backslash, and a backslash after it.
bool inFolder(const std::string& p, const std::string& folder) {
    for (size_t at = findWord(p, folder + "\\"); at != std::string::npos; at = findWord(p, folder + "\\", at + 1))
        if (at == 0 || p[at - 1] == '\\') return true;
    return false;
}

std::string fileOf(const std::string& p) {
    const size_t slash = p.find_last_of('\\');
    return slash == std::string::npos ? p : p.substr(slash + 1);
}
std::string stemOf(const std::string& p) {
    const std::string f = fileOf(p);
    const size_t dot = f.find_last_of('.');
    return dot == std::string::npos ? f : f.substr(0, dot);
}
// The folder right after `folder\` in the path: "" when none.
std::string folderAfter(const std::string& p, const std::string& folder) {
    const size_t at = findWord(p, folder + "\\");
    if (at == std::string::npos) return "";
    const size_t from = at + folder.size() + 1, slash = p.find('\\', from);
    return slash == std::string::npos ? "" : p.substr(from, slash - from);
}

bool mountedBody(const std::string& name) {
    // English names: "arch_mage_riding", "dragon_knight_chicken", "frog_oboro", "meister_madogear1"
    for (const char* w : {"riding", "chicken", "frog", "madogear"})
        if (name.find(w) != std::string::npos) return true;
    // 페코, 그리폰, 늑대, 쁘띠 (a Rune Knight's dragon), 드래곤, 마도 (Mado Gear), 멧돼지, 사자, 알파카, 여우,
    // 켈베로스, 타조, 포링
    static const char* const korean[] = {"\xC6\xE4\xC4\xDA", "\xB1\xD7\xB8\xAE\xC6\xF9", "\xB4\xC1\xB4\xEB",
                                         "\xBB\xDA\xB6\xEC", "\xB5\xE5\xB7\xA1\xB0\xEF", "\xB8\xB6\xB5\xB5",
                                         "\xB8\xE4\xB5\xC5\xC1\xF6", "\xBB\xE7\xC0\xDA", "\xBE\xCB\xC6\xC4\xC4\xAB",
                                         "\xBF\xA9\xBF\xEC", "\xC4\xCC\xBA\xA3\xB7\xCE\xBD\xBA", "\xC5\xB8\xC1\xB6",
                                         "\xC6\xF7\xB8\xB5"};
    for (const char* w : korean)
        if (has(name, w)) return true;
    return false;
}

// A texture of the trees' and plants' folder, or of a model there: grass, a bush, a flower, a vine ... by its name;
// the rest the trees' (their trunks, crowns, leaves).
Group plantGroupOf(const std::string& name) {
    for (const char* w : {"flow", "grs", "grass", "reed", "weed", "bush", "undbr", "vine", "cactus", "mush", "seaweed",
                          "berr", "fpot", "sun"})
        if (name.find(w) != std::string::npos) return Group::Plants;
    return Group::Trees;
}

Group byKind(MapKind k, bool ground) {
    if (ground) return Group(int(Group::GroundTown) + int(k));
    const Group models[] = {Group::BuildingsTown, Group::ModelsField, Group::ModelsDungeon, Group::Furniture,
                            Group::ModelsOther};
    return models[int(k)];
}

// The kind of map most of these are of, ties to the earlier kind (towns first) - other maps only when no town, field,
// dungeon or house takes it: the instances, arenas and castles (some 700 of 1,250) mostly copy those.
MapKind mostOf(const std::vector<uint32_t>& maps, const std::vector<MapKind>& kinds) {
    int count[5] = {};
    for (uint32_t m : maps)
        if (m < kinds.size()) ++count[int(kinds[m])];
    const int* most = std::max_element(count, count + 4);
    return *most ? MapKind(most - count) : MapKind::Other;
}

}  // namespace

MapKind mapKindOf(const std::string& name) {
    std::string m = zip::normalName(name);
    if (m.size() > 2 && std::isdigit(static_cast<unsigned char>(m[0])) && m[1] == '@') m = m.substr(2);  // 1@tower
    static const std::set<std::string> towns = {
        "prontera", "pprontera", "x_prt", "geffen", "payon", "morocc", "alberta", "izlude", "izlude_a", "izlude_b",
        "izlude_c", "izlude_d", "aldebaran", "comodo", "yuno", "amatsu", "gonryun", "umbala", "niflheim", "louyang",
        "louyang_e", "jawaii", "ayothaya", "ayothaya_e", "einbroch", "einbech", "lighthalzen", "x_lhz", "hugel",
        "rachel", "x_ra", "veins", "moscovia", "brasilis", "dewata", "malangdo", "malaya", "eclage", "ecl_hub01",
        "mora", "dicastes01", "dicastes02", "manuk", "splendide", "mid_camp", "harboro1", "harboro2", "lasagna",
        "rockrdg1", "rockrdg2", "verus04", "xmas", "wolfvill", "herosria"};
    if (towns.count(m)) return MapKind::Town;
    if (m.find("fild") != std::string::npos || m.rfind("mjolnir", 0) == 0) return MapKind::Field;
    static const char* const dungeons[] = {"dun", "anthell", "c_tower", "tower", "maze", "abyss", "abbey", "gefenia",
                                           "cave", "sewb", "pryd", "sphinx", "kh_", "juperos", "jupe_", "thor_v",
                                           "ra_san", "odin_tem", "tha_t", "treasure", "_d0"};
    if (m.rfind("gl_", 0) == 0) return MapKind::Dungeon;  // Glast Heim, its inside too
    for (const char* d : dungeons)
        if (m.find(d) != std::string::npos) return MapKind::Dungeon;
    for (size_t at = m.find("_in"); at != std::string::npos; at = m.find("_in", at + 1)) {
        const size_t next = at + 3;  // "alberta_in", "ama_in01", "gef_in_1"
        if (next == m.size() || std::isdigit(static_cast<unsigned char>(m[next])) || m[next] == '_')
            return MapKind::Indoor;
    }
    if (m.rfind("in_", 0) == 0) return MapKind::Indoor;  // "in_hunter"
    return MapKind::Other;
}

Group spriteGroupOf(const std::string& path) {
    const std::string p = zip::normalName(path);
    const std::string name = stemOf(p);
    if (name == "cursors") return Group::Cursor;
    if (inFolder(p, "accessory")) return Group::Headgear;  // newer headgear's folder
    switch (sprite_exe::sourceOf(p.c_str())) {
        case sprite_exe::Source::Interface: return name.rfind("emotion", 0) == 0 ? Group::Emotions : Group::Windows;
        case sprite_exe::Source::Npc: return Group::Npcs;
        case sprite_exe::Source::Item: return Group::ItemsGround;
        case sprite_exe::Source::Effect: return Group::EffectSprites;
        case sprite_exe::Source::Player: {
            if (inFolder(p, kHeadgear) || inFolder(p, "accessory")) return Group::Headgear;
            if (inFolder(p, kShield)) return Group::Shields;
            if (inFolder(p, kGarment)) return Group::Garments;
            if (inFolder(p, kHead)) return Group::Heads;
            if (inFolder(p, kBody)) return mountedBody(name) ? Group::Mounted : Group::Bodies;
            return Group::Weapons;  // a job's folder: its weapons
        }
        case sprite_exe::Source::Monster:
            if (inFolder(p, "homun")) return Group::Homunculi;
            return bossSprite(name) ? Group::Bosses : Group::Monsters;
        case sprite_exe::Source::Unknown: break;
    }
    return bossSprite(name) ? Group::Bosses : Group::Monsters;
}

Group interfaceGroupOf(const std::string& path, int width, int height) {
    (void)width;
    (void)height;
    const std::string p = zip::normalName(path);
    const std::string folder = folderAfter(p, kUi), name = fileOf(p);
    if (folder == "item") return Group::ItemIcons;
    if (folder == "collection" || folder == "colletion") return Group::ItemPictures;
    if (folder == "cardbmp") return Group::CardPictures;
    if (folder == "illust") return Group::Portraits;
    if (folder == "map" || folder == "minimap" || folder.rfind("worldview", 0) == 0) return Group::Maps;
    if (folder == "menu_icon" || folder == "ro_menu_icon") return Group::MenuIcons;
    if (folder.rfind("login", 0) == 0 || folder.rfind("select_character", 0) == 0 ||
        folder.rfind("make_character", 0) == 0)
        return Group::Login;
    if (folder.empty()) {  // the folder's own files
        if (name.rfind("loading", 0) == 0) return Group::Loading;
        if (name.rfind("worldmap", 0) == 0) return Group::Maps;
        for (const char* w : {"login", "rag_title", "ad_title"})
            if (name.find(w) != std::string::npos) return Group::Login;
    }
    return Group::Windows;
}

WorldUses worldUsesOf(const std::vector<std::string>& maps, const map_textures::Read& read) {
    WorldUses u;
    u.kinds.reserve(maps.size());
    std::map<std::string, std::vector<std::string>> modelTextures;  // a model's file -> its textures
    std::vector<uint8_t> data;
    std::string error;
    for (size_t i = 0; i < maps.size(); ++i) {
        const std::string map = map_textures::mapName(maps[i]);
        u.kinds.push_back(mapKindOf(map));
        if (map.empty()) continue;
        const uint32_t index = uint32_t(i);
        if (read("data\\" + map + ".gnd", data)) {
            std::set<std::string> once;
            for (const std::string& t : map_textures::groundTextures(data.data(), data.size()))
                if (!t.empty() && once.insert(zip::normalName("data\\texture\\" + t)).second)
                    u.textures[zip::normalName("data\\texture\\" + t)].ground.push_back(index);
        }
        if (!read("data\\" + map + ".rsw", data)) continue;
        rsw::World world;
        if (!rsw::parse(data.data(), data.size(), world, error)) continue;
        std::set<std::string> once;
        for (const rsw::Model& m : world.models) {
            const std::string file = zip::normalName("data\\model\\" + m.file);
            auto it = modelTextures.find(file);
            if (it == modelTextures.end()) {
                std::vector<std::string> textures;
                rsm::Model model;
                if (read(file, data) && rsm::parse(data.data(), data.size(), model, error))
                    for (const rsm::Part& part : model.parts)
                        if (!part.texture.empty())
                            textures.push_back(zip::normalName("data\\texture\\" + part.texture));
                it = modelTextures.emplace(file, std::move(textures)).first;
            }
            const bool plant = plantModelFile(file);
            for (const std::string& t : it->second) {
                WorldUses::Use& use = u.textures[t];
                if (once.insert(t).second) use.models.push_back(index);
                use.plantModel = use.plantModel || plant;
            }
        }
    }
    return u;
}

bool plantModelFile(const std::string& file) { return inFolder(zip::normalName(file), kPlants); }

Group worldGroupOf(const std::string& path, const WorldUses* uses) {
    const std::string p = zip::normalName(path);
    const std::string name = stemOf(p);
    if (inFolder(p, "effect")) return Group::SkillAnimations;
    if (inFolder(p, kWater)) return Group::Water;
    if (inFolder(p, kPlants)) return plantGroupOf(name);
    if (uses) {
        const auto it = uses->textures.find(p);
        if (it != uses->textures.end()) {
            const WorldUses::Use& u = it->second;
            if (u.plantModel && u.models.size() >= u.ground.size()) return plantGroupOf(name);
            if (!u.ground.empty() && u.ground.size() >= u.models.size())
                return byKind(mostOf(u.ground, uses->kinds), true);
            if (!u.models.empty()) return byKind(mostOf(u.models, uses->kinds), false);
        }
    }
    if (inFolder(p, kFloor)) return Group::GroundField;
    if (inFolder(p, kIndoor) || inFolder(p, kTownIndoor)) return Group::Furniture;
    if (inFolder(p, kOutdoor)) return Group::ModelsField;
    return Group::ModelsOther;
}

}  // namespace uagfx::ai
