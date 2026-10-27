#include "core/report_answers.hpp"

#include <cstdint>

#include "core/json.hpp"

namespace uagfx::answers {

std::string write(const std::vector<reports::Answer>& list) {
    json::Value all = json::Value::array();
    for (const reports::Answer& a : list) {
        json::Value one = json::Value::object();
        one.set("id", json::Value(int64_t(a.id)));
        one.set("code", a.code);
        one.set("status", reports::statusToken(a.status));
        one.set("fixed_in", a.fixedIn);
        one.set("at", a.at);
        one.set("text", a.text);
        all.push(one);
    }
    json::Value root = json::Value::object();
    root.set("answers", all);
    return json::dump(root);
}

bool read(const std::string& text, std::vector<reports::Answer>& out) {
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error) || v.type() != json::Value::Object ||
        v["answers"].type() != json::Value::Array)
        return false;
    std::vector<reports::Answer> list;
    for (const json::Value& one : v["answers"].items()) {
        reports::Answer a;
        a.code = one["code"].asString();
        if (!plainCode(a.code)) continue;
        a.id = one["id"].asInt();
        a.status = reports::statusOf(one["status"].asString());
        a.fixedIn = one["fixed_in"].asString();
        a.at = one["at"].asString();
        a.text = one["text"].asString();
        list.push_back(a);
    }
    out = list;
    return true;
}

std::string key(const reports::Answer& a) {
    return a.id > 0 ? a.code + "#" + std::to_string(a.id) : a.code + "@" + a.at;
}

bool plainCode(const std::string& code) {
    if (code.size() != 8 || code[0] != 'R' || code[1] != '-') return false;
    for (size_t i = 2; i < code.size(); ++i) {
        const char ch = code[i];
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))) return false;
    }
    return true;
}

}  // namespace uagfx::answers
