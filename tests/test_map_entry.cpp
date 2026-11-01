#include <cstdint>
#include <cstring>

#include "core/config.hpp"
#include "core/live_config.hpp"
#include "core/login_exe.hpp"
#include "core/map_entry.hpp"
#include "test.hpp"

using namespace uagfx;
using map_entry::Guard;
using map_entry::Seen;
using map_entry::Step;

namespace {

Seen entering(bool connected = false, bool failed = false) {
    Seen s;
    s.loginMode = true;
    s.state = login_exe::kStateEntering;
    s.connected = connected;
    s.failed = failed;
    return s;
}

Seen loginScreen() {
    Seen s;
    s.loginMode = true;
    s.state = 3;
    return s;
}

}  // namespace

TEST(map_entry_patient_wait_reconnects_a_failed_connect_a_few_times) {
    // 2.6.17: the client connects without waiting and never asks select() for the failure - a lost SYN left it
    // waiting for nothing. Patient: a failed connect is made again, at most kMaxReconnects times an entry.
    Guard g;
    CHECK(g.frame(entering(), 1000, true) == Step::None);
    for (int i = 0; i < map_entry::kMaxReconnects; ++i)
        CHECK(g.frame(entering(false, true), 2000 + uint64_t(i), true) == Step::Reconnect);
    CHECK(g.frame(entering(false, true), 9000, true) == Step::None);  // tries spent: the client's own wait decides
    Guard plain;  // not patient: the game's own way
    CHECK(plain.frame(entering(false, true), 1000, false) == Step::None);
}

TEST(map_entry_pause_after_a_failed_entry_until_the_map_forgets_it) {
    // The server keeps the failed entry's node: 90 s if the client never came (+30 s sweep), 270 s if it came and
    // left (+30 s); a selection before that is lost. Held until then; nothing held after an entry that worked.
    Guard g;
    g.frame(entering(), 10000, true);
    g.frame(loginScreen(), 25000, true);  // back on the login screen: failed, never connected
    CHECK_EQ(g.holdUntil(), uint64_t(10000 + map_entry::kNodeGoneMs));
    CHECK(g.holding(20000, true) && !g.holding(10000 + map_entry::kNodeGoneMs, true));
    CHECK(!g.holding(20000, false));  // the pause off: nothing held
    g.frame(entering(), 200000, true);
    g.frame(entering(true), 201000, true);  // it came to the map
    g.frame(loginScreen(), 260000, true);
    CHECK_EQ(g.holdUntil(), uint64_t(200000 + map_entry::kNodeOfLeftClientGoneMs));
    g.frame(entering(), 600000, true);
    Seen game;  // the login mode gone: on the map
    g.frame(game, 603000, true);
    CHECK_EQ(g.holdUntil(), uint64_t(0));
    CHECK(!g.holding(603001, true));
}

TEST(map_entry_switches_on_by_default) {
    CHECK(parseConfig("").connection.patientEntry && parseConfig("").connection.entryPause);
    CHECK(!parseConfig("[connection]\npatient_entry = 0\n").connection.patientEntry);
    CHECK(!parseConfig("[connection]\nentry_pause = 0\n").connection.entryPause);
}

TEST(login_exe_patient_wait_replaces_the_15_seconds) {
    // state 0xc's "add edx, 15000" - its immediate made a minute
    CHECK_EQ(int(login_exe::kWaitBytes[6]), 0x81);  // add edx, imm32
    CHECK_EQ(int(login_exe::kWaitBytes[8]) | int(login_exe::kWaitBytes[9]) << 8, 15000);
    uint32_t patient = 0;
    std::memcpy(&patient, login_exe::kPatientImm, 4);
    CHECK_EQ(patient, map_entry::kPatientWaitMs);
    CHECK_EQ(login_exe::kWaitImm, login_exe::kWaitAt + 8);
    // the selection's call: E8 rel32 to the client's queue (CConnection::Send)
    CHECK_EQ(int(login_exe::kSelectBytes[7]), 0xE8);
    int32_t rel = 0;
    std::memcpy(&rel, login_exe::kSelectBytes + 8, 4);
    CHECK_EQ(login_exe::kSelectAt + 7 + 5 + uint32_t(rel), login_exe::kQueue);
}

TEST(map_entry_switches_apply_at_once) {
    // the settings app's change reaches a running game without a restart
    const Config running = parseConfig("");
    const LiveUpdate u = liveUpdate(running, parseConfig("[connection]\nentry_pause = 0\n"));
    CHECK(!u.next.connection.entryPause && u.next.connection.patientEntry);
    CHECK(u.applied.size() == 1 && u.applied[0] == "entry_pause 1 -> 0");
    CHECK(appliesNow("connection.patient_entry") && appliesNow("connection.entry_pause"));
}

TEST(map_entry_hold_kept_and_after_a_quick_restart) {
    // 2.6.17, the user's test: closed with Alt-F4 and opened at once, one entry in five failed - the map server still
    // had the closed session. The hold outlives the client (a file) and a start soon after an exit holds the first
    // selection until kAfterExitMs past it; the later of two holds wins.
    Guard g;
    g.holdAtLeastUntil(5000);
    CHECK_EQ(g.holdUntil(), uint64_t(5000));
    g.holdAtLeastUntil(3000);
    CHECK_EQ(g.holdUntil(), uint64_t(5000));
    CHECK(g.holding(4000, true) && !g.holding(5000, true));
    CHECK_EQ(map_entry::kAfterExitMs, uint64_t(30000));
}
