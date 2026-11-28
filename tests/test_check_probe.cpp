// 3.0.24-beta.57 (docs/superpowers/plans/2026-11-28-full-check-probe-tour.md, task 1): the full check's setting
// probe - what it leaves alone, the value it tries, the frame's change, the card's passes, the log's errors, and its
// steps frame by frame (the window minimised, a shader still compiling, a value the game never takes)
#include <string>
#include <vector>

#include "core/check_probe.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::checkprobe;

TEST(check_probe_skips_the_line_and_the_server) {
    CHECK(!skipReason("mcp.enabled").empty());
    CHECK(!skipReason("mcp.port").empty());
    CHECK(!skipReason("connection.patient_entry").empty());
    CHECK(!skipReason("renderer.api").empty());
    CHECK(!skipReason("launcher.direct_start").empty());
    CHECK(!skipReason("report.disconnects").empty());
    CHECK(!skipReason("stats.send").empty());
    CHECK(!skipReason("debug.log").empty());
    CHECK(skipReason("effects.grass").empty());
    CHECK(skipReason("interface.theme").empty());
    CHECK(skipReason("renderer.cross_present").empty());
}

TEST(check_probe_test_value_flips_each_kind) {
    const settings::IniSetting* check = settings::findSetting("frames.fps_counter");
    CHECK(check != nullptr);
    CHECK_EQ(testValue(*check, "0"), std::string("1"));
    CHECK_EQ(testValue(*check, "1"), std::string("0"));
    const settings::IniSetting* choice = settings::findSetting("effects.clock");
    CHECK(choice != nullptr && choice->tokens.size() >= 3);
    CHECK_EQ(testValue(*choice, choice->tokens.at(0)), std::string(choice->tokens.at(1)));
    CHECK_EQ(testValue(*choice, choice->tokens.back()), std::string(choice->tokens.at(0)));
    CHECK_EQ(testValue(*choice, "no such word"), std::string(choice->tokens.at(0)));
    settings::IniSetting slider = *settings::findSetting("quality.sharpen");  // 0-100, step 5
    CHECK_EQ(testValue(slider, "50"), std::string("75"));
    CHECK_EQ(testValue(slider, "100"), std::string("75"));
    CHECK_EQ(testValue(slider, "90"), std::string("65"));
    CHECK_EQ(testValue(slider, "0"), std::string("25"));
    slider.min = 0, slider.max = 10, slider.step = 5;  // a quarter under a step: a step
    CHECK_EQ(testValue(slider, "0"), std::string("5"));
    settings::IniSetting number = slider;
    number.control = settings::Control::Number;
    number.min = -100000, number.max = 100000, number.step = 1;
    CHECK_EQ(testValue(number, "120"), std::string("121"));
    CHECK_EQ(testValue(number, "100000"), std::string("99999"));
    settings::IniSetting line = slider;
    line.control = settings::Control::Line;
    CHECK_EQ(testValue(line, "words"), std::string());
}

TEST(check_probe_frame_diff) {
    std::vector<uint32_t> a(100, 0xFF102030u), b = a;
    FrameDiff d = diff(a, 10, 10, b, 10, 10);
    CHECK(!d.sizeChanged);
    CHECK(d.changed == 0.0 && d.mean == 0.0);
    b[42] = 0xFF102030u + (40u << 8);  // green up 40 in one pixel of 100
    d = diff(a, 10, 10, b, 10, 10);
    CHECK(d.changed > 0.99 && d.changed < 1.01);
    CHECK(d.mean > 0.13 && d.mean < 0.14);  // 40 / (3 * 100)
    b[43] = 0xFF102030u + 5u;  // under the threshold: no pixel counted, the mean grows
    d = diff(a, 10, 10, b, 10, 10);
    CHECK(d.changed > 0.99 && d.changed < 1.01);
    const std::vector<uint32_t> big(256, 0xFF000000u);
    d = diff(a, 10, 10, big, 16, 16);
    CHECK(d.sizeChanged);
    CHECK(diff({}, 0, 0, {}, 0, 0).sizeChanged);
}

TEST(check_probe_passes_delta) {
    const std::map<std::string, double> before = {{"grass", 0.40}, {"water", 1.00}, {"fog", 0.30}};
    const std::map<std::string, double> after = {{"grass", 0.45}, {"water", 1.30}, {"rain", 0.80}};
    const json::Value d = passesDelta(before, after);
    CHECK(d["added"]["rain"].asNumber() > 0.79);
    CHECK_EQ(d["gone"][size_t(0)].asString(), std::string("fog"));
    CHECK(d["more"]["water"].asNumber() > 0.29 && d["more"]["water"].asNumber() < 0.31);
    CHECK(!d["more"].has("grass"));  // +0.05: under 0.15
    CHECK(passesDelta(before, before).members().empty());  // nothing to tell: no members at all
}

TEST(check_probe_log_errors) {
    const std::string log = "12:00:01.000 INFO  hello\n"
                            "12:00:01.100 ERROR shader water_ps failed\n"
                            "12:00:01.200 INFO  an error inside an info line\n"
                            "12:00:01.300 DEBUG x\n"
                            "12:00:01.400 ERROR again";
    CHECK_EQ(errorLines(log), 2);
    CHECK_EQ(errorLines(""), 0);
    CHECK_EQ(errorLines("ERROR at the start but no time\n"), 0);
}

namespace {

Job oneJob(const std::string& key = "effects.grass", const std::string& from = "0", const std::string& to = "1") {
    Job j;
    j.items.push_back(Item{key, from, to, ""});
    return j;
}

Seen seenAt(uint64_t ms, const std::string& value, bool shot = false, bool loading = false, uint32_t serial = 1) {
    Seen s;
    s.ms = ms, s.value = value, s.shotReady = shot, s.loading = loading, s.gpuSerial = serial;
    return s;
}

}  // namespace

TEST(check_probe_steps_a_setting) {
    Job j = oneJob();
    const Timing t;
    CHECK(step(j, seenAt(1000, "0"), t) == Do::ShotBefore);
    CHECK(step(j, seenAt(1016, "0"), t) == Do::Nothing);  // the frame not read yet
    CHECK(step(j, seenAt(1032, "0", true), t) == Do::Write);
    CHECK(step(j, seenAt(1048, "0"), t) == Do::Nothing);  // the file read the next frame
    CHECK(step(j, seenAt(1064, "1"), t) == Do::Nothing);  // taken: settling
    CHECK(j.row.reached);
    CHECK(step(j, seenAt(1500, "1", false, false, 1), t) == Do::Nothing);  // no new measurement of the card yet
    CHECK(step(j, seenAt(2400, "1", false, false, 2), t) == Do::ShotAfter);
    CHECK(step(j, seenAt(2416, "1", true, false, 2), t) == Do::WriteBack);
    CHECK(step(j, seenAt(2432, "1"), t) == Do::Nothing);
    CHECK(step(j, seenAt(2448, "0"), t) == Do::Record);
    CHECK(j.row.restored);
    CHECK(j.row.error.empty());
    CHECK_EQ(j.row.key, std::string("effects.grass"));
    CHECK(step(j, seenAt(2464, "0"), t) == Do::Finish);
    CHECK(step(j, seenAt(2480, "0"), t) == Do::Nothing);
    const json::Value r = rowJson(j.row);
    CHECK_EQ(r["check"].asString(), std::string("setting"));
    CHECK(r["reached"].asBool() && r["restored"].asBool());
}

TEST(check_probe_waits_out_loading) {
    Job j = oneJob();
    const Timing t;
    step(j, seenAt(1000, "0"), t);
    step(j, seenAt(1010, "0", true), t);   // Write
    step(j, seenAt(1020, "1"), t);         // taken
    uint64_t ms = 1030;
    for (; ms < 21030; ms += 500) CHECK(step(j, seenAt(ms, "1", false, true, 5), t) == Do::Nothing);
    // loading over: it settles again, a new measurement, then the frame
    CHECK(step(j, seenAt(ms, "1", false, false, 5), t) == Do::Nothing);
    CHECK(step(j, seenAt(ms + 1300, "1", false, false, 6), t) == Do::ShotAfter);
    CHECK(j.row.loadMs >= 19500 && j.row.loadMs <= 20500);
}

TEST(check_probe_minimised_pauses) {
    Job j = oneJob();
    const Timing t;
    CHECK(step(j, seenAt(1000, "0"), t) == Do::ShotBefore);
    Seen m = seenAt(1100, "0");
    m.minimised = true;
    for (int i = 0; i < 100; ++i, m.ms += 1000) CHECK(step(j, m, t) == Do::Nothing);  // 100 s away: no row
    CHECK(step(j, seenAt(m.ms, "0", true), t) == Do::Write);  // back: the frame came, on it goes
    CHECK(j.row.error.empty());
}

TEST(check_probe_no_frame_while_shown) {
    Job j = oneJob();
    const Timing t;
    step(j, seenAt(1000, "0"), t);
    CHECK(step(j, seenAt(2000, "0"), t) == Do::Nothing);
    CHECK(step(j, seenAt(4100, "0"), t) == Do::Record);  // 3 s and no frame: told, nothing written
    CHECK_EQ(j.row.error, std::string("no frame"));
    CHECK(!j.row.reached);
    CHECK(step(j, seenAt(4116, "0"), t) == Do::Finish);
}

TEST(check_probe_unreached_value) {
    Job j = oneJob();
    const Timing t;
    step(j, seenAt(1000, "0"), t);
    CHECK(step(j, seenAt(1010, "0", true), t) == Do::Write);
    CHECK(step(j, seenAt(2000, "0"), t) == Do::Nothing);
    CHECK(step(j, seenAt(4100, "0"), t) == Do::WriteBack);  // never taken: written back all the same
    CHECK(!j.row.reached);
    CHECK_EQ(j.row.error, std::string("not taken"));
    CHECK(step(j, seenAt(4116, "0"), t) == Do::Record);
    CHECK(j.row.restored);
}

TEST(check_probe_skipped_item_is_recorded) {
    Job j;
    j.items.push_back(Item{"mcp.enabled", "1", "0", "the MCP's own line"});
    j.items.push_back(Item{"roext.title", "x", "", ""});  // a line of text: no value to try
    const Timing t;
    CHECK(step(j, seenAt(1000, "1"), t) == Do::Record);
    CHECK_EQ(j.row.skip, std::string("the MCP's own line"));
    CHECK(step(j, seenAt(1016, "x"), t) == Do::Record);
    CHECK(!j.row.skip.empty());
    CHECK(step(j, seenAt(1032, "x"), t) == Do::Finish);
    CHECK_EQ(status(j)["at"].asInt(), int64_t(2));
}

TEST(check_probe_restart_plan) {
    const std::vector<Item> items = {{"ai.ground", "1", "0", ""}, {"display.hdr", "0", "1", ""}};
    const json::Value plan = restartPlan(items);
    CHECK_EQ(plan["items"][size_t(1)]["key"].asString(), std::string("display.hdr"));
    const auto now = [](const std::string& k) { return k == "ai.ground" ? std::string("0") : std::string("0"); };
    std::vector<Row> rows = verifyPlan(plan, false, now);
    CHECK_EQ(rows.size(), size_t(2));
    CHECK(rows[0].reached);
    CHECK(!rows[1].reached);  // display.hdr stayed 0: the restart did not take it
    rows = verifyPlan(plan, true, now);
    CHECK(rows[0].restored == false);  // ai.ground still 0, its value was 1
    CHECK(rows[1].restored);
    CHECK_EQ(rowJson(rows[0])["check"].asString(), std::string("restart"));
}
