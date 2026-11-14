#include "core/quest_progress.hpp"

#include "core/json.hpp"

namespace uagfx::questprogress {

bool parse(const std::string& text, std::vector<State>& out) {
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error) || v.type() != json::Value::Object || v["quests"].type() != json::Value::Array)
        return false;
    std::vector<State> states;
    for (const json::Value& q : v["quests"].items()) {
        State s;
        s.slug = q["slug"].asString();
        if (s.slug.empty()) continue;
        s.value = int(q["value"].asInt(0));
        s.step = int(q["step"].asInt(0));
        s.steps = int(q["steps"].asInt(0));
        s.started = q["started"].asBool(false);
        s.done = q["done"].asBool(false);
        const json::Value& n = q["npc"];
        if (n.type() == json::Value::Object) {
            s.npc = n["name"].asString();
            s.map = n["map"].asString();
            s.x = int(n["x"].asInt(-1));
            s.y = int(n["y"].asInt(-1));
        }
        states.push_back(std::move(s));
    }
    out = std::move(states);
    return true;
}

const State* find(const std::vector<State>& states, const std::string& slug) {
    for (const State& s : states)
        if (s.slug == slug) return &s;
    return nullptr;
}

}  // namespace uagfx::questprogress
