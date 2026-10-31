#include <cmath>

#include "pad/core/config/bindings.hpp"
#include "pad/core/logic/npc_hint.hpp"
#include "pad_fixtures.hpp"
#include "test.hpp"

using namespace fx;

namespace {

constexpr double kPi = 3.14159265358979323846;

}  // namespace

// --- walking ---

TEST(ctl_walk_ring_point_and_stop) {
    Controller c(Config{});
    WorldState w = world();
    PadState p = pad();
    p.lx = 1.0f;  // full right
    TickOutput o = c.tick(p, w, view(), 100);
    CHECK(o.mouse.active && o.mouse.lmb);
    CHECK_EQ(o.mouse.x, 532);  // centre (400, 300) + 0.22 * 600 = 132 px
    CHECK_EQ(o.mouse.y, 300);
    p.lx = 0;
    p.ly = 1.0f;  // stick up -> screen up
    o = c.tick(p, w, view(), 150);
    CHECK_EQ(o.mouse.x, 400);
    CHECK_EQ(o.mouse.y, 168);
    o = c.tick(pad(), w, view(), 200);  // released
    CHECK(!o.mouse.active);
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x00a7);
    CHECK(decodePos3(pkt(o, 0).data(), 5) == (Cell{100, 100}));
}

// --- 3.0.11-beta.88: walking without the mouse (the concept «Контроллер в интерфейсе uaRO», stage 4) ---

TEST(ctl_direct_walk_along_the_stick) {
    Controller c(Config{});
    c.setDirectWalk(true);
    c.setLook(0, 1);  // the camera looks north: the stick's up is north, its right east
    WorldState w = world();
    PadState p = pad();
    p.lx = 1.0f;
    TickOutput o = c.tick(p, w, view(), 100);
    CHECK(!o.mouse.active);  // the mouse is left alone
    // 3.0.11-beta.102: 5 cells ahead at the game's speed (150 ms a cell), the same at any zoom
    CHECK(o.packets.size() == 1 && pid(pkt(o, 0)) == 0x00a7 && decodePos3(pkt(o, 0).data(), 5) == (Cell{105, 100}));
    CHECK(c.walkTarget() && *c.walkTarget() == (Cell{105, 100}));
    o = c.tick(p, w, view(), 200);  // still far from it: no new ask
    CHECK(o.packets.empty());
    c.setSelfCell(Cell{103, 100});  // the game's own cell: halfway there - asked on before it arrives and stands
    o = c.tick(p, w, view(), 400);
    CHECK(o.packets.size() == 1 && decodePos3(pkt(o, 0).data(), 5) == (Cell{108, 100}));
    p.lx = 0;
    p.ly = 1.0f;  // up: north - another way at once
    o = c.tick(p, w, view(), 520);
    CHECK(o.packets.size() == 1 && decodePos3(pkt(o, 0).data(), 5) == (Cell{103, 105}));
    o = c.tick(pad(), w, view(), 700);  // let go: the cell it steps into, no further
    CHECK(o.packets.size() == 1 && pid(pkt(o, 0)) == 0x00a7 && decodePos3(pkt(o, 0).data(), 5) == (Cell{103, 101}));
    CHECK(!c.walkTarget());
}

TEST(ctl_direct_walk_turns_with_the_camera) {
    Controller c(Config{});
    c.setDirectWalk(true);
    c.setLook(1, 0);  // the camera looks east: the stick's up is east, its right south
    WorldState w = world();
    PadState p = pad();
    p.ly = 1.0f;
    TickOutput o = c.tick(p, w, view(), 100);
    CHECK(!o.packets.empty() && decodePos3(pkt(o, 0).data(), 5) == (Cell{105, 100}));
    p.ly = 0;
    p.lx = 1.0f;
    o = c.tick(p, w, view(), 300);
    CHECK(!o.packets.empty() && decodePos3(pkt(o, 0).data(), 5) == (Cell{100, 95}));
}

TEST(ctl_direct_walk_stops_short_of_a_wall_and_goes_round_a_post) {
    Controller c(Config{});
    c.setDirectWalk(true);
    c.setLook(0, 1);
    GroundGrid g;
    g.w = g.h = 200;
    g.walk.assign(200 * 200, 1);
    for (int y = 0; y < 200; ++y) g.walk[size_t(y) * 200 + 104] = 0;  // a wall at x 104
    c.setGround(&g);
    WorldState w = world();
    PadState p = pad();
    p.lx = 1.0f;
    TickOutput o = c.tick(p, w, view(), 100);
    CHECK(!o.packets.empty() && decodePos3(pkt(o, 0).data(), 5) == (Cell{103, 100}));
    g.walk[size_t(100) * 200 + 101] = 0;  // a post right in front: round it, short of the wall
    o = c.tick(p, w, view(), 500);
    CHECK(o.packets.size() == 1 && decodePos3(pkt(o, 0).data(), 5) == (Cell{103, 103}));
    CHECK(c.walkTarget() && *c.walkTarget() == (Cell{103, 103}));
    for (int x = 99; x <= 101; ++x)  // boxed in: nowhere to go, nothing asked
        for (int y = 99; y <= 101; ++y)
            if (x != 100 || y != 100) g.walk[size_t(y) * 200 + size_t(x)] = 0;
    o = c.tick(p, w, view(), 1700);
    CHECK(o.packets.empty() && c.walkTarget() && *c.walkTarget() == (Cell{100, 100}));
}

TEST(ctl_walk_radius_changes_at_once) {
    Controller c(Config{});
    WorldState w = world();
    PadState right = pad();
    right.lx = 1.0f;
    TickOutput o = c.tick(right, w, view(), 1000);  // 800x600: 22% of 600 = 132 px right of the centre
    CHECK_EQ(o.mouse.x, 400 + 132);
    c.setWalkRadius(0.10f);
    o = c.tick(right, w, view(), 1100);
    CHECK_EQ(o.mouse.x, 400 + 60);
}

TEST(ctl_walk_stop_note_tells_where) {
    Controller c(Config{});
    WorldState w = world();
    PadState p = pad();
    p.lx = 1.0f;
    c.tick(p, w, view(), 100);
    CHECK(c.walking());
    w.apply(EvSelfWalk{Cell{100, 100}, Cell{108, 100}}, 120);  // the server walks us east
    TickOutput o = c.tick(pad(), w, view(), 450);              // released: 2 cells walked by now
    CHECK(!c.walking());
    bool found = false;
    for (const std::string& n : o.notes)
        found = found || n == "walk stop at (104,100): we are at about (102,100), heading for (108,100)";
    CHECK(found);
}

TEST(ctl_walk_stop_goes_two_cells_ahead) {
    Controller c(Config{});
    WorldState w = world();
    PadState p = pad();
    p.lx = 1.0f;
    c.tick(p, w, view(), 100);
    c.onClientWalk(Cell{108, 100}, w, 110);  // the client asks for the cell under the walking point
    w.apply(EvSelfWalk{Cell{100, 100}, Cell{108, 100}}, 120);
    TickOutput o = c.tick(pad(), w, view(), 450);  // released: we think we are at (102,100)
    CHECK_EQ(o.packets.size(), 1u);
    CHECK(decodePos3(pkt(o, 0).data(), 5) == (Cell{104, 100}));  // the server is a step or two on already
}

TEST(ctl_walk_stop_not_past_the_destination) {
    Controller c(Config{});
    WorldState w = world();
    PadState p = pad();
    p.lx = 1.0f;
    c.tick(p, w, view(), 100);
    c.onClientWalk(Cell{101, 100}, w, 110);
    w.apply(EvSelfWalk{Cell{100, 100}, Cell{101, 100}}, 120);
    TickOutput o = c.tick(pad(), w, view(), 140);
    CHECK(decodePos3(pkt(o, 0).data(), 5) == (Cell{101, 100}));
}

TEST(ctl_quick_tap_stops_two_cells_out) {
    Controller c(Config{});
    WorldState w = world();
    PadState p = pad();
    p.lx = 1.0f;
    c.tick(p, w, view(), 100);
    c.onClientWalk(Cell{108, 100}, w, 110);  // asked for, but the server has not answered yet
    TickOutput o = c.tick(pad(), w, view(), 150);
    CHECK(decodePos3(pkt(o, 0).data(), 5) == (Cell{102, 100}));
}

TEST(ctl_walk_deadzone_and_center_press) {
    Config cfg;
    cfg.press = PressMode::Center;
    Controller c(cfg);
    WorldState w = world();
    PadState p = pad();
    p.lx = 0.2f;  // inside the 0.25 dead zone
    CHECK(!c.tick(p, w, view(), 10).mouse.active);
    p.lx = -1.0f;
    TickOutput o = c.tick(p, w, view(), 20);
    CHECK(o.mouse.active && o.mouse.lmb);
    CHECK_EQ(o.mouse.x, 400);  // the first frame presses at the centre
    CHECK_EQ(o.mouse.y, 300);
    o = c.tick(p, w, view(), 30);
    CHECK_EQ(o.mouse.x, 268);
    CHECK_EQ(o.mouse.y, 300);
}

TEST(ctl_inactive_when_unfocused_disabled_or_disconnected) {
    Controller c(Config{});
    WorldState w = world();
    PadState p = pad();
    p.lx = 1.0f;
    ViewInfo unfocused = view();
    unfocused.focused = false;
    CHECK(!c.tick(p, w, unfocused, 10).mouse.active);
    PadState off = p;
    off.connected = false;
    CHECK(!c.tick(off, w, view(), 20).mouse.active);
    TickOutput o = c.tick(pad(kPadBack), w, view(), 30);
    CHECK(!c.enabled());
    CHECK(o.rumble == Rumble::PadOff);
    CHECK(!c.tick(p, w, view(), 40).mouse.active);
    o = c.tick(pad(kPadBack), w, view(), 50);
    CHECK(c.enabled());
    CHECK(o.rumble == Rumble::PadOn);
    Config cfg;
    cfg.enabled = false;
    Controller startsOff(cfg);
    CHECK(!startsOff.enabled());
}

TEST(ctl_nothing_happens_while_dead_or_before_login) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{102, 100});
    w.apply(EvVanish{kSelf, 1}, 0);
    PadState p = pad(kPadA);
    p.lx = 1.0f;
    TickOutput o = c.tick(p, w, view(), 1000);
    CHECK(!o.mouse.active);
    CHECK(o.packets.empty());
    WorldState notYet;
    notYet.reset(kSelf);
    CHECK(!c.tick(p, notYet, view(), 1100).mouse.active);
}

TEST(ctl_buttons_held_before_focus_returns_do_nothing) {
    Controller c(Config{});
    WorldState w = slotWorld();
    addMob(w, 110000001, Cell{101, 100});
    ViewInfo unfocused = view();
    unfocused.focused = false;
    c.tick(pad(kPadA | kPadY), w, unfocused, 1000);  // pressed while the game window was in the background
    TickOutput o = c.tick(pad(kPadA | kPadY), w, view(), 1100);
    CHECK(o.packets.empty());
    CHECK_EQ(c.target(), 0u);
    o = c.tick(pad(kPadA | kPadY), w, view(), 2000);  // no repeat and no held attack either
    CHECK(o.packets.empty());
    CHECK_EQ(c.target(), 0u);
}

// --- A: attack ---

TEST(ctl_attack_takes_nearest_and_marks_it) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000003, Cell{106, 100});  // distance 6
    addMob(w, 110000001, Cell{102, 101});  // distance 2
    TickOutput o = c.tick(pad(kPadA), w, view(), 1000);
    CHECK_EQ(c.target(), 110000001u);
    CHECK(c.attacking());
    CHECK(o.rumble == Rumble::TargetSwitch);
    CHECK_EQ(o.mark, 110000001u);
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0190);
    CHECK_EQ(rd32(pkt(o, 0).data(), 5), 110000001u);
    CHECK_EQ(pkt(o, 0)[18], 7);
    o = c.tick(pad(), w, view(), 1100);  // released: the server keeps attacking on its own
    CHECK(o.packets.empty());
    CHECK_EQ(o.mark, 0u);
}

TEST(ctl_attack_tap_stops_when_target_dies) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{102, 100});
    addMob(w, 110000002, Cell{104, 100});
    c.tick(pad(kPadA), w, view(), 1000);
    c.tick(pad(), w, view(), 1100);
    w.apply(EvVanish{110000001, 1}, 1200);
    TickOutput o = c.tick(pad(), w, view(), 1300);
    CHECK_EQ(c.target(), 0u);
    CHECK(!c.attacking());
    CHECK(o.packets.empty());
    CHECK(c.tick(pad(), w, view(), 2000).packets.empty());  // the next one is not taken
}

TEST(ctl_held_attack_moves_on_to_next) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{102, 100});
    addMob(w, 110000002, Cell{104, 100});
    c.tick(pad(kPadA), w, view(), 1000);
    c.tick(pad(kPadA), w, view(), 1100);
    w.apply(EvVanish{110000001, 1}, 1200);
    TickOutput o = c.tick(pad(kPadA), w, view(), 1400);
    CHECK_EQ(c.target(), 110000002u);
    CHECK_EQ(o.mark, 110000002u);
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0190);
    CHECK_EQ(rd32(pkt(o, 0).data(), 5), 110000002u);
}

TEST(ctl_target_further_than_15_cells_is_dropped) {
    Controller c(Config{});
    WorldState w = world();  // we stand at (100, 100)
    addMob(w, 110000001, Cell{102, 100});
    c.tick(pad(kPadA), w, view(), 1000);
    c.tick(pad(), w, view(), 1100);
    w.apply(EvSetPos{110000001, Cell{109, 100}}, 1200);  // 9 cells: a hunter shooting from afar keeps it
    c.tick(pad(), w, view(), 1300);
    CHECK_EQ(c.target(), 110000001u);
    w.apply(EvSetPos{110000001, Cell{116, 100}}, 1400);  // 16 cells
    TickOutput o = c.tick(pad(), w, view(), 1500);
    CHECK_EQ(c.target(), 0u);
    CHECK(!c.attacking());
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0118);  // the server may be chasing it: stop that too
    bool noted = false;
    for (const std::string& n : o.notes) noted = noted || n.find("too far") != std::string::npos;
    CHECK(noted);
}

TEST(ctl_held_attack_leaves_a_far_target_for_the_nearest) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{102, 100});
    addMob(w, 110000002, Cell{97, 100});
    c.tick(pad(kPadA), w, view(), 1000);
    CHECK_EQ(c.target(), 110000001u);
    w.apply(EvSetPos{110000001, Cell{120, 100}}, 1100);
    TickOutput o = c.tick(pad(kPadA), w, view(), 1400);
    CHECK_EQ(c.target(), 110000002u);
    CHECK_EQ(o.packets.size(), 2u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0118);
    CHECK_EQ(pid(pkt(o, 1)), 0x0190);
    CHECK_EQ(rd32(pkt(o, 1).data(), 5), 110000002u);
}

TEST(ctl_slot_does_not_cast_on_a_far_target) {
    Config cfg;
    Controller c(cfg);
    WorldState w = slotWorld();  // X: Bash (range 1)
    addMob(w, 110000001, Cell{101, 100});
    addMob(w, 110000002, Cell{99, 100});
    c.tick(pad(kPadA), w, view(), 1000);
    c.tick(pad(), w, view(), 1100);
    CHECK_EQ(c.target(), 110000001u);
    w.apply(EvSetPos{110000001, Cell{100, 120}}, 1200);
    TickOutput o = c.tick(pad(kPadX), w, view(), 1300);
    CHECK_EQ(c.target(), 110000002u);
    bool cast = false;
    for (const Bytes& p : o.packets) cast = cast || (pid(p) == 0x0072 && rd32(p.data(), 21) == 110000002u);
    CHECK(cast);
}

TEST(ctl_held_attack_leaves_monsters_beyond_reach_alone) {
    Controller c(Config{});
    WorldState w = world();  // the server shows monsters up to 35 cells away (uAthena area_size)
    addMob(w, 110000001, Cell{120, 100});
    TickOutput o = c.tick(pad(kPadA), w, view(), 1000);
    CHECK(o.rumble == Rumble::Denied);  // the press: no monster within reach
    CHECK(o.packets.empty());
    int packets = 0;
    int rumbles = 0;
    for (uint32_t t = 1016; t < 2000; t += 16) {
        o = c.tick(pad(kPadA), w, view(), t);
        packets += int(o.packets.size());
        rumbles += o.rumble != Rumble::None ? 1 : 0;
    }
    CHECK_EQ(packets, 0);
    CHECK_EQ(rumbles, 0);
    CHECK_EQ(c.target(), 0u);
    addMob(w, 110000002, Cell{104, 100});  // one comes into reach: the held A takes it
    o = c.tick(pad(kPadA), w, view(), 2100);
    CHECK_EQ(c.target(), 110000002u);
}

TEST(ctl_slot_with_only_a_far_monster_is_denied) {
    Controller c(Config{});
    WorldState w = slotWorld();  // X: Bash (range 1)
    addMob(w, 110000001, Cell{100, 120});
    TickOutput o = c.tick(pad(kPadX), w, view(), 1000);
    CHECK(o.rumble == Rumble::Denied);
    bool walk = false;
    for (const Bytes& p : o.packets) walk = walk || pid(p) == 0x00a7;
    CHECK(!walk);
    CHECK_EQ(c.target(), 0u);
}

// --- which monster a press goes for: the nearest, unless it is being fought or was picked with the right stick

TEST(ctl_press_takes_the_nearest_not_a_remembered_target) {
    Controller c(Config{});
    WorldState w = world();  // we stand at (100, 100)
    addMob(w, 110000001, Cell{102, 100});
    c.tick(pad(kPadA), w, view(), 1000);  // fights 110000001
    c.tick(pad(), w, view(), 1100);
    PadState walk = pad();
    walk.lx = 1.0f;
    c.tick(walk, w, view(), 1200);  // the stick walk ends the fight; the monster stays remembered
    c.tick(pad(), w, view(), 1300);
    w.apply(EvSetPos{110000001, Cell{108, 100}}, 1400);
    addMob(w, 110000002, Cell{98, 101});  // now the nearest
    TickOutput o = c.tick(pad(kPadA), w, view(), 1500);
    CHECK_EQ(c.target(), 110000002u);
    CHECK_EQ(o.mark, 110000002u);
}

TEST(ctl_press_keeps_the_monster_under_attack) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{103, 100});
    c.tick(pad(kPadA), w, view(), 1000);
    c.tick(pad(), w, view(), 1100);  // a tap fights it until it is gone
    addMob(w, 110000002, Cell{101, 100});  // a nearer one turns up
    c.tick(pad(kPadA), w, view(), 1200);
    CHECK_EQ(c.target(), 110000001u);
}

TEST(ctl_press_keeps_a_right_stick_pick) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{108, 100});  // east, 8 cells
    addMob(w, 110000002, Cell{98, 100});   // west, 2 cells
    c.tick(padR(1, 0), w, view(), 1000);   // picked on purpose
    c.tick(pad(), w, view(), 1100);
    c.tick(pad(kPadA), w, view(), 1200);
    CHECK_EQ(c.target(), 110000001u);
}

TEST(ctl_slot_takes_the_nearest_when_not_fighting) {
    Controller c(Config{});
    WorldState w = slotWorld();  // X: Bash (range 1)
    addMob(w, 110000001, Cell{101, 100});
    c.tick(pad(kPadX), w, view(), 1000);  // Bash on the nearest: it becomes the target
    c.tick(pad(), w, view(), 1100);
    CHECK_EQ(c.target(), 110000001u);
    w.apply(EvSetPos{110000001, Cell{104, 100}}, 1200);
    addMob(w, 110000002, Cell{99, 100});
    TickOutput o = c.tick(pad(kPadX), w, view(), 1700);
    CHECK_EQ(c.target(), 110000002u);
    bool cast = false;
    for (const Bytes& p : o.packets) cast = cast || (pid(p) == 0x0072 && rd32(p.data(), 21) == 110000002u);
    CHECK(cast);
}

TEST(ctl_press_logs_why_it_took_that_monster) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{103, 104});  // 5.0 in a straight line, 4 RO steps
    addMob(w, 110000002, Cell{95, 100});   // 5.0, 5 steps
    TickOutput o = c.tick(pad(kPadA), w, view(), 1000);
    CHECK_EQ(c.target(), 110000001u);
    bool told = false;
    for (const std::string& n : o.notes)
        told = told || (n.find("nearest of 2") != std::string::npos &&
                        n.find("110000001 at (103,104) 5.0") != std::string::npos &&
                        n.find("110000002 at (95,100) 5.0") != std::string::npos &&
                        n.find("we are at (100,100)") != std::string::npos);
    CHECK(told);
}

TEST(ctl_candidates_are_monsters_only) {
    Controller c(Config{});
    WorldState w = world();
    w.apply(EvActor{2000002, 7, 3, 150, Cell{101, 100}, Cell{101, 100}}, 0);          // player
    w.apply(EvActor{110000009, 1002, 100, 200, Cell{101, 101}, Cell{101, 101}}, 0);  // pet
    addMob(w, 110000001, Cell{105, 100});
    c.tick(pad(kPadA), w, view(), 100);
    CHECK_EQ(c.target(), 110000001u);
}

TEST(ctl_no_monsters_denied) {
    Controller c(Config{});
    WorldState w = world();
    TickOutput o = c.tick(pad(kPadA), w, view(), 100);
    CHECK_EQ(c.target(), 0u);
    CHECK(o.packets.empty());
    CHECK(o.rumble == Rumble::Denied);
}

TEST(ctl_held_attack_with_no_monsters_waits_quietly) {
    Controller c(Config{});
    WorldState w = world();
    TickOutput o = c.tick(pad(kPadA), w, view(), 1000);
    CHECK(o.rumble == Rumble::Denied);
    o = c.tick(pad(kPadA), w, view(), 1300);  // still held: no buzzing
    CHECK(o.rumble == Rumble::None);
    CHECK(o.packets.empty());
    addMob(w, 110000001, Cell{102, 100});
    o = c.tick(pad(kPadA), w, view(), 1600);  // one shows up while A is held
    CHECK_EQ(c.target(), 110000001u);
    CHECK_EQ(o.packets.size(), 1u);
    c.tick(pad(), w, view(), 1700);
    w.apply(EvVanish{110000001, 1}, 1800);
    c.tick(pad(), w, view(), 1900);
    CHECK(!c.attacking());
    addMob(w, 110000002, Cell{105, 100});
    o = c.tick(padR(1, 0), w, view(), 2000);  // picking a new target later does not attack it
    CHECK_EQ(c.target(), 110000002u);
    CHECK(o.packets.empty());
}

TEST(ctl_attack_throttled) {
    Controller c(Config{});  // 300 ms between attack requests
    WorldState w = world();
    addMob(w, 110000001, Cell{102, 100});
    TickOutput o1 = c.tick(pad(kPadA), w, view(), 1000);
    c.tick(pad(), w, view(), 1050);
    TickOutput o2 = c.tick(pad(kPadA), w, view(), 1100);
    c.tick(pad(), w, view(), 1150);
    TickOutput o3 = c.tick(pad(kPadA), w, view(), 1400);
    CHECK_EQ(o1.packets.size(), 1u);
    CHECK(o2.packets.empty());
    CHECK_EQ(o3.packets.size(), 1u);
}

TEST(ctl_held_attack_gives_up_on_unreachable_target) {
    Controller c(Config{});  // give up after 4 s, blacklist for 10 s
    WorldState w = world();
    addMob(w, 110000001, Cell{102, 100});  // e.g. an NPC with a monster sprite: our hits never land
    addMob(w, 110000002, Cell{105, 100});
    c.tick(pad(kPadA), w, view(), 1000);
    CHECK_EQ(c.target(), 110000001u);
    c.tick(pad(kPadA), w, view(), 3000);
    CHECK_EQ(c.target(), 110000001u);  // 2 s without hits: keep trying
    TickOutput o = c.tick(pad(kPadA), w, view(), 5000);
    CHECK_EQ(c.target(), 110000002u);  // 4 s without hits: the next one, since A is held
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(rd32(pkt(o, 0).data(), 5), 110000002u);
    w.apply(EvAction{kSelf, 110000002, 0}, 8000);  // our hit lands
    c.tick(pad(kPadA), w, view(), 9500);
    CHECK_EQ(c.target(), 110000002u);
    w.apply(EvVanish{110000002, 1}, 9600);  // it died
    c.tick(pad(kPadA), w, view(), 9700);
    CHECK_EQ(c.target(), 0u);  // 110000001 stays blacklisted until 15000
    c.tick(pad(kPadA), w, view(), 15000);
    CHECK_EQ(c.target(), 110000001u);
}

TEST(ctl_attack_tap_gives_up_without_taking_next) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{102, 100});
    addMob(w, 110000002, Cell{105, 100});
    c.tick(pad(kPadA), w, view(), 1000);
    c.tick(pad(), w, view(), 1100);
    TickOutput o = c.tick(pad(), w, view(), 5000);
    CHECK_EQ(c.target(), 0u);
    CHECK(!c.attacking());
    CHECK(o.packets.empty());
}

TEST(ctl_walking_interrupts_attack) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{102, 100});
    c.tick(pad(kPadA), w, view(), 1000);
    c.tick(pad(), w, view(), 1100);
    PadState p = pad();
    p.ly = -1.0f;
    c.tick(p, w, view(), 1500);
    TickOutput o = c.tick(pad(), w, view(), 2000);
    CHECK_EQ(o.packets.size(), 1u);  // only the stop request
    CHECK_EQ(pid(pkt(o, 0)), 0x00a7);
    CHECK_EQ(c.target(), 110000001u);  // the target stays for the next A
    CHECK(!c.attacking());
}

TEST(ctl_held_attack_resumes_after_walk) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{102, 100});
    c.tick(pad(kPadA), w, view(), 1000);
    PadState p = pad(kPadA);
    p.lx = 1.0f;
    TickOutput o = c.tick(p, w, view(), 1500);
    CHECK(o.mouse.active);
    CHECK(o.packets.empty());  // no attack while walking
    o = c.tick(pad(kPadA), w, view(), 2000);  // stick released, A still held: stop, then attack again
    CHECK_EQ(o.packets.size(), 2u);
    CHECK_EQ(pid(pkt(o, 0)), 0x00a7);
    CHECK_EQ(pid(pkt(o, 1)), 0x0190);
}

TEST(ctl_held_attack_waits_while_sitting) {
    Controller c(Config{});
    WorldState w = world();
    w.apply(EvAction{kSelf, 0, 2}, 0);  // sitting
    addMob(w, 110000001, Cell{102, 100});
    TickOutput o = c.tick(pad(kPadA), w, view(), 1000);
    CHECK_EQ(c.target(), 110000001u);
    CHECK(o.packets.empty());
    w.apply(EvAction{kSelf, 0, 3}, 1100);  // stood up
    o = c.tick(pad(kPadA), w, view(), 1200);
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0190);
}

TEST(ctl_timers_survive_tick_wraparound) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{102, 100});
    addMob(w, 110000002, Cell{105, 100});
    const uint32_t t0 = 0xFFFFFC18u;  // GetTickCount() wraps 1000 ms later
    c.tick(pad(kPadA), w, view(), t0);
    CHECK_EQ(c.target(), 110000001u);
    c.tick(pad(kPadA), w, view(), t0 + 3000u);  // past zero, 3 s elapsed
    CHECK_EQ(c.target(), 110000001u);
    c.tick(pad(kPadA), w, view(), t0 + 4000u);  // 4 s without hits
    CHECK_EQ(c.target(), 110000002u);
}

TEST(ctl_skill_during_attack_resumes_it) {
    Controller c(Config{});
    WorldState w = slotWorld();
    addMob(w, 110000001, Cell{103, 100});  // within Fire Bolt's 9 cells
    c.tick(pad(kPadA), w, view(), 1000);
    c.tick(pad(), w, view(), 1100);
    w.apply(EvAction{kSelf, 110000001, 0}, 1500);         // our hit
    TickOutput o = c.tick(pad(kPadUp), w, view(), 2000);  // F7: Fire Bolt; the server ends the attack for it
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0072);
    CHECK(c.tick(pad(), w, view(), 2200).packets.empty());
    o = c.tick(pad(), w, view(), 2300);  // 300 ms after the skill: the attack is asked for again
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0190);
    CHECK_EQ(rd32(pkt(o, 0).data(), 5), 110000001u);
}

TEST(ctl_client_skill_during_attack_resumes_it) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{102, 100});
    c.tick(pad(kPadA), w, view(), 1000);
    c.tick(pad(), w, view(), 1100);
    w.apply(EvAction{kSelf, 110000001, 0}, 1500);
    c.onClientSkill(1600);  // a skill from the keyboard ends the attack too
    CHECK(c.tick(pad(), w, view(), 1800).packets.empty());
    TickOutput o = c.tick(pad(), w, view(), 1900);
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0190);
    Controller calm(Config{});
    calm.onClientSkill(100);  // not attacking: nothing to resume
    CHECK(calm.tick(pad(), w, view(), 500).packets.empty());
}

TEST(ctl_attack_asked_again_when_hits_stop) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{102, 100});
    c.tick(pad(kPadA), w, view(), 1000);
    c.tick(pad(), w, view(), 1100);
    w.apply(EvAction{kSelf, 110000001, 0}, 1500);  // our hit
    CHECK(c.tick(pad(), w, view(), 2400).packets.empty());  // 0.9 s since the hit: fine
    TickOutput o = c.tick(pad(), w, view(), 2500);         // 1 s without a hit: the attack stopped somehow
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0190);
    CHECK(c.tick(pad(), w, view(), 2600).packets.empty());
    w.apply(EvSelfWalk{Cell{100, 100}, Cell{101, 100}}, 3400);  // the client walks us up to it
    CHECK(c.tick(pad(), w, view(), 3500).packets.empty());     // a request would stop that walk
    o = c.tick(pad(), w, view(), 3600);                        // arrived, still no hit: ask again
    CHECK_EQ(o.packets.size(), 1u);
}

TEST(ctl_give_up_counts_from_the_first_request) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{102, 100});  // our hits never land
    c.tick(pad(kPadA), w, view(), 1000);
    c.tick(pad(), w, view(), 1100);
    CHECK_EQ(c.tick(pad(), w, view(), 2000).packets.size(), 1u);  // asked again
    CHECK_EQ(c.tick(pad(), w, view(), 3000).packets.size(), 1u);  // and again
    c.tick(pad(), w, view(), 5000);  // 4 s after the first request: give up anyway
    CHECK_EQ(c.target(), 0u);
}

TEST(ctl_mouse_click_hands_the_fight_to_the_mouse) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{102, 100});
    c.tick(pad(kPadA), w, view(), 1000);
    c.tick(pad(), w, view(), 1100);
    ViewInfo clicked = view();
    clicked.mouseClicked = true;  // the player clicked with the real mouse, e.g. to walk away
    c.tick(pad(), w, clicked, 1200);
    CHECK(!c.attacking());
    CHECK_EQ(c.target(), 110000001u);  // the target stays for A and the skills
    CHECK(c.tick(pad(), w, view(), 3000).packets.empty());  // nothing pulls the player back
}

// --- slots ---

TEST(ctl_slot_buttons_cover_the_first_row) {
    Controller c(Config{});
    WorldState w = slotWorld();
    addMob(w, 110000001, Cell{101, 100});
    uint32_t t = 1000;
    auto tap = [&](const PadState& p) {
        TickOutput o = c.tick(p, w, view(), t);
        c.tick(pad(), w, view(), t + 10);
        t += 200;
        return o;
    };
    auto one = [](const TickOutput& o, uint16_t id) { return o.packets.size() == 1 && pid(pkt(o, 0)) == id; };
    TickOutput o = tap(pad(kPadX));  // F1: Bash on the nearest monster, which becomes the target
    CHECK(one(o, 0x0072) && rd16(pkt(o, 0).data(), 10) == 5);
    CHECK_EQ(c.target(), 110000001u);
    o = tap(pad(kPadY));  // F2: Heal on ourselves
    CHECK(one(o, 0x0072) && rd16(pkt(o, 0).data(), 10) == 28 && rd32(pkt(o, 0).data(), 21) == kSelf);
    o = tap(pad(kPadB));  // F3: Safety Wall under ourselves
    CHECK(one(o, 0x0113) && rd16(pkt(o, 0).data(), 9) == 12 && rd16(pkt(o, 0).data(), 12) == 100);
    o = tap(pad(kPadRB));  // F4: Thunderstorm on the target's cell
    CHECK(one(o, 0x0113) && rd16(pkt(o, 0).data(), 9) == 21 && rd16(pkt(o, 0).data(), 12) == 101);
    o = tap(padRT(0));  // F5: Red Potion
    CHECK(one(o, 0x009f));
    o = tap(pad(kPadLB));  // F6: Orange Potion, none in the inventory
    CHECK(o.packets.empty() && o.rumble == Rumble::Denied);
    o = tap(pad(kPadUp));  // F7: Fire Bolt on the target
    CHECK(one(o, 0x0072) && rd16(pkt(o, 0).data(), 10) == 19 && rd32(pkt(o, 0).data(), 21) == 110000001u);
    o = tap(pad(kPadRight));  // F8: empty
    CHECK(o.packets.empty() && o.rumble == Rumble::Denied);
    Bindings b = defaultBindings();  // 0.7: D-pad down talks to NPCs by default; F9 there as before
    for (size_t i = 0; i < kBindableCount; ++i)
        if (kBindableButtons[i] == kPadDown) b[i] = Action{Action::Kind::Slot, 8};
    c.setBindings(b);
    o = tap(pad(kPadDown));  // F9: Ankle Snare, next to us towards the target
    CHECK(one(o, 0x0113) && rd16(pkt(o, 0).data(), 9) == 117);
    CHECK(!c.attacking());  // no button but A starts an attack
}

TEST(ctl_slot_press_note_says_what_went_out) {
    Controller c(Config{});
    WorldState w = slotWorld();
    TickOutput o = c.tick(pad(kPadY), w, view(), 1000);  // F2: Heal
    CHECK(!o.notes.empty() && o.notes.back() == "F2: skill 28 lv 5");
    o = c.tick(pad(kPadY), w, view(), 1250);  // the held repeat stays quiet
    CHECK_EQ(o.packets.size(), 1u);
    CHECK(o.notes.empty());
    o = c.tick(padRT(0), w, view(), 1400);  // F5: Red Potion
    CHECK(!o.notes.empty() && o.notes.back() == "F5: item 501");
}

// --- 3.0.11-beta.85: a support skill on a party member (the concept's «Выбор цели навыка») ---

namespace {
constexpr uint32_t kMate = 2000002, kMate2 = 2000003, kAway = 2000004;
WorldState partyWorld() {
    WorldState w = slotWorld();
    w.apply(EvActor{kMate, 4, 0, 150, Cell{103, 100}, Cell{103, 100}}, 0);   // an acolyte 3 cells away
    w.apply(EvActor{kMate2, 7, 0, 150, Cell{100, 120}, Cell{100, 120}}, 0);  // a knight 20 cells away
    return w;
}
}  // namespace

TEST(ctl_support_skill_waits_for_a_party_member) {
    Controller c(Config{});
    WorldState w = partyWorld();
    c.setFriends({kMate, kMate2, kAway});  // kAway: in the party, not in sight - not offered
    TickOutput o = c.tick(pad(kPadY), w, view(), 1000);  // F2: Heal - whom?
    CHECK(o.packets.empty());
    CHECK_EQ(c.choosing(), kMate);
    CHECK(!o.notes.empty() && o.notes.back() == "F2: skill 28 - whom? 3 to choose");
    c.tick(pad(), w, view(), 1100);
    o = c.tick(pad(kPadA), w, view(), 1200);  // A: on the chosen one
    CHECK(o.packets.size() == 1 && pid(pkt(o, 0)) == 0x0072 && rd16(pkt(o, 0).data(), 10) == 28 &&
          rd32(pkt(o, 0).data(), 21) == kMate);
    CHECK_EQ(c.choosing(), 0u);
    c.tick(pad(kPadA), w, view(), 1300);
    CHECK(!c.attacking());  // A cast; held on, it does not attack
}

TEST(ctl_support_skill_again_casts_on_the_character) {
    Controller c(Config{});
    WorldState w = partyWorld();
    c.setFriends({kMate});
    c.tick(pad(kPadY), w, view(), 1000);
    c.tick(pad(), w, view(), 1100);
    CHECK_EQ(c.choosing(), kMate);
    TickOutput o = c.tick(pad(kPadY), w, view(), 1200);  // the skill's own button again: on the character
    CHECK(o.packets.size() == 1 && rd32(pkt(o, 0).data(), 21) == kSelf);
    CHECK_EQ(c.choosing(), 0u);
    o = c.tick(pad(kPadY), w, view(), 1700);  // held on: no repeat until pressed again
    CHECK(o.packets.empty());
}

TEST(ctl_support_choice_moves_with_the_dpad_and_cancels) {
    Controller c(Config{});
    WorldState w = partyWorld();
    c.setFriends({kMate, kMate2});
    uint32_t t = 1000;
    auto tap = [&](uint16_t b) {
        TickOutput o = c.tick(pad(b), w, view(), t);
        c.tick(pad(), w, view(), t + 10);
        t += 100;
        return o;
    };
    tap(kPadY);
    CHECK_EQ(c.choosing(), kMate);
    CHECK(tap(kPadDown).packets.empty());  // D-pad down: the next one, no talking to an NPC
    CHECK_EQ(c.choosing(), kMate2);
    tap(kPadDown);
    CHECK_EQ(c.choosing(), kSelf);  // the character last
    tap(kPadDown);
    CHECK_EQ(c.choosing(), kMate);  // round again
    tap(kPadUp);
    CHECK_EQ(c.choosing(), kSelf);
    TickOutput o = tap(kPadX);  // another skill's button does nothing meanwhile
    CHECK(o.packets.empty());
    CHECK_EQ(c.choosing(), kSelf);
    o = tap(kPadB);  // B: cancelled, its own slot (F3) not used
    CHECK(o.packets.empty());
    CHECK_EQ(c.choosing(), 0u);
    CHECK(!o.notes.empty() && o.notes.back() == "F2: the choice cancelled");
}

TEST(ctl_support_choice_far_member_walks_up) {
    Controller c(Config{});
    WorldState w = partyWorld();
    c.setFriends({kMate2});  // 20 cells away, Heal reaches 9
    c.tick(pad(kPadY), w, view(), 1000);
    c.tick(pad(), w, view(), 1100);
    TickOutput o = c.tick(pad(kPadA), w, view(), 1200);
    CHECK(c.approaching());
    CHECK(!o.notes.empty() && o.notes.back() == "approaching for skill 28");
}

TEST(ctl_support_choice_ends_by_start_and_by_time) {
    Controller c(Config{});
    WorldState w = partyWorld();
    c.setFriends({kMate});
    c.tick(pad(kPadY), w, view(), 1000);
    c.tick(pad(), w, view(), 1100);
    c.tick(pad(kPadStart), w, view(), 1200);  // Start: cancelled (the runtime opens no menu meanwhile)
    CHECK_EQ(c.choosing(), 0u);
    c.tick(pad(), w, view(), 1300);
    c.tick(pad(kPadY), w, view(), 2000);
    c.tick(pad(), w, view(), 2100);
    CHECK_EQ(c.choosing(), kMate);
    TickOutput o = c.tick(pad(), w, view(), 10100);  // 8 s: given up
    CHECK_EQ(c.choosing(), 0u);
    CHECK(!o.notes.empty() && o.notes.back() == "F2: the choice timed out");
}

TEST(ctl_support_choice_ends_when_a_window_takes_the_pad) {
    Controller c(Config{});
    WorldState w = partyWorld();
    c.setFriends({kMate});
    c.tick(pad(kPadY), w, view(), 1000);
    CHECK_EQ(c.choosing(), kMate);
    ViewInfo menu = view();
    menu.paused = true;  // the menu or «Довідник» has the pad
    TickOutput o = c.tick(pad(), w, menu, 1100);
    CHECK_EQ(c.choosing(), 0u);
    CHECK(!o.notes.empty() && o.notes.front() == "F2: the choice cancelled");
}

TEST(ctl_support_skill_alone_casts_at_once) {
    Controller c(Config{});
    WorldState w = partyWorld();
    c.setFriends({kAway});  // nobody of the party in sight: on the character at once, as before
    TickOutput o = c.tick(pad(kPadY), w, view(), 1000);
    CHECK(o.packets.size() == 1 && rd32(pkt(o, 0).data(), 21) == kSelf);
    CHECK_EQ(c.choosing(), 0u);
}

TEST(ctl_triggers_have_hysteresis) {
    Controller c(Config{});
    WorldState w = slotWorld();
    PadState p = pad();
    p.rt = 0.6f;  // past half-way: F5
    CHECK_EQ(c.tick(p, w, view(), 1000).packets.size(), 1u);
    p.rt = 0.45f;  // eased off a little: still held, no new press
    CHECK(c.tick(p, w, view(), 1100).packets.empty());
    p.rt = 0.2f;  // released
    c.tick(p, w, view(), 1200);
    p.rt = 0.6f;  // pressed again
    CHECK_EQ(c.tick(p, w, view(), 1300).packets.size(), 1u);
}

TEST(ctl_held_slot_repeats) {
    Controller c(Config{});  // every 250 ms
    WorldState w = slotWorld();
    CHECK_EQ(c.tick(pad(kPadY), w, view(), 1000).packets.size(), 1u);  // Heal
    CHECK(c.tick(pad(kPadY), w, view(), 1100).packets.empty());
    CHECK_EQ(c.tick(pad(kPadY), w, view(), 1250).packets.size(), 1u);
    CHECK(c.tick(pad(kPadY), w, view(), 1400).packets.empty());
    CHECK_EQ(c.tick(pad(kPadY), w, view(), 1500).packets.size(), 1u);
    Config once;
    once.repeatMs = 0;
    Controller c2(once);
    CHECK_EQ(c2.tick(pad(kPadY), w, view(), 1000).packets.size(), 1u);
    CHECK(c2.tick(pad(kPadY), w, view(), 2000).packets.empty());
}

TEST(ctl_held_slot_waits_while_approaching) {
    Controller c(Config{});
    WorldState w = slotWorld();
    addMob(w, 110000001, Cell{105, 100});
    TickOutput o = c.tick(pad(kPadX), w, view(), 1000);  // Bash, range 1: walk there first
    CHECK(c.approaching());
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x00a7);
    o = c.tick(pad(kPadX), w, view(), 1300);  // held: no repeat on the way, only the next step
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x00a7);
    w.apply(EvSetPos{kSelf, Cell{104, 100}}, 1400);
    o = c.tick(pad(kPadX), w, view(), 1450);  // arrived: the Bash goes out
    CHECK(!c.approaching());
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0072);
    CHECK(c.tick(pad(kPadX), w, view(), 1460).packets.empty());  // the slot just fired
    o = c.tick(pad(kPadX), w, view(), 1710);
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0072);
}

TEST(ctl_no_walking_up_while_the_stick_walks) {
    Controller c(Config{});
    WorldState w = slotWorld();
    addMob(w, 110000001, Cell{112, 100});  // 12 cells east: beyond Fire Bolt's 9
    PadState p = pad(kPadUp);              // F7: Fire Bolt while walking west, away from it
    p.lx = -1.0f;
    TickOutput o = c.tick(p, w, view(), 1000);
    CHECK(!c.approaching());
    CHECK(o.packets.empty());  // no walk towards it, against the stick
    CHECK(o.rumble == Rumble::Denied);
    CHECK_EQ(c.target(), 0u);
    w.apply(EvSetPos{110000001, Cell{108, 100}}, 1200);  // it came within range
    o = c.tick(p, w, view(), 1250);                     // F7 still held: the repeat casts while we walk on
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0072);
    CHECK(o.mouse.active && o.mouse.lmb);
}

TEST(ctl_no_pickup_walk_while_the_stick_walks) {
    Controller c(Config{});
    WorldState w = world();
    w.apply(EvGroundItem{4242, 909, Cell{104, 100}}, 0);  // 4 cells away
    PadState p = padLT(0);
    p.lx = -1.0f;
    TickOutput o = c.tick(p, w, view(), 1000);
    CHECK(!c.approaching());
    CHECK(o.packets.empty());
    CHECK(o.rumble == Rumble::Denied);
    w.apply(EvGroundItem{4243, 909, Cell{99, 100}}, 1100);  // one right next to us
    o = c.tick(p, w, view(), 1250);                        // LT still held: picked up on the way
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x00f5);
}

TEST(ctl_held_potion_repeats_during_a_walk_up) {
    Controller c(Config{});
    WorldState w = slotWorld();
    addMob(w, 110000001, Cell{105, 100});
    TickOutput o = c.tick(padRT(kPadX), w, view(), 1000);  // F1 Bash walks up; F5 Red Potion
    CHECK(c.approaching());
    CHECK_EQ(o.packets.size(), 2u);
    o = c.tick(padRT(kPadX), w, view(), 1250);  // both held: the potion repeats, the Bash waits for its walk
    CHECK(c.approaching());
    bool potion = false;
    for (const Bytes& b : o.packets) potion = potion || pid(b) == 0x009f;
    CHECK(potion);
}

TEST(ctl_held_repeat_never_takes_over_another_walk_up) {
    Controller c(Config{});
    WorldState w = slotWorld();
    addMob(w, 110000001, Cell{112, 100});          // 12 cells: Bash and Thunderstorm both need a walk up
    c.tick(pad(kPadX), w, view(), 1000);           // F1: Bash walks up
    c.tick(pad(kPadX | kPadRB), w, view(), 1100);  // F4 pressed too: a new press takes over
    TickOutput o = c.tick(pad(kPadX | kPadRB), w, view(), 1250);  // F1's repeat must not take it back
    CHECK(c.approaching());
    bool bash = false;
    for (const std::string& n : o.notes) bash = bash || n.find("skill 5") != std::string::npos;
    CHECK(!bash);
}

TEST(ctl_slot_throttle_is_per_slot) {
    Controller c(Config{});
    WorldState w = slotWorld();
    TickOutput o = c.tick(padRT(kPadY), w, view(), 1000);  // two slots at once: both go out
    CHECK_EQ(o.packets.size(), 2u);
    c.tick(pad(), w, view(), 1050);
    o = c.tick(pad(kPadY), w, view(), 1100);  // the same slot again 100 ms later: too soon
    CHECK(o.packets.empty());
    CHECK(o.rumble == Rumble::None);
    c.tick(pad(), w, view(), 1120);
    CHECK_EQ(c.tick(pad(kPadY), w, view(), 1160).packets.size(), 1u);
}

TEST(ctl_slot_enemy_skill_targets_nearest_then_approaches) {
    Controller c(Config{});
    WorldState w = slotWorld();
    addMob(w, 110000001, Cell{105, 100});  // distance 5
    TickOutput o = c.tick(pad(kPadUp), w, view(), 1000);  // F7: Fire Bolt, range 9
    CHECK_EQ(c.target(), 110000001u);
    CHECK_EQ(o.mark, 110000001u);
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0072);
    CHECK_EQ(rd16(pkt(o, 0).data(), 10), 19);
    CHECK_EQ(rd32(pkt(o, 0).data(), 21), 110000001u);
    c.tick(pad(), w, view(), 1100);
    o = c.tick(pad(kPadX), w, view(), 1300);  // F1: Bash, range 1: walk first
    CHECK(c.approaching());
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x00a7);
    CHECK(decodePos3(pkt(o, 0).data(), 5) == (Cell{105, 100}));
    w.apply(EvSetPos{kSelf, Cell{104, 100}}, 1400);  // arrived next to it
    o = c.tick(pad(), w, view(), 1450);
    CHECK(!c.approaching());
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0072);
    CHECK_EQ(rd16(pkt(o, 0).data(), 10), 5);
}

TEST(ctl_empty_slot_hint_when_server_sent_no_bar) {
    Controller c(Config{});
    WorldState w = world();  // the server never sent 0x02b9 (HOTKEY_SAVING off)
    TickOutput o = c.tick(pad(kPadX), w, view(), 1000);
    CHECK(o.rumble == Rumble::Denied);
    CHECK(!o.notes.empty() && o.notes.back().find("hotkey bar") != std::string::npos);
    Controller c2(Config{});
    WorldState known = slotWorld();  // the bar arrived; F8 is just empty
    o = c2.tick(pad(kPadRight), known, view(), 1000);
    CHECK(!o.notes.empty() && o.notes.back().find("hotkey bar") == std::string::npos);
}

TEST(ctl_slot_denial_says_why_for_the_client_bar) {
    Controller c(Config{});
    WorldState w = world();
    std::array<ClientBarSlot, kClientBarSlots> bar{};
    bar[0].kind = ClientBarSlot::Kind::Skill;
    bar[0].name = "AC_DOUBLE";  // on the client's bar, but not in the skill list the server sent
    w.setClientBar(bar);
    TickOutput o = c.tick(pad(kPadX), w, view(), 1000);
    CHECK(o.rumble == Rumble::Denied);
    CHECK(!o.notes.empty() && o.notes.back().find("AC_DOUBLE") != std::string::npos);
}

TEST(ctl_held_empty_slot_stays_quiet) {
    Controller c(Config{});
    WorldState w = slotWorld();
    TickOutput o = c.tick(pad(kPadRight), w, view(), 1000);  // F8 is empty
    CHECK(o.rumble == Rumble::Denied);
    o = c.tick(pad(kPadRight), w, view(), 1300);  // held: no buzzing every 250 ms
    CHECK(o.rumble == Rumble::None);
    CHECK(o.notes.empty());
}

// --- LT: pickup, D-pad left: sit ---

TEST(ctl_pickup_nearest_with_approach) {
    Controller c(Config{});
    WorldState w = world();
    w.apply(EvGroundItem{4242, 909, Cell{104, 100}}, 0);  // distance 4
    w.apply(EvGroundItem{4243, 909, Cell{100, 101}}, 0);  // distance 1
    w.apply(EvGroundItem{4244, 909, Cell{120, 100}}, 0);  // distance 20: out of reach
    TickOutput o = c.tick(padLT(0), w, view(), 1000);
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x00f5);
    CHECK_EQ(rd32(pkt(o, 0).data(), 4), 4243u);
    c.tick(pad(), w, view(), 1010);
    w.apply(EvGroundItemGone{4243}, 1020);
    o = c.tick(padLT(0), w, view(), 1100);
    CHECK(c.approaching());
    CHECK_EQ(pid(pkt(o, 0)), 0x00a7);
    o = c.tick(pad(), w, view(), 1200);  // 100 ms: no new step yet
    CHECK(o.packets.empty());
    o = c.tick(pad(), w, view(), 1350);  // 250 ms: step again
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x00a7);
    w.apply(EvSetPos{kSelf, Cell{103, 100}}, 1400);
    o = c.tick(pad(), w, view(), 1450);
    CHECK_EQ(pid(pkt(o, 0)), 0x00f5);
    CHECK_EQ(rd32(pkt(o, 0).data(), 4), 4242u);
    CHECK(!c.approaching());
}

TEST(ctl_held_pickup_takes_items_one_by_one) {
    Controller c(Config{});
    WorldState w = world();
    w.apply(EvGroundItem{4242, 909, Cell{101, 100}}, 0);
    w.apply(EvGroundItem{4243, 909, Cell{100, 101}}, 0);
    TickOutput o = c.tick(padLT(0), w, view(), 1000);
    CHECK_EQ(rd32(pkt(o, 0).data(), 4), 4242u);  // equally close: the lower id
    w.apply(EvGroundItemGone{4242}, 1100);
    o = c.tick(padLT(0), w, view(), 1250);
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(rd32(pkt(o, 0).data(), 4), 4243u);
}

TEST(ctl_held_pickup_moves_past_an_item_it_cannot_take) {
    Controller c(Config{});
    WorldState w = world();
    w.apply(EvGroundItem{4242, 909, Cell{101, 100}}, 0);  // someone else's loot: the server keeps refusing it
    w.apply(EvGroundItem{4243, 909, Cell{102, 100}}, 0);
    const PadState lt = padLT(0);
    CHECK_EQ(rd32(pkt(c.tick(lt, w, view(), 1000), 0).data(), 4), 4242u);
    CHECK_EQ(rd32(pkt(c.tick(lt, w, view(), 1250), 0).data(), 4), 4242u);  // tried again
    TickOutput o = c.tick(lt, w, view(), 2000);  // a second on it and it is still there: the next one
    CHECK(c.approaching());
    CHECK_EQ(pid(pkt(o, 0)), 0x00a7);
    c.tick(pad(), w, view(), 2100);
    o = c.tick(lt, w, view(), 2200);  // a new press gives it another try
    CHECK_EQ(pid(pkt(o, 0)), 0x00f5);
    CHECK_EQ(rd32(pkt(o, 0).data(), 4), 4242u);
}

TEST(ctl_held_pickup_waits_after_a_walk_up) {
    Controller c(Config{});
    WorldState w = world();
    w.apply(EvGroundItem{4242, 909, Cell{104, 100}}, 0);
    c.tick(padLT(0), w, view(), 1000);
    CHECK(c.approaching());
    w.apply(EvSetPos{kSelf, Cell{103, 100}}, 1400);
    TickOutput o = c.tick(padLT(0), w, view(), 1450);  // arrived: picked up
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x00f5);
    CHECK(c.tick(padLT(0), w, view(), 1460).packets.empty());  // not a second request right behind it
}

TEST(ctl_pickup_nothing_denied_and_approach_timeout) {
    Controller c(Config{});
    WorldState w = world();
    TickOutput o = c.tick(padLT(0), w, view(), 1000);
    CHECK(o.rumble == Rumble::Denied);
    c.tick(pad(), w, view(), 1010);
    w.apply(EvGroundItem{4242, 909, Cell{108, 100}}, 1020);
    c.tick(padLT(0), w, view(), 1100);
    CHECK(c.approaching());
    c.tick(pad(), w, view(), 6100);  // 5 s and still not there
    CHECK(!c.approaching());
}

TEST(ctl_stick_cancels_approach) {
    Controller c(Config{});
    WorldState w = world();
    w.apply(EvGroundItem{4242, 909, Cell{108, 100}}, 0);
    c.tick(padLT(0), w, view(), 1000);
    CHECK(c.approaching());
    PadState p = pad();
    p.lx = 1.0f;
    c.tick(p, w, view(), 1100);
    CHECK(!c.approaching());
}

TEST(ctl_sit_stand_on_dpad_left) {
    Controller c(Config{});
    WorldState w = world();
    TickOutput o = c.tick(pad(kPadLeft), w, view(), 1000);
    CHECK_EQ(pid(pkt(o, 0)), 0x0190);
    CHECK_EQ(pkt(o, 0)[18], 2);  // sit
    c.tick(pad(), w, view(), 1010);
    w.apply(EvAction{kSelf, 0, 2}, 1050);  // the server confirms: sitting
    o = c.tick(pad(kPadLeft), w, view(), 1100);
    CHECK_EQ(pkt(o, 0)[18], 3);  // stand
    CHECK(c.tick(pad(kPadLeft), w, view(), 1500).packets.empty());  // held: no repeat
}

// --- R3 and the right stick ---

TEST(ctl_l3_clears_target_and_stops_attack) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{102, 100});
    c.tick(pad(kPadA), w, view(), 1000);
    c.tick(pad(), w, view(), 1100);
    TickOutput o = c.tick(pad(kPadL3), w, view(), 1200);
    CHECK_EQ(c.target(), 0u);
    CHECK(!c.attacking());
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0118);
    CHECK(o.rumble == Rumble::TargetCleared);
    c.tick(pad(), w, view(), 1300);
    o = c.tick(pad(kPadL3), w, view(), 1400);  // nothing to clear
    CHECK(o.packets.empty());
    CHECK(o.rumble == Rumble::None);
}

TEST(ctl_l3_also_stops_a_walk_up) {
    Controller c(Config{});
    WorldState w = slotWorld();
    addMob(w, 110000001, Cell{112, 100});  // out of Fire Bolt's range
    c.tick(pad(kPadUp), w, view(), 1000);  // walking up to cast
    CHECK(c.approaching());
    w.apply(EvSelfWalk{Cell{100, 100}, Cell{111, 100}}, 1050);  // the server walks us
    c.tick(pad(), w, view(), 1100);
    TickOutput o = c.tick(pad(kPadL3), w, view(), 1300);
    CHECK(!c.approaching());
    CHECK_EQ(o.packets.size(), 2u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0118);
    CHECK_EQ(pid(pkt(o, 1)), 0x00a7);  // and stop: two cells on from (101,100), not behind the server
    CHECK(decodePos3(pkt(o, 1).data(), 5) == (Cell{103, 100}));
}

TEST(ctl_right_stick_picks_in_direction) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{105, 100});  // east
    addMob(w, 110000002, Cell{100, 106});  // north
    TickOutput o = c.tick(padR(1, 0), w, view(), 1000);
    CHECK_EQ(c.target(), 110000001u);
    CHECK(o.rumble == Rumble::TargetSwitch);
    CHECK_EQ(o.mark, 110000001u);
    CHECK(o.packets.empty());  // choosing is not attacking
    CHECK(!c.attacking());
    o = c.tick(padR(0, 1), w, view(), 1100);  // stick up: north
    CHECK_EQ(c.target(), 110000002u);
    CHECK_EQ(o.mark, 110000002u);
    c.tick(pad(), w, view(), 1200);  // released: the choice stays
    CHECK_EQ(c.target(), 110000002u);
    o = c.tick(padR(0, -1), w, view(), 1300);  // nothing to the south
    CHECK_EQ(c.target(), 110000002u);
    CHECK(o.rumble == Rumble::None);
}

// 3.0.11-beta.87 (the concept's stage 3): a light tilt of the right stick shows where a push would take the aim (a
// dashed ring under it) and changes nothing; a push takes it
TEST(ctl_right_stick_light_tilt_previews_the_next_target) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{105, 100});  // east
    addMob(w, 110000002, Cell{100, 106});  // north
    c.tick(padR(1, 0), w, view(), 1000);
    c.tick(pad(), w, view(), 1100);
    CHECK_EQ(c.target(), 110000001u);
    CHECK_EQ(c.preview(), 0u);
    TickOutput o = c.tick(padR(0, 0.45f), w, view(), 1200);  // a light tilt north
    CHECK_EQ(c.preview(), 110000002u);
    CHECK_EQ(c.target(), 110000001u);
    CHECK(o.rumble == Rumble::None);
    c.tick(padR(1, 0.1f), w, view(), 1300);  // a push east, at the target: no preview of it
    CHECK_EQ(c.preview(), 0u);
    c.tick(pad(), w, view(), 1400);
    c.tick(padR(0, 0.45f), w, view(), 1500);
    c.tick(padR(0, 0.9f), w, view(), 1600);  // pushed on: taken
    CHECK_EQ(c.target(), 110000002u);
    CHECK_EQ(c.preview(), 0u);
    c.tick(pad(), w, view(), 1700);
    CHECK_EQ(c.preview(), 0u);  // let go: none
}

TEST(ctl_right_stick_thresholds_and_switch_margin) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{105, 100});  // east, aim score 10
    addMob(w, 110000002, Cell{104, 101});  // 14 degrees north of east, 4.1 cells
    c.tick(padR(0.5f, 0), w, view(), 1000);  // below 0.6: not aiming yet
    CHECK_EQ(c.target(), 0u);
    c.tick(padR(1, 0), w, view(), 1100);
    CHECK_EQ(c.target(), 110000001u);
    c.tick(padR(1, 0.15f), w, view(), 1200);  // the other one is better now, but only by 4.8
    CHECK_EQ(c.target(), 110000001u);
    c.tick(padR(1, 0.25f), w, view(), 1300);  // straight at it: better by 15.8
    CHECK_EQ(c.target(), 110000002u);
    c.tick(padR(0.5f, 0), w, view(), 1400);  // 0.5 keeps aiming once started (off below 0.35)
    CHECK_EQ(c.target(), 110000001u);
    c.tick(padR(0.3f, 0), w, view(), 1500);  // let go
    c.tick(padR(0, 0.5f), w, view(), 1600);  // 0.5 does not start aiming again
    CHECK_EQ(c.target(), 110000001u);
}

TEST(ctl_right_stick_redirects_running_attack) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{102, 100});  // east, nearest
    addMob(w, 110000002, Cell{100, 103});  // north
    c.tick(pad(kPadA), w, view(), 1000);
    CHECK_EQ(c.target(), 110000001u);
    c.tick(pad(), w, view(), 1100);
    TickOutput o = c.tick(padR(0, 1), w, view(), 1400);
    CHECK_EQ(c.target(), 110000002u);
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0190);
    CHECK_EQ(rd32(pkt(o, 0).data(), 5), 110000002u);
}

TEST(ctl_new_target_cancels_a_walk_up_to_the_old_one) {
    Controller c(Config{});
    WorldState w = slotWorld();
    addMob(w, 110000001, Cell{112, 100});  // east, the nearest, out of Fire Bolt's range
    addMob(w, 110000002, Cell{100, 114});  // north, farther
    c.tick(pad(kPadUp), w, view(), 1000);  // walking up to cast Fire Bolt at the east one
    CHECK(c.approaching());
    c.tick(pad(), w, view(), 1100);
    c.tick(padR(0, 1), w, view(), 1200);  // the right stick picks the north one
    CHECK_EQ(c.target(), 110000002u);
    CHECK(!c.approaching());  // the Fire Bolt does not go to the old one
}

TEST(ctl_r3_switches_the_right_stick_to_the_camera) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{105, 100});  // east
    TickOutput o = c.tick(pad(kPadR3), w, view(), 1000);
    CHECK(c.cameraMode());
    CHECK(o.rumble == Rumble::CameraOn);
    float turned = 0;
    for (uint32_t t = 1100; t <= 1600; t += 50) turned += c.tick(padR(1, 0), w, view(), t).camera.turnDeg;
    CHECK_EQ(c.target(), 0u);  // the stick turns the camera now, it picks nothing
    CHECK(turned > 40.0f && turned < 80.0f);  // about half a second at full speed
    float back = 0;
    for (uint32_t t = 1650; t <= 2150; t += 50) back += c.tick(padR(-1, 0), w, view(), t).camera.turnDeg;
    CHECK(back < -40.0f);
    c.tick(pad(), w, view(), 2200);
    o = c.tick(pad(kPadR3), w, view(), 2300);
    CHECK(!c.cameraMode());
    CHECK(o.rumble == Rumble::CameraOff);
    c.tick(pad(), w, view(), 2400);
    c.tick(padR(1, 0), w, view(), 2500);
    CHECK_EQ(c.target(), 110000001u);  // targets again
}

TEST(ctl_camera_zoom_steps_and_dead_zone) {
    Controller c(Config{});
    WorldState w = world();
    c.tick(pad(kPadR3), w, view(), 1000);
    c.tick(pad(), w, view(), 1050);
    int in = 0;
    for (uint32_t t = 1100; t <= 2100; t += 50) in += c.tick(padR(0, 1), w, view(), t).camera.zoomSteps;
    CHECK(in >= 4 && in <= 8);  // stick up: closer, a few wheel notches a second
    int out = 0;
    for (uint32_t t = 2150; t <= 3150; t += 50) out += c.tick(padR(0, -1), w, view(), t).camera.zoomSteps;
    CHECK(out <= -4);
    TickOutput o = c.tick(padR(0.15f, 0.1f), w, view(), 3200);  // a resting stick does nothing
    o = c.tick(padR(0.15f, 0.1f), w, view(), 3300);
    CHECK(o.camera.turnDeg == 0.0f && o.camera.zoomSteps == 0);
}

TEST(ctl_camera_toggle_leaves_a_walk_up_alone) {
    Controller c(Config{});
    WorldState w = slotWorld();
    addMob(w, 110000001, Cell{100, 107});  // 7 cells: Bash needs a walk up
    c.tick(pad(kPadX), w, view(), 1000);
    CHECK(c.approaching());
    c.tick(pad(kPadX | kPadR3), w, view(), 1100);  // R3 while X is held: only the camera changes
    CHECK(c.approaching());
    CHECK(c.cameraMode());
}

TEST(ctl_back_to_targets_waits_for_the_stick_to_come_back) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{105, 100});
    c.tick(padR(1, 0, kPadR3), w, view(), 1000);  // clicked the stick while pushing it: camera
    c.tick(padR(1, 0), w, view(), 1100);
    c.tick(padR(1, 0, kPadR3), w, view(), 1200);  // and back
    c.tick(padR(1, 0), w, view(), 1300);
    CHECK_EQ(c.target(), 0u);  // no pick until the stick comes back
    c.tick(pad(), w, view(), 1400);
    c.tick(padR(1, 0), w, view(), 1500);
    CHECK_EQ(c.target(), 110000001u);
}

TEST(ctl_mark_each_monster_at_most_once_a_second) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{105, 100});  // east
    addMob(w, 110000002, Cell{100, 106});  // north
    CHECK_EQ(c.tick(padR(1, 0), w, view(), 1000).mark, 110000001u);
    CHECK_EQ(c.tick(padR(0, 1), w, view(), 1100).mark, 110000002u);
    TickOutput o = c.tick(padR(1, 0), w, view(), 1200);  // back to the first one
    CHECK_EQ(c.target(), 110000001u);
    CHECK_EQ(o.mark, 0u);  // its "!" is still showing
    c.tick(padR(0, 1), w, view(), 2050);
    o = c.tick(padR(1, 0), w, view(), 2100);
    CHECK_EQ(o.mark, 110000001u);
}

// --- camera ---

TEST(ctl_right_stick_follows_camera_turn) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{105, 100});  // east
    addMob(w, 110000002, Cell{100, 106});  // north
    PadState walkRight = pad();
    walkRight.lx = 1.0f;
    c.tick(walkRight, w, view(), 1000);       // the pad holds the walking point right of the centre
    c.onClientWalk(Cell{100, 107}, w, 1000);  // and the client walks north: the camera is turned
    CHECK(std::fabs(c.camera().radians() - kPi / 2) < 1e-6);
    c.tick(pad(), w, view(), 1100);
    c.tick(padR(1, 0), w, view(), 1200);  // right on the screen is north on the map now
    CHECK_EQ(c.target(), 110000002u);
}

TEST(ctl_camera_learns_only_from_a_steady_walking_point) {
    Config cfg;
    cfg.press = PressMode::Center;
    Controller c(cfg);
    WorldState w = world();
    PadState p = pad();
    p.lx = 1.0f;
    c.tick(p, w, view(), 1000);  // the first frame presses in the centre
    c.onClientWalk(Cell{100, 107}, w, 1000);
    CHECK_EQ(c.camera().samples(), 0u);
    c.tick(p, w, view(), 1020);  // on the ring now; the client may still act on the old point for a moment
    c.onClientWalk(Cell{100, 107}, w, 1030);
    CHECK_EQ(c.camera().samples(), 0u);
    p.ly = 0.1f;  // a slight wobble keeps the point steady
    c.tick(p, w, view(), 1100);
    c.onClientWalk(Cell{100, 107}, w, 1170);
    CHECK_EQ(c.camera().samples(), 1u);
    p.lx = 0;
    p.ly = 1.0f;  // turned the stick up
    c.tick(p, w, view(), 1200);
    c.onClientWalk(Cell{93, 100}, w, 1250);
    CHECK_EQ(c.camera().samples(), 1u);
    c.tick(pad(), w, view(), 1400);  // stopped: walks the player clicks with the mouse teach nothing
    c.onClientWalk(Cell{93, 100}, w, 1450);
    CHECK_EQ(c.camera().samples(), 1u);
}

// --- layouts from the menu ---

namespace {

size_t bindIndex(uint16_t button) {
    for (size_t i = 0; i < kBindableButtons.size(); ++i)
        if (kBindableButtons[i] == button) return i;
    return 0;
}

}  // namespace

TEST(ctl_custom_bindings) {
    Controller c(Config{});
    WorldState w = slotWorld();
    addMob(w, 110000001, Cell{102, 100});
    Bindings b = defaultBindings();
    b[bindIndex(kPadX)] = Action{Action::Kind::Attack, 0};
    b[bindIndex(kPadA)] = Action{Action::Kind::Slot, 1};  // F2: Heal
    c.setBindings(b);
    TickOutput o = c.tick(pad(kPadA), w, view(), 1000);
    CHECK_EQ(o.packets.size(), 1u);
    CHECK(pid(pkt(o, 0)) == 0x0072 && rd16(pkt(o, 0).data(), 10) == 28);
    CHECK(!c.attacking());
    c.tick(pad(), w, view(), 1100);
    o = c.tick(pad(kPadX), w, view(), 1200);
    CHECK(c.attacking());
    CHECK_EQ(c.target(), 110000001u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0190);
}

TEST(ctl_unbound_button_does_nothing) {
    Controller c(Config{});
    WorldState w = slotWorld();
    addMob(w, 110000001, Cell{102, 100});
    Bindings b = defaultBindings();
    b[bindIndex(kPadA)] = Action{};
    c.setBindings(b);
    TickOutput o = c.tick(pad(kPadA), w, view(), 1000);
    CHECK(o.packets.empty());
    CHECK(o.rumble == Rumble::None);
    CHECK(!c.attacking());
}

TEST(ctl_held_attack_follows_its_button) {
    Controller c(Config{});
    WorldState w = world();
    addMob(w, 110000001, Cell{102, 100});
    addMob(w, 110000002, Cell{104, 100});
    Bindings b = defaultBindings();
    b[bindIndex(kPadA)] = Action{};
    b[bindIndex(kPadY)] = Action{Action::Kind::Attack, 0};
    c.setBindings(b);
    c.tick(pad(kPadY), w, view(), 1000);
    c.tick(pad(kPadY), w, view(), 1100);
    w.apply(EvVanish{110000001, 1}, 1200);
    c.tick(pad(kPadY), w, view(), 1400);
    CHECK_EQ(c.target(), 110000002u);  // held Y moves on like a held A did
}

TEST(ctl_slot_from_the_second_row) {
    Controller c(Config{});
    WorldState w = slotWorld();
    w.setHotkey(10, Hotkey{1, 28, 5});  // row 2, F2: Heal
    Bindings b = defaultBindings();
    b[bindIndex(kPadX)] = Action{Action::Kind::Slot, 10};
    c.setBindings(b);
    TickOutput o = c.tick(pad(kPadX), w, view(), 1000);
    CHECK_EQ(o.packets.size(), 1u);
    CHECK(rd16(pkt(o, 0).data(), 10) == 28);
}

TEST(ctl_paused_for_the_menu) {
    Controller c(Config{});
    WorldState w = slotWorld();
    addMob(w, 110000001, Cell{102, 100});
    PadState p = pad();
    p.lx = 1.0f;
    c.tick(p, w, view(), 1000);  // walking
    ViewInfo paused = view();
    paused.paused = true;
    TickOutput o = c.tick(p, w, paused, 1100);  // the menu opened: stop
    CHECK(!o.mouse.active);
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x00a7);
    o = c.tick(pad(kPadA | kPadX), w, paused, 1200);  // menu navigation: nothing for the game
    CHECK(o.packets.empty());
    CHECK_EQ(c.target(), 0u);
    o = c.tick(pad(kPadA | kPadX), w, view(), 1300);  // closed with A still held: not a new press
    CHECK(o.packets.empty());
    CHECK_EQ(c.target(), 0u);
    c.tick(pad(), w, view(), 1400);
    o = c.tick(pad(kPadA), w, view(), 1500);  // a real press
    CHECK_EQ(c.target(), 110000001u);
}

TEST(ctl_menu_stops_a_walk_up_and_an_attack) {
    Controller c(Config{});
    WorldState w = slotWorld();
    addMob(w, 110000001, Cell{102, 100});
    addMob(w, 110000002, Cell{112, 100});
    c.tick(pad(kPadA), w, view(), 1000);  // attacking the near one
    c.tick(pad(), w, view(), 1100);
    ViewInfo paused = view();
    paused.paused = true;
    TickOutput o = c.tick(pad(), w, paused, 1200);  // the menu opens
    CHECK(!c.attacking());
    bool stopAttack = false;
    for (const Bytes& b : o.packets) stopAttack = stopAttack || pid(b) == 0x0118;
    CHECK(stopAttack);
    CHECK(c.tick(pad(), w, paused, 1300).packets.empty());  // only once
    Controller c2(Config{});
    WorldState w2 = slotWorld();
    addMob(w2, 110000002, Cell{112, 100});
    c2.tick(pad(kPadUp), w2, view(), 1000);  // Fire Bolt out of range: walking up
    CHECK(c2.approaching());
    w2.apply(EvSelfWalk{Cell{100, 100}, Cell{103, 100}}, 1050);
    o = c2.tick(pad(), w2, paused, 1100);
    CHECK(!c2.approaching());
    bool stop = false;
    for (const Bytes& b : o.packets) stop = stop || pid(b) == 0x00a7;
    CHECK(stop);
}

// --- NPCs (0.7) ---

namespace {

ViewInfo dialogView(NpcDialog d) {
    ViewInfo v = view();
    v.dialog = d;
    return v;
}

WorldState npcWorld() {
    WorldState w = world();
    w.apply(EvActor{110000101, 86, 0, 200, Cell{103, 100}, Cell{103, 100}}, 0);  // a town NPC, 3 cells east
    w.apply(EvActor{110000102, 86, 0, 200, Cell{90, 100}, Cell{90, 100}}, 0);    // another, 10 cells west
    w.apply(EvActor{110000103, 111, 0, 200, Cell{101, 100}, Cell{101, 100}}, 0);  // an invisible one
    addMob(w, 110000001, Cell{100, 101});
    return w;
}

}  // namespace

TEST(ctl_talk_clicks_the_nearest_npc_or_the_one_the_stick_points_at) {
    Controller c(Config{});
    WorldState w = npcWorld();
    TickOutput o = c.tick(pad(kPadDown), w, view(), 1000);
    CHECK_EQ(o.packets.size(), 1u);
    CHECK_EQ(pid(pkt(o, 0)), 0x0090);
    CHECK_EQ(rd32(pkt(o, 0).data(), 2), 110000101u);
    CHECK_EQ(int(pkt(o, 0)[6]), 1);
    c.tick(pad(), w, view(), 1100);
    o = c.tick(padR(-1, 0, kPadDown), w, view(), 1200);  // the right stick points west
    CHECK_EQ(rd32(pkt(o, 0).data(), 2), 110000102u);
    WorldState empty = world();
    c.tick(pad(), empty, view(), 1300);
    o = c.tick(pad(kPadDown), empty, view(), 1400);
    CHECK(o.packets.empty() && o.rumble == Rumble::Denied);
}

TEST(ctl_npc_window_takes_the_buttons) {
    Controller c(Config{});
    WorldState w = npcWorld();
    PadState p = pad(kPadA);
    p.lx = 1.0f;  // the stick pushed too: the character stays
    TickOutput o = c.tick(p, w, dialogView(NpcDialog::Next), 1000);
    CHECK(!o.mouse.active && o.packets.empty() && !c.attacking());
    CHECK(o.dialog.size() == 1 && o.dialog[0] == DialogCommand::Next);
    o = c.tick(pad(kPadB), w, dialogView(NpcDialog::Next), 1100);  // B does not end the NPC's talk
    CHECK(o.dialog.empty());
    o = c.tick(pad(kPadA), w, dialogView(NpcDialog::Close), 1200);
    CHECK(o.dialog.size() == 1 && o.dialog[0] == DialogCommand::Close);
    o = c.tick(pad(kPadA), w, view(), 1300);  // closed, A still held: no attack until pressed again
    CHECK(o.packets.empty() && !c.attacking());
}

TEST(ctl_npc_menu_moves_with_repeat_and_answers) {
    Controller c(Config{});
    WorldState w = npcWorld();
    const ViewInfo menu = dialogView(NpcDialog::Menu);
    TickOutput o = c.tick(pad(kPadDown), w, menu, 1000);
    CHECK(o.dialog.size() == 1 && o.dialog[0] == DialogCommand::MenuDown);
    CHECK(c.tick(pad(kPadDown), w, menu, 1200).dialog.empty());  // held: the first repeat after 400 ms
    CHECK(c.tick(pad(kPadDown), w, menu, 1400).dialog.size() == 1);
    CHECK(c.tick(pad(kPadDown), w, menu, 1520).dialog.size() == 1);
    o = c.tick(pad(kPadUp), w, menu, 1600);
    CHECK(o.dialog.size() == 1 && o.dialog[0] == DialogCommand::MenuUp);
    o = c.tick(pad(kPadA), w, menu, 1700);
    CHECK(o.dialog.size() == 1 && o.dialog[0] == DialogCommand::MenuOk);
    c.tick(pad(), w, menu, 1800);
    o = c.tick(pad(kPadB), w, menu, 1900);
    CHECK(o.dialog.size() == 1 && o.dialog[0] == DialogCommand::MenuCancel);
}

TEST(ctl_npc_number_steps_by_1_10_100) {
    Controller c(Config{});
    WorldState w = npcWorld();
    const ViewInfo num = dialogView(NpcDialog::Number);
    CHECK(!c.tick(pad(kPadDown), w, num, 1000).number);  // not below 0
    c.tick(pad(), w, num, 1050);
    CHECK(c.tick(pad(kPadUp), w, num, 1100).number == std::optional<int>(1));
    CHECK(c.tick(pad(kPadRB), w, num, 1200).number == std::optional<int>(11));
    CHECK(c.tick(pad(kPadRight), w, num, 1300).number == std::optional<int>(111));
    CHECK(c.tick(pad(kPadLB), w, num, 1400).number == std::optional<int>(101));
    TickOutput o = c.tick(pad(kPadA), w, num, 1500);
    CHECK(o.dialog.size() == 1 && o.dialog[0] == DialogCommand::NumberOk);
    c.tick(pad(), w, view(), 1600);  // a new number field starts from the client's 0 again
    CHECK(c.tick(pad(kPadUp), w, num, 1700).number == std::optional<int>(1));
}

TEST(ctl_npc_window_without_a_button_closes_with_b_held) {
    Controller c(Config{});
    WorldState w = npcWorld();
    const ViewInfo waiting = dialogView(NpcDialog::Waiting);
    CHECK(c.tick(pad(kPadB), w, waiting, 1000).dialog.empty());
    CHECK(c.tick(pad(kPadB), w, waiting, 1500).dialog.empty());
    TickOutput o = c.tick(pad(kPadB), w, waiting, 2000);
    CHECK(o.dialog.size() == 1 && o.dialog[0] == DialogCommand::Close);
    CHECK(c.tick(pad(kPadB), w, waiting, 3100).dialog.empty());  // once
}

TEST(ctl_choice_hint_names_the_buttons) {  // 3.0.11-beta.85
    CHECK(choiceHint(kPadY, Lang::Uk) == "↑ ↓: кому  ·  A: застосувати  ·  Y: на себе  ·  B: скасувати");
    useSonyLabels(true);
    CHECK(choiceHint(kPadY, Lang::En) == "↑ ↓: whom  ·  Cross: cast  ·  Triangle: on yourself  ·  Circle: cancel");
    useSonyLabels(false);
    Controller c(Config{});
    WorldState w = partyWorld();
    c.setFriends({kMate});
    CHECK_EQ(c.choosingButton(), 0);
    c.tick(pad(kPadY), w, view(), 1000);
    CHECK_EQ(c.choosingButton(), kPadY);
}

// 3.0.11-beta.92 (the concept's stage 6): the hints as parts - the buttons apart from their words, for the interface's
// own hint bar to draw them as the controller's glyphs
TEST(ctl_hint_parts) {
    std::vector<HintPart> p = npcHintParts(NpcDialog::Menu, Lang::Uk);
    CHECK_EQ(p.size(), size_t(3));
    CHECK(p[0].buttons == std::vector<uint16_t>({kPadUp, kPadDown}) && p[0].words == "вибір");
    CHECK(p[1].buttons == std::vector<uint16_t>({kPadA}) && p[1].words == "OK");
    CHECK(p[2].buttons == std::vector<uint16_t>({kPadB}) && p[2].words == "скасувати");
    p = npcHintParts(NpcDialog::Waiting, Lang::Uk);
    CHECK(p.size() == 2 && p[0].buttons.empty() && p[0].words == "NPC говорить");
    CHECK(p[1].before == "утримати" && p[1].buttons == std::vector<uint16_t>({kPadB}) && p[1].words == "закрити");
    p = npcHintParts(NpcDialog::Number, Lang::En);
    CHECK(p.size() == 4 && p[1].buttons == std::vector<uint16_t>({kPadLB, kPadRB}) && p[1].words == "±10");
    CHECK(p[2].buttons == std::vector<uint16_t>({kPadLeft, kPadRight}));
    p = choiceHintParts(kPadY, Lang::Uk);
    CHECK(p.size() == 4 && p[2].buttons == std::vector<uint16_t>({kPadY}) && p[2].words == "на себе");
    CHECK(npcHintParts(NpcDialog::None, Lang::En).empty());
}

TEST(ctl_npc_hint_names_the_buttons) {
    CHECK(npcHint(NpcDialog::None, Lang::En).empty());
    CHECK(npcHint(NpcDialog::Next, Lang::En) == "A: next");
    CHECK(npcHint(NpcDialog::Menu, Lang::Ru) == "↑ ↓: выбор  ·  A: OK  ·  B: отмена");
    useSonyLabels(true);
    CHECK(npcHint(NpcDialog::Close, Lang::Uk) == "Cross / Circle: закрити");
    CHECK(npcHint(NpcDialog::Number, Lang::En).find("L1 / R1: ±10") != std::string::npos);
    useSonyLabels(false);
}
