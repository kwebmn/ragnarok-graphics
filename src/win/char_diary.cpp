#include "win/char_diary.hpp"

#include <windows.h>

#include <ctime>

#include "core/char_status.hpp"
#include "core/layout.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::win::chardiarywin {
namespace {

constexpr uint32_t kJobAt = 0x771F8C;
constexpr ULONGLONG kWriteMs = 60000;

struct State {
    bool read = false, dirty = false;
    chardiary::Diary diary;
    std::string name, map;
    int level = 0, jobLevel = 0;
    ULONGLONG lastTick = 0, writtenAt = 0;
    // 3.0.11-beta.274 (feature 4): where the session began - its time, levels, experience, zeny
    long long startedAt = 0, startZeny = 0, startBaseExp = 0, startBaseNext = 0, startJobExp = 0, startJobNext = 0;
    int startBase = 0, startJob = 0;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

std::string path() { return layout::join(gameDir(), layout::kCharDiary); }

void readIt(State& s) {
    if (s.read) return;
    s.read = true;
    std::string text;
    if (readTextFile(path(), text)) s.diary = chardiary::parse(text);
}

void writeIt(State& s, ULONGLONG now) {
    s.writtenAt = now;
    s.dirty = false;
    if (!writeFileAtomic(path(), chardiary::text(s.diary)) && s.told++ < 5)
        LOG_INFO("characters: %s cannot be written", path().c_str());
}

}  // namespace

long long nowUnix() { return static_cast<long long>(std::time(nullptr)); }

void frame(unsigned codePage) {
    State& s = state();
    const ULONGLONG now = GetTickCount64();
    const ULONGLONG dt = s.lastTick && now > s.lastTick ? now - s.lastTick : 0;
    s.lastTick = now;
    if (!inGameWorld()) {
        if (s.dirty) writeIt(s, now);  // left the world: what it saw kept at once
        s.name.clear();
        return;
    }
    const std::string raw = readOwnName();
    const std::string map = readGameMap();
    charstatus::Status st;
    if (raw.empty() || map.empty() || !readOwnStatus(st) || st.baseLevel <= 0) return;
    readIt(s);
    const std::string name = utf8Of(widened(raw, codePage));
    chardiary::Entry& e = s.diary[name];
    const bool changed = name != s.name || map != s.map || st.baseLevel != s.level || st.jobLevel != s.jobLevel;
    if (name != s.name) {  // .274: a session begins
        if (s.told++ < 20) LOG_INFO("characters: %s in the game - kept in the diary", name.c_str());
        s.startedAt = nowUnix();
        s.startBase = st.baseLevel;
        s.startJob = st.jobLevel;
        s.startBaseExp = st.baseExp;
        s.startBaseNext = st.baseNext;
        s.startJobExp = st.jobExp;
        s.startJobNext = st.jobNext;
        s.startZeny = st.zeny;
    }
    e.sessionSeconds = nowUnix() - s.startedAt;  // .274: the session so far, its gains
    e.sessionEnd = nowUnix();
    e.baseGain = chardiary::gained(s.startBase, s.startBaseExp, s.startBaseNext, st.baseLevel, st.baseExp,
                                   st.baseNext);
    e.jobGain = chardiary::gained(s.startJob, s.startJobExp, s.startJobNext, st.jobLevel, st.jobExp, st.jobNext);
    e.zenyGain = static_cast<long long>(st.zeny) - s.startZeny;
    e.lastSeen = nowUnix();
    e.map = map;
    e.level = st.baseLevel;
    e.jobLevel = st.jobLevel;
    uint32_t job = 0;
    if (readGame(kJobAt, &job, 4)) e.job = int(job);
    static ULONGLONG part = 0;  // the time played: whole seconds into the entry, the rest kept
    part += name == s.name && dt < 5000 ? dt : 0;
    e.played += static_cast<long long>(part / 1000);
    part %= 1000;
    s.name = name;
    s.map = map;
    s.level = st.baseLevel;
    s.jobLevel = st.jobLevel;
    s.dirty = true;
    if (changed || now - s.writtenAt >= kWriteMs) writeIt(s, now);
}

const chardiary::Entry* find(const std::string& nameUtf8) {
    State& s = state();
    readIt(s);
    const auto it = s.diary.find(nameUtf8);
    return it == s.diary.end() ? nullptr : &it->second;
}

}  // namespace uagfx::win::chardiarywin
