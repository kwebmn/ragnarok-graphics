#include "win/accounts_store.hpp"

#include <windows.h>

#include <string>

#include "win/log.hpp"
#include "win/user_files.hpp"

namespace uagfx::win::accountsstore {
namespace {

constexpr const char* kFile = "accounts.dat";  // under %LOCALAPPDATA%\uaroGraphics (win/user_files)
constexpr ULONGLONG kLookEveryMs = 2000;

struct State {
    accounts::Shelf shelf;
    bool read = false;
    unsigned long long stamp = 0;  // the file's write time as last read or written, 0 none
    ULONGLONG lookedAt = 0;
};

State& state() {
    static State s;
    return s;
}

void load(State& s) {
    s.shelf = accounts::Shelf{};
    s.stamp = userfiles::writeTime(kFile);
    if (!s.stamp) return;  // none yet: an empty shelf
    std::string words;
    const bool ok = userfiles::readSealed(kFile, words) && accounts::fromJson(words, s.shelf);
    SecureZeroMemory(words.data(), words.size());
    if (ok) return;
    s.shelf = accounts::Shelf{};
    userfiles::keepAside(kFile);
    s.stamp = 0;
    LOG_INFO("login: the saved accounts could not be read - kept as accounts.dat.bad");
}

}  // namespace

accounts::Shelf& shelf() {
    State& s = state();
    const ULONGLONG now = GetTickCount64();
    if (!s.read || now - s.lookedAt >= kLookEveryMs) {
        s.lookedAt = now;
        if (!s.read || userfiles::writeTime(kFile) != s.stamp) load(s);  // another game window changed it, or forgot it
        s.read = true;
    }
    return s.shelf;
}

bool save() {
    State& s = state();
    std::string words = accounts::toJson(s.shelf);
    const bool saved = userfiles::writeSealed(kFile, words);
    SecureZeroMemory(words.data(), words.size());
    if (!saved) {
        LOG_ERROR("login: the accounts could not be saved");
        return false;
    }
    s.stamp = userfiles::writeTime(kFile);
    s.read = true;
    return true;
}

}  // namespace uagfx::win::accountsstore
