// 3.0.17-beta.13 (the owner: «функционал старого и нового не пересекались», the old client «перестав запускатись»
// - «Cannot init d3d»): uaRO HD keeps its settings in its own key; the old client's screen comes back from its
// setup.exe
#include "core/settings/game_key.hpp"
#include "test.hpp"

using namespace uagfx::settings;

TEST(game_key_the_old_client_keeps_the_shared_key) {
    CHECK_EQ(gameKeyLeaf("uaRO"), std::string("Ragnarok"));
    CHECK_EQ(gameKeyLeaf("UARO"), std::string("Ragnarok"));
    CHECK_EQ(gameKeyLeaf(""), std::string("Ragnarok"));
    CHECK_EQ(gameKeyPath("Ragnarok"), std::string("Software\\Gravity Soft\\Ragnarok"));
}

TEST(game_key_a_client_of_its_own) {
    CHECK_EQ(gameKeyLeaf("uaRO HD"), std::string("uaRO HD"));
    CHECK_EQ(gameKeyPath("uaRO HD"), std::string("Software\\Gravity Soft\\uaRO HD"));
    // too long for the game's string: a short name of its own, the same every time
    const std::string longOne = gameKeyLeaf("uaRO HD Classic+");
    CHECK(longOne.rfind("uaRO-", 0) == 0);
    CHECK(gameKeyPath(longOne).size() <= kGameKeyRoom);
    CHECK_EQ(longOne, gameKeyLeaf("uaRO HD Classic+"));
    CHECK(longOne != gameKeyLeaf("uaRO HD Remaster"));
    // a name of Gravity's own keys never: they would be shared again
    CHECK(gameKeyLeaf("ragnarok") != "ragnarok");
    CHECK(gameKeyLeaf("RoClient") != "RoClient");
    CHECK(gameKeyLeaf("ragnarok").rfind("uaRO-", 0) == 0);
}

namespace {

GameRegistry sharedKey(bool setupInCommon) {
    GameRegistry r;
    r.common.exists = true;
    if (setupInCommon) {
        r.common.keys[""]["WIDTH"] = RegValue::dword(1024);
        r.common.keys[""]["HEIGHT"] = RegValue::dword(768);
        r.common.keys[""]["DEVICENAME"] = RegValue::sz("dgVoodoo HAL");
    }
    r.personal.exists = true;
    r.personal.keys[""]["WIDTH"] = RegValue::dword(2560);
    r.personal.keys[""]["HEIGHT"] = RegValue::dword(1440);
    r.personal.keys[""]["BITPERPIXEL"] = RegValue::dword(32);
    r.personal.keys[""]["DEVICENAME"] = RegValue::sz("Direct3D HAL");
    r.personal.keys[""]["GUIDDRIVER"] = RegValue::binary(std::vector<uint8_t>(16, 0));
    r.personal.keys[""]["TEXTUREMODE"] = RegValue::dword(1);
    r.personal.keys["Option"]["WIDTH"] = RegValue::dword(5);  // not the screen: a subkey's
    return r;
}

}  // namespace

TEST(game_key_the_shared_key_gets_its_setup_back) {
    const std::vector<RegWrite> w = sharedRepair(sharedKey(true));
    CHECK_EQ(w.size(), size_t(5));  // the screen's five values there; TEXTUREMODE and Option stay
    for (const RegWrite& one : w) {
        CHECK(one.op == RegOp::Delete);
        CHECK(one.subkey.empty());
        CHECK(one.name != "TEXTUREMODE");
    }
    CHECK(sharedRepair(sharedKey(false)).empty());  // no setup.exe values under: the old client keeps what it has
    CHECK(hasOwnSetup(sharedKey(true).common));
    CHECK(!hasOwnSetup(sharedKey(false).common));
}
