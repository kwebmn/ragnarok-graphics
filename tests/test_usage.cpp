#include <algorithm>
#include <string>
#include <vector>

#include "core/effect_groups.hpp"
#include "core/live_config.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/usage.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

int count(const std::string& text, const std::string& what) {
    int n = 0;
    for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1)) ++n;
    return n;
}

}  // namespace

TEST(usage_a_line_a_stretch_of_one_setup) {
    // 2.2.1: the players' setups with their minutes, closed when the setup changes or 5 minutes go by
    usage::Recorder r;
    usage::Machine m;
    m.version = "2.2.1";
    m.card = "NVIDIA GeForce RTX 5060 Ti";
    m.api = "d3d12";
    m.vendor = 0x10de;
    m.vramMb = 16311;
    m.level = 3;
    r.begin(m, 0xabcdef);
    usage::Setup s;
    s.preset = "strong";
    s.effects = "torch,grass";
    std::string out;
    for (int i = 0; i < 60 * 60; ++i) r.frame(1.0 / 60.0, true, 7.0, s, out);  // a minute outdoors
    CHECK(out.empty());
    s.effects = "torch";  // the grass turned off: the first stretch closes
    r.change("grass 1 -> 0", out);
    r.frame(1.0 / 60.0, false, -1.0, s, out);
    CHECK_EQ(count(out, "\"t\":\"s\""), 1);
    CHECK_EQ(count(out, "\"t\":\"c\""), 1);
    CHECK(out.find("\"on\":\"torch,grass\"") != std::string::npos);
    CHECK(out.find("\"min\":1.00") != std::string::npos && out.find("\"outdoor\":1.00") != std::string::npos);
    CHECK(out.find("\"fps\":60.0") != std::string::npos && out.find("\"gpu\":7.00") != std::string::npos);
    CHECK(out.find("\"sid\":\"0000000000abcdef\"") != std::string::npos);
    CHECK(out.find("\"what\":\"grass 1 -> 0\"") != std::string::npos);
    std::string more;
    for (int i = 0; i < 6 * 60 * 60; ++i) r.frame(1.0 / 60.0, false, -1.0, s, more);  // six minutes on: one closed
    CHECK_EQ(count(more, "\"t\":\"s\""), 1);
    r.frame(5.0, false, -1.0, s, more);  // a stall: nothing counted
    std::string last;
    r.close(last);
    CHECK_EQ(count(last, "\"t\":\"s\""), 1);
    CHECK(last.find("\"gpu\":-1.00") != std::string::npos);  // nothing measured in it
}

TEST(usage_json_strings_are_escaped) {
    CHECK(usage::jsonString("a\"b\\c\n") == "\"a\\\"b\\\\c\\u000a\"");
}

TEST(usage_upload_takes_whole_lines) {
    size_t taken = 0;
    const std::string lines = "{\"t\":\"s\"}\n{\"t\":\"c\"}\n{\"t\":\"s\"";  // the last one not ended yet
    const std::string body = usage::uploadBody("0123abcd", "2.2.1", lines, 1000, taken);
    CHECK_EQ(int(taken), 20);
    CHECK(body == "{\"id\":\"0123abcd\",\"version\":\"2.2.1\",\"lines\":\"{\\\"t\\\":\\\"s\\\"}\\u000a"
                  "{\\\"t\\\":\\\"c\\\"}\\u000a\"}");
    usage::uploadBody("x", "y", lines, 15, taken);  // only the first fits
    CHECK_EQ(int(taken), 10);
    usage::uploadBody("x", "y", lines, 5, taken);
    CHECK_EQ(int(taken), 0);
}

namespace {

// The live update's lines between two ini texts, as the statistics note them.
std::vector<std::string> noted(const std::string& before, const std::string& after, int card) {
    const Config was = parseConfig(before), now = parseConfig(after);
    return usage::changes(was.effects, now.effects, card, liveUpdate(was, now).applied);
}

bool has(const std::vector<std::string>& lines, const std::string& line) {
    return std::find(lines.begin(), lines.end(), line) != lines.end();
}

}  // namespace

TEST(usage_changes_note_a_switch_made_under_a_set_alone) {
    // 2.2.2: a set made the player's own rewrites every switch (water 0 -> 1, grade 0 -> 1 in the file's words) while
    // nothing drawn changes: only the switch the player changed and the set's change are noted
    const std::string text = "[effects]\npreset = medium\nwater = 0\ngrade = 0\n";
    std::vector<settings::IniEdit> edits;
    for (const auto& [k, v] : effect_groups::materialize(parseConfig(text).effects, 2, "grass"))
        edits.push_back({"effects", k, v});
    edits.push_back({"effects", "grass", "0"});
    const std::vector<std::string> lines = noted(text, settings::applyIniEdits(text, edits), 2);
    CHECK_EQ(lines.size(), size_t(2));
    CHECK(has(lines, "preset medium -> custom") && has(lines, "grass 1 -> 0"));
}

TEST(usage_changes_leave_out_what_a_set_switches) {
    // a set chosen switches many effects: that is the set's move (noted as such), not the player's verdict on each
    const std::vector<std::string> sets = noted("[effects]\npreset = medium\n", "[effects]\npreset = strong\n", 3);
    CHECK_EQ(sets.size(), size_t(1));
    CHECK(has(sets, "preset medium -> strong"));
    // the player's own switches, a group and the rest as they are
    const std::vector<std::string> own = noted("[effects]\npreset = custom\ngrass = 1\n[quality]\nsharpen = 50\n",
                                               "[effects]\npreset = custom\ngrass = 0\ngroup_nature = 0\n"
                                               "[quality]\nsharpen = 80\n",
                                               3);
    CHECK(has(own, "grass 1 -> 0") && has(own, "group_nature 1 -> 0") && has(own, "sharpen 50 -> 80"));
    CHECK_EQ(own.size(), size_t(3));
}
