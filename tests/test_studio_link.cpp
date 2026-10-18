#include <string>
#include <utility>
#include <vector>

#include "core/studio_link.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(link_lines_split_and_join) {
    // 2.3: the studio and its engine talk by a JSON object a line over a named pipe
    studio::Message m;
    m.id = 7;
    m.op = "enlarge";
    m.body = json::Value::object();
    m.body.set("model", "anime");
    const std::string line = studio::lineOf(m);
    CHECK_EQ(line, std::string("{\"id\":7,\"op\":\"enlarge\",\"body\":{\"model\":\"anime\"}}\n"));
    studio::LineReader r;
    studio::Message got;
    r.feed(line.data(), 10);
    CHECK(!r.next(got));  // half a line
    r.feed(line.data() + 10, line.size() - 10);
    CHECK(r.next(got) && got.id == 7 && got.op == "enlarge" && got.body["model"].asString() == "anime");
    CHECK(!r.next(got));
    const std::string two = "not json\n" + line + studio::lineOf(studio::Message{0, "log", json::Value("hi")});
    r.feed(two.data(), two.size());
    CHECK(r.next(got) && got.id == 7);
    CHECK(r.next(got) && got.op == "log" && got.body.asString() == "hi");
    CHECK_EQ(r.skipped(), size_t(1));
    const std::string crlf = "{\"id\":1,\"op\":\"hello\"}\r\n";
    r.feed(crlf.data(), crlf.size());
    CHECK(r.next(got) && got.op == "hello" && got.body.type() == json::Value::Null);
}

TEST(pick_and_seen_keep_cp949_names) {
    // the game's map and file names are CP949 bytes; the studio reads them back as they were
    studio::Pick p;
    p.key = "256x256\\0123456789abcdef";
    p.kind = "ground";
    p.how = "pack";
    p.map = "\xC7\xC1\xB7\xD0";
    p.picture = "world/256x256/0123456789abcdef.webp";
    p.x = 640;
    p.y = 360;
    studio::Pick back;
    CHECK(studio::parsePick(studio::pickText(p), back));
    CHECK(back.key == p.key && back.kind == p.kind && back.how == p.how && back.map == p.map);
    CHECK(back.picture == p.picture && back.x == 640 && back.y == 360);
    CHECK(!studio::parsePick("x=1\n", back));  // no key
    std::vector<studio::Seen> seen = {{"256x256\\0123456789abcdef", 1, studio::How::Pack, 120},
                                      {"64x64\\00000000000000aa", 2, studio::How::Soft, 3}};
    std::string map;
    std::vector<studio::Seen> got;
    CHECK(studio::parseSeen(studio::seenText("\xC7\xC1\xB7\xD0", seen), map, got));
    CHECK(map == "\xC7\xC1\xB7\xD0" && got.size() == 2);
    CHECK(got[1].key == "64x64\\00000000000000aa" && got[1].how == studio::How::Soft && got[1].count == 3);
    CHECK(got[0].kind == 1 && got[0].how == studio::How::Pack);
    studio::How h;
    CHECK(studio::parseHow("kind-off", h) && h == studio::How::KindOff);
    CHECK(!studio::parseHow("sparkle", h));
    CHECK(std::string(studio::howName(studio::How::Hidden)) == "hidden");
}

TEST(seen_keeps_the_last_twenty) {
    std::vector<std::pair<std::string, int64_t>> files;
    for (int i = 0; i < 25; ++i) files.emplace_back("map" + std::to_string(i) + ".txt", 1000 + i);
    const std::vector<std::string> gone = studio::seenToDelete(files);
    CHECK_EQ(gone.size(), size_t(5));
    CHECK(gone[0] == "map0.txt" && gone[4] == "map4.txt");
    CHECK(studio::seenToDelete({{"a.txt", 1}}).empty());
}

TEST(changed_round_trips) {
    const std::vector<std::string> keys = {"256x256\\0123456789abcdef", "64x64\\00000000000000aa"};
    CHECK(studio::parseChanged(studio::changedText(keys)) == keys);
    CHECK(studio::parseChanged("\r\n\n").empty());
}

TEST(raw_round_trips_and_refuses_lies) {
    const std::vector<uint32_t> argb = {0xFF000000u, 0x80FFFFFFu, 0x00FF00FFu, 0xFF123456u, 1, 2};
    int w = 0, h = 0;
    std::vector<uint32_t> back;
    CHECK(studio::parseRaw(studio::rawOf(3, 2, argb), w, h, back) && w == 3 && h == 2 && back == argb);
    std::string lie = studio::rawOf(3, 2, argb);
    lie[4] = char(100);  // a width the bytes do not hold
    CHECK(!studio::parseRaw(lie, w, h, back));
    CHECK(!studio::parseRaw("URAW", w, h, back));
    CHECK(!studio::parseRaw(studio::rawOf(3, 2, argb).substr(0, 20), w, h, back));
    CHECK(studio::rawOf(3, 2, {1, 2}).empty());  // fewer pixels than the size: none
}
