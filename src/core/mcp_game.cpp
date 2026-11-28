#include "core/mcp_game.hpp"

#include "core/check_probe.hpp"
#include "core/live_config.hpp"
#include "core/settings/schema.hpp"

namespace uagfx::mcpgame {
namespace {

json::Value pair(int a, int b) {
    json::Value v = json::Value::array();
    v.push(a);
    v.push(b);
    return v;
}

json::Value list(const std::vector<itemstate::Item>& things, const std::function<std::string(int)>& nameOf) {
    json::Value out = json::Value::array();
    for (const itemstate::Item& i : things) {
        json::Value x = json::Value::object();
        x.set("index", int(i.index));
        x.set("id", int(i.id));
        const std::string name = nameOf ? nameOf(int(i.id)) : std::string();
        if (!name.empty()) x.set("name", name);
        x.set("amount", int64_t(i.amount));
        if (i.refine) x.set("refine", int(i.refine));
        if (i.places) x.set("worn", int(i.places));
        out.push(x);
    }
    return out;
}

const char* kindWord(chatpanel::Kind k) {
    switch (k) {
        case chatpanel::Kind::Public: return "public";
        case chatpanel::Kind::Party: return "party";
        case chatpanel::Kind::Guild: return "guild";
        case chatpanel::Kind::Whisper: return "whisper";
        case chatpanel::Kind::Announce: return "announce";
        case chatpanel::Kind::System: return "system";
        case chatpanel::Kind::Battle: return "battle";
        case chatpanel::Kind::Loot: return "loot";
    }
    return "other";
}

}  // namespace

json::Value character(const charblock::Data& d) {
    json::Value c = json::Value::object();
    c.set("name", d.name);
    c.set("job", d.job);
    c.set("baseLevel", d.baseLevel);
    c.set("jobLevel", d.jobLevel);
    c.set("hp", pair(d.hp, d.hpMost));
    c.set("sp", pair(d.sp, d.spMost));
    if (d.weightMost > 0) {
        json::Value w = json::Value::object();
        w.set("now", d.weightNow);
        w.set("most", d.weightMost);
        w.set("percent", d.weight);
        if (d.overweight >= 0) w.set("overweight", d.overweight);
        c.set("weight", w);
    }
    if (d.zeny >= 0) c.set("zeny", int64_t(d.zeny));
    c.set("statusPoints", d.statusPoints);
    c.set("skillPoints", d.skillPoints);
    if (d.ammo >= 0) c.set("ammo", d.ammo);
    if (!d.guild.empty()) c.set("guild", d.guild);
    if (!d.party.empty()) c.set("party", d.party);
    return c;
}

json::Value items(const std::vector<itemstate::Item>& bag, const std::vector<itemstate::Item>& cart,
                  const std::vector<itemstate::Item>& storage, const std::function<std::string(int id)>& nameOf) {
    json::Value r = json::Value::object();
    r.set("bag", list(bag, nameOf));
    r.set("cart", list(cart, nameOf));
    r.set("storage", list(storage, nameOf));
    return r;
}

json::Value chat(const std::vector<chatpanel::Line>& lines) {
    json::Value out = json::Value::array();
    for (const chatpanel::Line& l : lines) {
        json::Value x = json::Value::object();
        x.set("kind", kindWord(l.kind));
        if (!l.who.empty()) x.set("who", l.who);
        std::string text;
        for (const chatpanel::Span& s : l.spans) text += s.text;
        x.set("text", text);
        if (l.repeat > 1) x.set("repeat", l.repeat);
        out.push(x);
    }
    return out;
}

json::Value settings(const Config& c, const std::string& prefix, settings::Lang lang) {
    json::Value out = json::Value::array();
    for (const settings::IniSetting& s : settings::iniSettings()) {
        const std::string key = settings::sectionKey(s);
        if (key.compare(0, prefix.size(), prefix) != 0) continue;
        json::Value x = json::Value::object();
        x.set("key", key);
        x.set("value", settings::tokenOf(s, c));
        const settings::Control k = s.control;
        const bool slider = k == settings::Control::Slider || k == settings::Control::Number;
        x.set("kind", k == settings::Control::Slider   ? "slider"
                      : k == settings::Control::Number ? "number"
                      : k == settings::Control::Check  ? "check"
                      : k == settings::Control::Line   ? "text"
                                                       : "choice");
        if (k == settings::Control::Line) {
        } else if (slider) {
            x.set("min", s.min);
            x.set("max", s.max);
            x.set("step", s.step);
        } else {
            json::Value values = json::Value::array();
            for (const char* t : s.tokens) values.push(std::string(t));
            x.set("values", values);
        }
        x.set("label", settings::tr(lang, s.label));
        x.set("live", appliesNow(key));  // 3.0.24-beta.57: the full check - at once, or at the next start
        const std::string skip = checkprobe::skipReason(key);
        if (!skip.empty()) x.set("skip", skip);
        out.push(x);
    }
    return out;
}

}  // namespace uagfx::mcpgame
