#include <string>

#include "core/json.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(json_round_trips_nested) {
    // 2.3: the studio's pipe, session and queue are JSON
    json::Value v = json::Value::object();
    json::Value list = json::Value::array();
    json::Value item = json::Value::object();
    item.set("n", -1.5);
    item.set("on", true);
    item.set("none", json::Value());
    list.push(item);
    list.push(3);
    list.push(1e3);
    v.set("list", list);
    v.set("name", "a b");
    const std::string text = json::dump(v);
    CHECK_EQ(text, std::string("{\"list\":[{\"n\":-1.5,\"on\":true,\"none\":null},3,1000],\"name\":\"a b\"}"));
    json::Value back;
    std::string error;
    CHECK(json::parse(text, back, error));
    CHECK_EQ(json::dump(back), text);
    CHECK(back["list"][0]["n"].asNumber() == -1.5);
    CHECK(back["list"][0]["on"].asBool());
    CHECK(back["list"][0]["none"].type() == json::Value::Null);
    CHECK_EQ(back["list"][1].asInt(), int64_t(3));
    CHECK_EQ(back["list"].items().size(), size_t(3));
    CHECK_EQ(back["name"].asString(), std::string("a b"));
    // a member set again replaces the first
    back.set("name", "c");
    CHECK_EQ(back.members().size(), size_t(2));
    CHECK_EQ(back["name"].asString(), std::string("c"));
    // big integers stay whole (job ids, times)
    json::Value big = json::Value::object();
    big.set("t", int64_t(1760000000123));
    CHECK(json::parse(json::dump(big), back, error));
    CHECK_EQ(back["t"].asInt(), int64_t(1760000000123));
}

TEST(json_keeps_utf8_and_escapes) {
    json::Value v;
    std::string error;
    CHECK(json::parse("\"\\u0416\\n\"", v, error));
    CHECK_EQ(v.asString(), std::string("\xD0\x96\n"));
    CHECK(json::parse("\"\\ud83d\\ude00\"", v, error));
    CHECK_EQ(v.asString(), std::string("\xF0\x9F\x98\x80"));
    // the game's file names are CP949 bytes: they go through as they are
    const std::string cp949 = "data\\texture\\\xC0\xAF\xC0\xFA";
    json::Value back;
    CHECK(json::parse(json::dump(json::Value(cp949)), back, error));
    CHECK_EQ(back.asString(), cp949);
    // and a Cyrillic folder in UTF-8
    const std::string folder = "C:\\Users\\\xD0\x90\xD0\xBD\\AppData";
    CHECK(json::parse(json::dump(json::Value(folder)), back, error));
    CHECK_EQ(back.asString(), folder);
    CHECK_EQ(json::dump(json::Value("a\"\\\t\x01")), std::string("\"a\\\"\\\\\\t\\u0001\""));
    CHECK(!json::parse("\"\\ud83d\"", v, error));  // half a pair
}

TEST(json_refuses_broken) {
    json::Value v;
    std::string error;
    CHECK(!json::parse("{\"a\":}", v, error) && !error.empty());
    CHECK(!json::parse("[1,]", v, error));
    CHECK(!json::parse("{\"a\":1} x", v, error));
    CHECK(!json::parse("", v, error));
    CHECK(!json::parse("\"open", v, error));
    CHECK(!json::parse("tru", v, error));
    CHECK(!json::parse("{\"a\" 1}", v, error));
    CHECK(!json::parse(std::string(65, '[') + std::string(65, ']'), v, error));
    CHECK(json::parse(std::string(64, '[') + std::string(64, ']'), v, error));
    CHECK(json::parse(" { \"a\" : [ 1 , 2 ] } \n", v, error));
}

TEST(json_missing_member_is_null) {
    json::Value v;
    std::string error;
    CHECK(json::parse("{\"a\":1}", v, error));
    CHECK(v["b"].type() == json::Value::Null);
    CHECK(!v.has("b") && v.has("a"));
    CHECK(v["b"]["c"].type() == json::Value::Null);
    CHECK(v[size_t(3)].type() == json::Value::Null);
    CHECK_EQ(v["b"].asString(), std::string());
    CHECK_EQ(v["a"].asInt(7), int64_t(1));
    CHECK_EQ(v["b"].asInt(7), int64_t(7));
    CHECK(v["a"].asString().empty());
}
