// 3.0.24-beta.34 (docs/superpowers/plans/2026-11-27-mcp-full-test.md, task 5): the MCP's game tool - the character's
// numbers, the things (bag, cart, storage), the chat's last lines: what an action did, seen from outside
#include <string>
#include <vector>

#include "core/char_block.hpp"
#include "core/chat_panel.hpp"
#include "core/item_state.hpp"
#include "core/mcp.hpp"
#include "core/config.hpp"
#include "core/mcp_game.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(mcp_game_character_items_chat) {
    charblock::Data d;
    d.name = "TestHD", d.job = "Archer", d.baseLevel = 47, d.jobLevel = 33;
    d.hp = 853, d.hpMost = 853, d.sp = 106, d.spMost = 106;
    d.weightNow = 336, d.weightMost = 2630, d.weight = 12, d.zeny = 404099, d.statusPoints = 4;
    const json::Value c = mcpgame::character(d);
    CHECK_EQ(c["name"].asString(), std::string("TestHD"));
    CHECK(c["hp"][size_t(0)].asInt() == 853 && c["hp"][size_t(1)].asInt() == 853);
    CHECK(c["weight"]["now"].asInt() == 336 && c["weight"]["most"].asInt() == 2630);
    CHECK_EQ(c["weight"]["percent"].asInt(), int64_t(12));
    CHECK_EQ(c["zeny"].asInt(), int64_t(404099));
    itemstate::Item a;
    a.index = 2, a.id = 501, a.amount = 30;
    itemstate::Item b;
    b.index = 3, b.id = 1201, b.amount = 1, b.refine = 7, b.places = 2;
    const json::Value it = mcpgame::items({a, b}, {}, {a}, [](int id) {
        return id == 501 ? std::string("Red Potion") : id == 1201 ? std::string("Knife") : std::string();
    });
    CHECK_EQ(it["bag"].items().size(), size_t(2));
    CHECK_EQ(it["bag"][size_t(0)]["name"].asString(), std::string("Red Potion"));
    CHECK(it["bag"][size_t(1)]["refine"].asInt() == 7 && it["bag"][size_t(1)]["worn"].asInt() == 2);
    CHECK(!it["bag"][size_t(0)].has("worn") && !it["bag"][size_t(0)].has("refine"));
    CHECK(it["cart"].items().size() == 0 && it["storage"].items().size() == 1);
    chatpanel::Line l;
    l.kind = chatpanel::Kind::System;
    l.spans = {chatpanel::Span{"Big Mike is not in the game now", chatpanel::kSystem, false, false}};
    chatpanel::Line w;
    w.kind = chatpanel::Kind::Whisper, w.who = "Big Mike", w.repeat = 2;
    w.spans = {chatpanel::Span{"[from Big Mike] ", chatpanel::kWhisper, true, false},
               chatpanel::Span{"hi", chatpanel::kWhisper, false, false}};
    const json::Value ch = mcpgame::chat({l, w});
    CHECK_EQ(ch.items().size(), size_t(2));
    CHECK_EQ(ch[size_t(0)]["kind"].asString(), std::string("system"));
    CHECK_EQ(ch[size_t(1)]["text"].asString(), std::string("[from Big Mike] hi"));
    CHECK(ch[size_t(1)]["who"].asString() == "Big Mike" && ch[size_t(1)]["repeat"].asInt() == 2);
    // its arguments: what character|items|chat, lines 1-100
    json::Value a1 = json::Value::object();
    a1.set("what", "items");
    CHECK(mcp::checkArgs("game", a1).empty());
    a1.set("what", "secrets");
    CHECK(!mcp::checkArgs("game", a1).empty());
    a1.set("what", "chat");
    a1.set("lines", 101);
    CHECK(!mcp::checkArgs("game", a1).empty());
}

// 3.0.24-beta.34 (task 6): every setting the driver knows - its value now and what it takes, for the test's matrix
TEST(mcp_settings_list) {
    const Config c;
    const json::Value all = mcpgame::settings(c, "interface.theme", settings::Lang::En);
    bool theme = false;
    for (const json::Value& s : all.items())
        if (s["key"].asString() == "interface.theme") {
            theme = s["value"].asString() == "dark" && s["kind"].asString() == "choice";
            bool light = false;
            for (const json::Value& v : s["values"].items()) light = light || v.asString() == "light";
            theme = theme && light && !s["label"].asString().empty();
        }
    CHECK(theme);
    for (const json::Value& s : all.items()) CHECK(s["key"].asString().rfind("interface.theme", 0) == 0);
    CHECK(mcpgame::settings(c, "no.such", settings::Lang::En).items().empty());
    const json::Value every = mcpgame::settings(c, "", settings::Lang::Uk);
    CHECK(every.items().size() == settings::iniSettings().size());
    bool slider = false;
    for (const json::Value& s : every.items())
        if (s["kind"].asString() == "slider") slider = s.has("min") && s.has("max");
    CHECK(slider);
    json::Value a = json::Value::object();
    a.set("prefix", "effects.");
    CHECK(mcp::checkArgs("settings", a).empty());
    a.set("prefix", "../ini");
    CHECK(!mcp::checkArgs("settings", a).empty());
    // 3.0.24-beta.57: the full check reads which apply at once and which it leaves alone
    bool live = false, waits = false, skipped = false;
    for (const json::Value& s : every.items()) {
        CHECK(s.has("live"));
        if (s["key"].asString() == "effects.grass") live = s["live"].asBool() && !s.has("skip");
        if (s["key"].asString() == "ai.ground") waits = !s["live"].asBool(true);
        if (s["key"].asString() == "mcp.enabled") skipped = !s["skip"].asString().empty();
    }
    CHECK(live && waits && skipped);
}

TEST(mcp_probe_args) {
    json::Value a = json::Value::object();
    CHECK(!mcp::checkArgs("probe", a).empty());  // no action
    a.set("action", "start");
    CHECK(mcp::checkArgs("probe", a).empty());
    a.set("prefix", "effects.");
    a.set("photo", true);
    CHECK(mcp::checkArgs("probe", a).empty());
    a.set("prefix", "../x");
    CHECK(!mcp::checkArgs("probe", a).empty());
    a.set("prefix", "effects.");
    json::Value keys = json::Value::array();
    keys.push("effects.grass");
    a.set("keys", keys);
    CHECK(mcp::checkArgs("probe", a).empty());
    keys.push("Not A Key");
    a.set("keys", keys);
    CHECK(!mcp::checkArgs("probe", a).empty());
    json::Value many = json::Value::array();
    for (int i = 0; i < 701; ++i) many.push("effects.grass");
    a.set("keys", many);
    CHECK(!mcp::checkArgs("probe", a).empty());  // over 700
    for (const char* act : {"status", "stop", "restart", "verify"}) {
        json::Value b = json::Value::object();
        b.set("action", act);
        CHECK(mcp::checkArgs("probe", b).empty());
    }
    json::Value c = json::Value::object();
    c.set("action", "explode");
    CHECK(!mcp::checkArgs("probe", c).empty());
    c.set("action", "start");
    c.set("photo", "yes");
    CHECK(!mcp::checkArgs("probe", c).empty());
}

TEST(mcp_tour_args) {
    json::Value a = json::Value::object();
    CHECK(!mcp::checkArgs("tour", a).empty());
    for (const char* act : {"start", "here", "status", "stop"}) {
        json::Value b = json::Value::object();
        b.set("action", act);
        CHECK(mcp::checkArgs("tour", b).empty());
    }
    a.set("action", "start");
    json::Value only = json::Value::array();
    only.push("items");
    only.push("friend setup");
    a.set("only", only);
    CHECK(mcp::checkArgs("tour", a).empty());
    only.push(3);
    a.set("only", only);
    CHECK(!mcp::checkArgs("tour", a).empty());
    bool listed = false;
    for (const mcp::Tool& t : mcp::tools()) listed = listed || std::string(t.name) == "tour";
    CHECK(listed);
}
