#include "win/site_key.hpp"

#include <windows.h>

#include "core/json.hpp"
#include "win/code_page.hpp"
#include "win/user_files.hpp"

namespace uagfx::win::sitekey {
namespace {

constexpr const char* kFile = "site.dat";

struct State {
    bool read = false;
    unsigned long long stamp = 0;
    std::string token, nick;
};

State& state() {
    static State s;
    const unsigned long long stamp = userfiles::writeTime(kFile);
    if (!s.read || stamp != s.stamp) {  // read once, again when another game window signed in or out
        s.read = true;
        s.stamp = stamp;
        s.token.clear();
        s.nick.clear();
        std::string words, error;
        json::Value v;
        if (stamp && userfiles::readSealed(kFile, words) && json::parse(words, v, error)) {
            s.token = v["token"].asString();
            s.nick = v["nick"].asString();
        }
        if (!words.empty()) SecureZeroMemory(words.data(), words.size());
    }
    return s;
}

}  // namespace

std::string token() { return state().token; }

std::string nick() { return state().nick; }

bool save(const std::string& token, const std::string& nick) {
    json::Value v = json::Value::object();
    v.set("token", json::Value(token));
    v.set("nick", json::Value(nick));
    std::string words = json::dump(v);
    const bool ok = userfiles::writeSealed(kFile, words);
    SecureZeroMemory(words.data(), words.size());
    State& s = state();
    s.read = false;  // read again (its new stamp)
    return ok;
}

void forget() {
    userfiles::remove(kFile);
    state().read = false;
}

std::string deviceName() {
    wchar_t name[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD n = MAX_COMPUTERNAME_LENGTH + 1;
    const std::string computer = GetComputerNameW(name, &n) ? utf8Of(std::wstring(name, n)) : std::string("PC");
    return "uaRO · " + computer;
}

}  // namespace uagfx::win::sitekey
