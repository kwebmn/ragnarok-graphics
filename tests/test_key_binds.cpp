// 3.0.24-beta.55 (the owner's «делай все что можешь без проверки», item 12: the keyboard concept's last stage): the
// walk mode's keys rebound - [keys] in uaro-graphics.ini, each action a key by its place (scan code, any layout)
#include "core/key_binds.hpp"
#include "core/walk_mode.hpp"
#include "test.hpp"

using namespace uagfx;
using keybinds::Bind;

TEST(key_binds_defaults_tokens_and_lookup) {
    const keybinds::Binds b = keybinds::defaults();
    CHECK_EQ(b.scan[int(Bind::Toggle)], 0x29u);  // `
    CHECK_EQ(b.scan[int(Bind::Attack)], 0x39u);  // Space
    CHECK_EQ(b.scan[int(Bind::Pickup)], 0x21u);  // F
    CHECK_EQ(b.scan[int(Bind::Target)], 0x0Fu);  // Tab
    CHECK_EQ(keybinds::scanOf("z"), 0x2Cu);
    CHECK_EQ(keybinds::scanOf("nope"), 0u);
    CHECK(std::string(keybinds::tokenOf(0x39)) == "space" && std::string(keybinds::tokenOf(0x11)).empty());  // W: none
    CHECK(std::string(keybinds::settingKey(Bind::CameraRight)) == "camera_right");
    bool found = false;
    CHECK(keybinds::boundTo(b, 0x21, found) == Bind::Pickup && found);
    keybinds::boundTo(b, 0x2C, found);
    CHECK(!found);  // Z: nothing
    for (const char* t : keybinds::kTokens) CHECK(keybinds::scanOf(t) != 0);
}

TEST(key_binds_the_walk_mode_follows_them) {
    walkmode::Mode m;
    keybinds::Binds b = keybinds::defaults();
    b.scan[int(Bind::Attack)] = 0x2C;  // Z
    b.scan[int(Bind::Toggle)] = 0x30;  // B
    m.setBinds(b);
    walkmode::Key k;
    k.vk = 'B', k.scan = 0x30, k.down = true, k.chatEmpty = true;
    CHECK(m.key(k) == walkmode::Act::Toggle && m.on());
    k.vk = 'Z', k.scan = 0x2C;
    CHECK(m.key(k) == walkmode::Act::Action && m.action() == walkmode::Do::Attack);
    k.vk = ' ', k.scan = 0x39;
    CHECK(m.key(k) == walkmode::Act::Swallow);  // Space: no action now, not typed either
    k.vk = 0xC0, k.scan = 0x29;
    CHECK(m.key(k) == walkmode::Act::Swallow && m.on());  // ` no longer switches it
}

#include "core/config.hpp"
#include "core/setting_change.hpp"
#include "core/settings/schema.hpp"

// [keys] read: each action's key by its word; a word not one of them leaves the default
TEST(key_binds_read_from_the_file) {
    const Config c = parseConfig("[keys]\nattack = z\nwalk_toggle = b\npick_up = w\n");
    CHECK_EQ(c.keys.scan[int(Bind::Attack)], 0x2Cu);
    CHECK_EQ(c.keys.scan[int(Bind::Toggle)], 0x30u);
    CHECK_EQ(c.keys.scan[int(Bind::Pickup)], 0x21u);  // «w» walks: not allowed, F kept
    for (int i = 0; i < keybinds::kBinds; ++i)
        CHECK(settings::findSetting(std::string("keys.") + keybinds::settingKey(Bind(i))) != nullptr);
}

// A key another action has: the two swap - never one key for two actions
TEST(key_binds_a_taken_key_swaps) {
    const Config c = parseConfig("");
    std::vector<settings::IniEdit> edits;
    CHECK(settingchange::edits(c, 3, "keys.attack", "f", edits) == settingchange::Outcome::Edits);
    bool attackF = false, pickupSpace = false;
    for (const settings::IniEdit& e : edits) {
        attackF = attackF || (e.section == "keys" && e.key == "attack" && e.value == "f");
        pickupSpace = pickupSpace || (e.section == "keys" && e.key == "pick_up" && e.value == "space");
    }
    CHECK(attackF && pickupSpace);
    edits.clear();
    CHECK(settingchange::edits(c, 3, "keys.attack", "z", edits) == settingchange::Outcome::Edits);
    CHECK_EQ(edits.size(), size_t(1));  // Z free: no swap
}

// The keys list's walk mode line by the player's keys - {1} the toggle ... {8} the camera's right, in Bind's order
TEST(key_binds_the_walk_line_names_the_players_keys) {
    keybinds::Binds b = keybinds::defaults();
    b.scan[int(Bind::Attack)] = 0x2C;  // Z
    const auto name = [](unsigned scan) {
        std::string t = keybinds::tokenOf(scan);
        for (char& c : t) c = char(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
        return t;
    };
    CHECK(keybinds::walkLine("{1} - mode: {2} attack, {6} target, {7} / {8} camera", b, name) ==
          "GRAVE - mode: Z attack, TAB target, Q / E camera");
    CHECK(keybinds::walkLine("no marks", b, name) == "no marks");
}

// 3.0.24-beta.56 (the owner's «давай», item 4): the keys list (the game's «Клавіші», the app's «Головне») with its walk
// mode line by the player's keys - the line beginning with «` » the form's; the keys named as the list names them
TEST(key_binds_the_keys_list_by_the_players_keys) {
    keybinds::Binds b = keybinds::defaults();
    b.scan[int(Bind::Target)] = 0x2C;  // Z
    b.scan[int(Bind::Toggle)] = 0x30;  // B
    const std::string list = "A - a\n` - walk: Tab target, Space attack\nB - b";
    CHECK(keybinds::keysList(list, "{1} - walk: {6} target, {2} attack", b, "Space") ==
          "A - a\nB - walk: Z target, Space attack\nB - b");
    CHECK(keybinds::keysList("no walk line", "{1} x", b, "Space") == "no walk line");
    CHECK(keybinds::keyName(0x29, "Space") == "`" && keybinds::keyName(0x39, "Пробіл") == "Пробіл");
    CHECK(keybinds::keyName(0x0F, "") == "Tab" && keybinds::keyName(0x2C, "") == "Z");
}
