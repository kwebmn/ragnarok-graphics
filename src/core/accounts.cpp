#include "core/accounts.hpp"

#include <cctype>

#include "core/json.hpp"

namespace uagfx::accounts {
namespace {

std::string lower(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = char(std::tolower((unsigned char)c));
    return out;
}

Account* find(Shelf& s, const std::string& login) {
    const std::string l = lower(login);
    for (Account& a : s.list)
        if (lower(a.login) == l) return &a;
    return nullptr;
}

json::Value charValue(const LastChar& c) {
    json::Value v = json::Value::object();
    v.set("name", json::Value(c.name));
    v.set("job", json::Value(c.job));
    v.set("map", json::Value(c.map));
    v.set("jobId", json::Value(c.jobId));
    v.set("level", json::Value(c.level));
    v.set("slot", json::Value(c.slot));
    v.set("sex", json::Value(c.sex));
    v.set("hair", json::Value(c.hair));
    v.set("hairColour", json::Value(c.hairColour));
    return v;
}

LastChar charOf(const json::Value& v) {
    LastChar c;
    c.name = v["name"].asString();
    c.job = v["job"].asString();
    c.map = v["map"].asString();
    c.jobId = int(v["jobId"].asInt(0));
    c.level = int(v["level"].asInt(0));
    c.slot = int(v["slot"].asInt(-1));
    c.sex = int(v["sex"].asInt(0));
    c.hair = int(v["hair"].asInt(0));
    c.hairColour = int(v["hairColour"].asInt(0));
    return c;
}

}  // namespace

std::string toJson(const Shelf& s) {
    json::Value list = json::Value::array();
    for (const Account& a : s.list) {
        json::Value v = json::Value::object();
        v.set("login", json::Value(a.login));
        if (a.keepPassword) v.set("password", json::Value(a.password));
        v.set("label", json::Value(a.label));
        v.set("keepPassword", json::Value(a.keepPassword));
        v.set("autoEnter", json::Value(a.autoEnter));
        v.set("refused", json::Value(a.refused));
        v.set("usedAt", json::Value(int64_t(a.usedAt)));
        v.set("last", charValue(a.last));
        list.push(std::move(v));
    }
    json::Value v = json::Value::object();
    v.set("v", json::Value(1));
    v.set("addAfterLogin", json::Value(s.addAfterLogin));
    v.set("list", std::move(list));
    return json::dump(v);
}

bool fromJson(const std::string& text, Shelf& out) {
    out = Shelf{};
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error) || v.type() != json::Value::Object || v["list"].type() != json::Value::Array)
        return false;
    Shelf s;
    s.addAfterLogin = v["addAfterLogin"].asBool(true);
    for (const json::Value& r : v["list"].items()) {
        Account a;
        a.login = r["login"].asString();
        if (a.login.empty() || find(s, a.login)) continue;
        a.keepPassword = r["keepPassword"].asBool(true);
        if (a.keepPassword) a.password = r["password"].asString();
        a.label = r["label"].asString();
        a.autoEnter = r["autoEnter"].asBool(true);
        a.refused = r["refused"].asBool(false);
        a.usedAt = r["usedAt"].asInt(0);
        a.last = charOf(r["last"]);
        s.list.push_back(std::move(a));
    }
    out = std::move(s);
    return true;
}

void upsert(Shelf& s, const std::string& login, const std::string& password, long long now) {
    if (login.empty()) return;
    Account* a = find(s, login);
    if (!a) {
        s.list.push_back(Account{});
        a = &s.list.back();
        a->login = login;
    }
    a->password = password;
    a->refused = false;
    a->usedAt = now;
}

bool remove(Shelf& s, const std::string& login) {
    Account* a = find(s, login);
    if (!a) return false;
    s.list.erase(s.list.begin() + (a - s.list.data()));
    return true;
}

bool move(Shelf& s, size_t index, int delta) {
    const long long to = (long long)index + delta;
    if (index >= s.list.size() || delta == 0 || to < 0 || to >= (long long)s.list.size()) return false;
    std::swap(s.list[index], s.list[size_t(to)]);
    return true;
}

void recordChar(Shelf& s, const std::string& login, const LastChar& c, long long now) {
    if (Account* a = find(s, login)) {
        a->last = c;
        a->usedAt = now;
    }
}

uint64_t loginHash(const std::string& login) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (const char c : lower(login)) {
        h ^= uint8_t(c);
        h *= 0x100000001b3ull;
    }
    return h;
}

}  // namespace uagfx::accounts
