#include <string>

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
