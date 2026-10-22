#include "pad/core/config/bindings.hpp"
#include "test.hpp"

using namespace uapad;

namespace {

size_t indexOf(uint16_t button) {
    for (size_t i = 0; i < kBindableButtons.size(); ++i)
        if (kBindableButtons[i] == button) return i;
    return kBindableButtons.size();
}

Action slot(int s) { return Action{Action::Kind::Slot, uint8_t(s)}; }
Action kind(Action::Kind k) { return Action{k, 0}; }

}  // namespace

TEST(bindings_default_is_the_hunter_layout) {
    const Bindings b = defaultBindings();
    CHECK(b[indexOf(kPadA)] == kind(Action::Kind::Attack));
    CHECK(b[indexOf(kPadX)] == slot(0));
    CHECK(b[indexOf(kPadY)] == slot(1));
    CHECK(b[indexOf(kPadB)] == slot(2));
    CHECK(b[indexOf(kPadRB)] == slot(3));
    CHECK(b[indexOf(kPadRT)] == slot(4));
    CHECK(b[indexOf(kPadLB)] == slot(5));
    CHECK(b[indexOf(kPadUp)] == slot(6));
    CHECK(b[indexOf(kPadRight)] == slot(7));
    CHECK(b[indexOf(kPadDown)] == kind(Action::Kind::Talk));  // 0.7; slot F9 before
    CHECK(b[indexOf(kPadLT)] == kind(Action::Kind::Pickup));
    CHECK(b[indexOf(kPadLeft)] == kind(Action::Kind::SitStand));
    CHECK(b[indexOf(kPadR3)] == kind(Action::Kind::CameraToggle));  // the right stick: targets <-> camera
    CHECK(b[indexOf(kPadL3)] == kind(Action::Kind::ClearTarget));
}

TEST(bindings_action_ring_wraps) {
    CHECK(nextAction(kind(Action::Kind::None), -1) == kind(Action::Kind::Talk));
    CHECK(nextAction(kind(Action::Kind::Attack), +1) == slot(0));
    CHECK(nextAction(slot(0), -1) == kind(Action::Kind::Attack));
    CHECK(nextAction(slot(26), +1) == kind(Action::Kind::Pickup));
    CHECK(nextAction(kind(Action::Kind::ClearTarget), +1) == kind(Action::Kind::CameraToggle));
    CHECK(nextAction(kind(Action::Kind::CameraToggle), +1) == kind(Action::Kind::Talk));
    CHECK(nextAction(kind(Action::Kind::Talk), +1) == kind(Action::Kind::None));
    Action a = kind(Action::Kind::None);
    for (int i = 0; i < 34; ++i) a = nextAction(a, +1);
    CHECK(a == kind(Action::Kind::None));  // 34 actions in the ring
}

TEST(bindings_tokens_round_trip) {
    Action a = kind(Action::Kind::None);
    for (int i = 0; i < 33; ++i) {
        const auto back = parseAction(actionToken(a));
        CHECK(back && *back == a);
        a = nextAction(a, +1);
    }
    CHECK(actionToken(slot(11)) == "slot 12");
    CHECK(!parseAction("slot 0"));
    CHECK(!parseAction("slot 28"));
    CHECK(!parseAction("jump"));
    CHECK(parseAction("  Slot 3 ") && *parseAction("  Slot 3 ") == slot(2));
}

TEST(bindings_profiles_round_trip) {
    Profiles p;
    p.fallback[indexOf(kPadX)] = kind(Action::Kind::Attack);
    Profiles::Character hunter;
    hunter.name = "Hunter";
    hunter.bindings = defaultBindings();
    hunter.bindings[indexOf(kPadL3)] = slot(20);
    p.characters[150001] = hunter;
    const Profiles back = parseProfiles(formatProfiles(p));
    CHECK(back.fallback == p.fallback);
    CHECK_EQ(back.characters.size(), 1u);
    CHECK(back.characters.count(150001) && back.characters.at(150001).name == "Hunter");
    CHECK(back.characters.count(150001) && back.characters.at(150001).bindings == hunter.bindings);
}

TEST(bindings_missing_keys_fall_back) {
    const Profiles p = parseProfiles("[char 7]\nname = Mage\nB = attack\n[default]\nA = slot 3\n");
    CHECK(p.fallback[indexOf(kPadA)] == slot(2));
    CHECK(p.fallback[indexOf(kPadX)] == slot(0));  // not in [default]: built in
    const Bindings mage = p.forCharacter(7);
    CHECK(mage[indexOf(kPadB)] == kind(Action::Kind::Attack));
    CHECK(mage[indexOf(kPadA)] == slot(2));  // from [default], even though it comes later in the file
    CHECK(mage[indexOf(kPadY)] == slot(1));
    CHECK(p.forCharacter(8) == p.fallback);  // a character with no section of its own
}

TEST(bindings_bad_lines_are_skipped) {
    const Profiles p = parseProfiles(
        "\xEF\xBB\xBF; saved with a BOM\n"
        "[default]\nA = jump\nZZ = attack\nnot a line\nX = slot 99\n"
        "[char abc]\nA = attack\n"
        "[weird]\nA = attack\n"
        "[char 5]\nY = pickup ; comment\n");
    CHECK(p.fallback == defaultBindings());
    CHECK_EQ(p.characters.size(), 1u);
    CHECK(p.forCharacter(5)[indexOf(kPadY)] == kind(Action::Kind::Pickup));
}

TEST(bindings_labels) {
    CHECK(actionLabel(slot(0), Lang::Ru) == "Слот F1");
    CHECK(actionLabel(slot(11), Lang::Ru) == "Слот F3, ряд 2");
    CHECK(actionLabel(kind(Action::Kind::None), Lang::En) == "—");
    CHECK(std::string(buttonLabel(indexOf(kPadUp))) == "D-pad ↑");
    CHECK(std::string(buttonToken(indexOf(kPadRight))) == "RIGHT");
}

TEST(bindings_file_header_is_ascii) {  // one encoding in the file: the system one of the character names
    const std::string text = formatProfiles(Profiles{});
    bool ascii = true;
    for (unsigned char c : text) ascii = ascii && c < 0x80;
    CHECK(ascii);
    CHECK(text.find("; ") == 0);
    CHECK(parseProfiles(text).fallback == defaultBindings());
}

TEST(bindings_saving_one_layout_keeps_the_others) {
    Profiles disk;  // what another client already wrote
    disk.fallback[indexOf(kPadL3)] = slot(9);
    disk.characters[1].name = "Priest";
    disk.characters[1].bindings = defaultBindings();
    disk.characters[1].bindings[indexOf(kPadA)] = slot(1);
    Bindings hunter = defaultBindings();
    hunter[indexOf(kPadX)] = kind(Action::Kind::Attack);
    const Profiles merged = withLayout(disk, 2, "Hunter", hunter);
    CHECK(merged.characters.count(1) && merged.characters.at(1).bindings == disk.characters.at(1).bindings);
    CHECK(merged.characters.count(2) && merged.characters.at(2).bindings == hunter);
    CHECK(merged.characters.count(2) && merged.characters.at(2).name == "Hunter");
    CHECK(merged.fallback == disk.fallback);
    const Profiles def = withLayout(disk, 0, "", hunter);  // before login: the default layout
    CHECK(def.fallback == hunter);
    CHECK_EQ(def.characters.size(), 1u);
}

TEST(bindings_camera_toggle_token_and_labels) {
    CHECK(actionToken(kind(Action::Kind::CameraToggle)) == "camera");
    CHECK(actionLabel(kind(Action::Kind::CameraToggle), Lang::En) == "Camera / target");
    CHECK(actionLabel(kind(Action::Kind::CameraToggle), Lang::Ru) == "Камера / цель");
}

TEST(bindings_layouts_from_before_0_5_get_the_camera_on_r3) {
    const Profiles p = parseProfiles(
        "[default]\nR3 = untarget\nL3 = none\n"
        "[char 7]\nname = Hunter\nR3 = untarget\nL3 = none\nX = slot 2\n"
        "[char 8]\nR3 = untarget\nL3 = slot 10\n");  // L3 in use: it stays
    CHECK(p.fallback[indexOf(kPadR3)] == kind(Action::Kind::CameraToggle));
    CHECK(p.fallback[indexOf(kPadL3)] == kind(Action::Kind::ClearTarget));
    const Bindings hunter = p.forCharacter(7);
    CHECK(hunter[indexOf(kPadR3)] == kind(Action::Kind::CameraToggle));
    CHECK(hunter[indexOf(kPadL3)] == kind(Action::Kind::ClearTarget));
    CHECK(hunter[indexOf(kPadX)] == slot(1));  // the rest as saved
    const Bindings other = p.forCharacter(8);
    CHECK(other[indexOf(kPadR3)] == kind(Action::Kind::CameraToggle));
    CHECK(other[indexOf(kPadL3)] == slot(9));
    CHECK_EQ(p.movedToCamera, 3);
}

TEST(bindings_a_0_5_file_keeps_r3_as_saved) {
    const std::string text = formatProfiles(Profiles{});
    CHECK(text.find("[file]") != std::string::npos);
    const Profiles p = parseProfiles("[file]\nversion = 2\n[default]\nR3 = untarget\nL3 = none\n");
    CHECK(p.fallback[indexOf(kPadR3)] == kind(Action::Kind::ClearTarget));  // chosen in 0.5: left alone
    CHECK(p.fallback[indexOf(kPadL3)] == kind(Action::Kind::None));
    CHECK_EQ(p.movedToCamera, 0);
    CHECK_EQ(parseProfiles(text).movedToCamera, 0);
}

TEST(bindings_talk_round_trips_and_older_layouts_get_it_on_down) {
    CHECK(actionToken(kind(Action::Kind::Talk)) == "talk");
    CHECK(parseAction("talk") == kind(Action::Kind::Talk));
    CHECK(actionLabel(kind(Action::Kind::Talk), Lang::Ru) == "Говорить с NPC");
    // 0.6 and older: D-pad down on its default F9 becomes talk; one moved elsewhere, or a file of 0.7, stays.
    const Profiles old = parseProfiles("[file]\nversion = 2\n[default]\nDOWN = slot 9\n[char 7]\nDOWN = slot 3\n");
    CHECK(old.fallback[indexOf(kPadDown)] == kind(Action::Kind::Talk));
    CHECK(old.characters.at(7).bindings[indexOf(kPadDown)] == slot(2));
    CHECK_EQ(old.movedToTalk, 1);
    CHECK_EQ(old.movedToCamera, 0);
    const Profiles kept = parseProfiles("[file]\nversion = 3\n[default]\nDOWN = slot 9\n");
    CHECK(kept.fallback[indexOf(kPadDown)] == slot(8));
    CHECK_EQ(kept.movedToTalk, 0);
    CHECK(formatProfiles(Profiles{}).find("version = 3") != std::string::npos);
}
