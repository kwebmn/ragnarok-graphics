// 3.0.21-beta.13 (the owner: «надо еще подсказку сделать что мышка работает при нажатии на альт. вообще у нас с
// подобными подсказками проблема. надо пройтись и в таких моментах наделать подсказки, так как люди не разберутся как
// пользоваться»): the key hints - at the moments the driver changes what the keys and the mouse do, a line at the
// bottom middle says what to press (core/key_hints)
#include <array>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/key_hints.hpp"
#include "core/live_config.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;
namespace kh = uagfx::keyhint;
using kh::Moment;

namespace {
bool has(const std::array<bool, kh::kMoments>& a, Moment m) { return a[size_t(m)]; }
std::string keysOf(const std::vector<kh::Part>& parts) {
    std::string out;
    for (const kh::Part& p : parts) out += "[" + p.keys + "]";
    return out;
}
const kh::Part* partWith(const std::vector<kh::Part>& parts, const std::string& keys) {
    for (const kh::Part& p : parts)
        if (p.keys == keys) return &p;
    return nullptr;
}
std::array<bool, kh::kMoments> only(Moment m) {
    std::array<bool, kh::kMoments> a{};
    a[size_t(m)] = true;
    return a;
}
}  // namespace

// Which moments the game is in: the 3D view (entered), a window open while the mouse turns the view (Alt held: the
// cursor is free - no moment), the free camera, a skill waiting for its target while looking, the storage or the cart
TEST(key_hints_moments) {
    kh::Now n;
    const std::array<bool, kh::kMoments> nothing{};
    CHECK(kh::momentsOf(n) == nothing);
    n.view3d = n.looking = true;
    std::array<bool, kh::kMoments> a = kh::momentsOf(n);
    CHECK(has(a, Moment::View3d) && !has(a, Moment::View3dWindow) && !has(a, Moment::FreeCamera));
    n.windowOpen = true;
    CHECK(has(kh::momentsOf(n), Moment::View3dWindow));
    n.altHeld = true, n.looking = false;  // the cursor free already
    CHECK(!has(kh::momentsOf(n), Moment::View3dWindow));
    n = kh::Now{};
    n.view3d = n.looking = n.freeCamera = true;
    a = kh::momentsOf(n);
    CHECK(has(a, Moment::FreeCamera) && !has(a, Moment::View3d));
    n.flightPoints = 1;  // 3.0.22-beta.24: a point set - the flight's line in the free camera's place
    a = kh::momentsOf(n);
    CHECK(has(a, Moment::Flight) && !has(a, Moment::FreeCamera));
    n = kh::Now{};
    n.lapse = 2;  // the time stopped: its line, in any view
    CHECK(has(kh::momentsOf(n), Moment::TimeLapse));
    n = kh::Now{};
    n.view3d = n.looking = true;
    n.skillWait = 2;
    CHECK(has(kh::momentsOf(n), Moment::View3dSkill));
    n.looking = false;  // Alt held: the game's own press
    CHECK(!has(kh::momentsOf(n), Moment::View3dSkill));
    n = kh::Now{};
    n.windowOpen = true, n.skillWait = 2;  // the usual view: none of the 3D view's
    a = kh::momentsOf(n);
    CHECK(!has(a, Moment::View3dWindow) && !has(a, Moment::View3dSkill));
    n.itemsShared = true;
    CHECK(has(kh::momentsOf(n), Moment::ItemPicks));
}

// What each line says - the keys as caps, the words after them, in the player's language
TEST(key_hints_lines) {
    kh::Now n;
    n.view3d = n.looking = n.windowOpen = true;
    const std::vector<kh::Part> window = kh::partsOf(Moment::View3dWindow, n, settings::Lang::Uk);
    CHECK(window.size() == 1 && window[0].keys == "Alt" && window[0].words.find("вікн") != std::string::npos);
    n.behind = true;
    const std::vector<kh::Part> behind = kh::partsOf(Moment::View3d, n, settings::Lang::Uk);
    CHECK_EQ(keysOf(behind), std::string("[Alt][Колесо][Shift][Tab][Ctrl+Shift+V]"));
    const kh::Part* next = partWith(behind, "Ctrl+Shift+V");
    CHECK(next && next->words == "від першої особи");
    n.behind = false;  // from the eyes: no distance, no camera round the character; the next view the usual one
    const std::vector<kh::Part> eyes = kh::partsOf(Moment::View3d, n, settings::Lang::Uk);
    CHECK_EQ(keysOf(eyes), std::string("[Alt][Tab][Ctrl+Shift+V]"));
    next = partWith(eyes, "Ctrl+Shift+V");
    CHECK(next && next->words == "звичайний вигляд");
    CHECK_EQ(keysOf(kh::partsOf(Moment::FreeCamera, n, settings::Lang::Uk)),
             std::string("[W A S D][Пробіл][C][Shift][Ctrl+Shift+P][Ctrl+Shift+C]"));
    // 3.0.22-beta.24 (the owner 19.11 «делай 1-4»): the flight's line once a point is set; the time-lapse's
    n.flightPoints = 2;
    const std::vector<kh::Part> flight = kh::partsOf(Moment::Flight, n, settings::Lang::Uk);
    CHECK_EQ(keysOf(flight), std::string("[Ctrl+Shift+P][Ctrl+Shift+O][Ctrl+Shift+L][Ctrl+Shift+C]"));
    CHECK(flight[1].words == "пролетіти");
    n.flying = true;
    CHECK(kh::partsOf(Moment::Flight, n, settings::Lang::Uk)[1].words == "зупинити політ");
    n.flightPoints = 0;
    n.flying = false;
    n.lapse = 1;
    CHECK(kh::partsOf(Moment::TimeLapse, n, settings::Lang::Uk)[0].words == "зупинити час");
    n.lapse = 2;
    CHECK(kh::partsOf(Moment::TimeLapse, n, settings::Lang::Uk)[0].words == "знову «Годинник»");
    n.lapse = 0;
    n.skillWait = 2;
    const std::vector<kh::Part> enemy = kh::partsOf(Moment::View3dSkill, n, settings::Lang::Uk);
    n.skillWait = 4;
    const std::vector<kh::Part> friendly = kh::partsOf(Moment::View3dSkill, n, settings::Lang::Uk);
    CHECK(enemy.size() == 1 && friendly.size() == 1 && enemy[0].words != friendly[0].words);
    CHECK(friendly[0].words.find("себе") != std::string::npos);
    // 3.0.22-beta.7: Alt+right click moves a thing whole - first in the storage's and the cart's line
    const std::vector<kh::Part> items = kh::partsOf(Moment::ItemPicks, n, settings::Lang::Uk);
    CHECK_EQ(keysOf(items), std::string("[Alt+ПКМ][Ctrl+клік][Shift+клік][Ctrl+A]"));
    CHECK(items[0].words == "перенести");
    CHECK_EQ(kh::partsOf(Moment::ItemPicks, n, settings::Lang::En)[0].keys, std::string("Alt+right click"));
    // the other languages: their own words
    const std::vector<kh::Part> ru = kh::partsOf(Moment::View3dWindow, n, settings::Lang::Ru);
    const std::vector<kh::Part> en = kh::partsOf(Moment::View3dWindow, n, settings::Lang::En);
    CHECK(ru.size() == 1 && en.size() == 1 && ru[0].words != window[0].words && en[0].words != ru[0].words);
    n.behind = true;
    CHECK(partWith(kh::partsOf(Moment::View3d, n, settings::Lang::En), "Wheel") != nullptr);
}

// How many times each has shown and which the player knows, kept between the games
TEST(key_hints_seen_kept) {
    kh::Seen s;
    s.times[size_t(Moment::View3d)] = 3;
    s.learned[size_t(Moment::View3dWindow)] = true;
    const kh::Seen back = kh::read(kh::write(s));
    CHECK(back.times == s.times && back.learned == s.learned);
    const kh::Seen odd = kh::read("view3d 9\nnonsense 4\nview3d_window learned\r\nfree_camera -2\n");
    CHECK_EQ(odd.times[size_t(Moment::View3d)], kh::kTimes);  // no more than it can be
    CHECK(odd.learned[size_t(Moment::View3dWindow)]);
    CHECK_EQ(odd.times[size_t(Moment::FreeCamera)], 0);
    // 3.0.22-beta.24: the free camera's line with Ctrl+Shift+P under a new name - shown anew to all
    CHECK_EQ(std::string(kh::keyOf(Moment::FreeCamera)), std::string("free_camera_points"));
    const kh::Seen camera = kh::read("free_camera learned\nfree_camera 5\n");
    CHECK(!camera.learned[size_t(Moment::FreeCamera)] && camera.times[size_t(Moment::FreeCamera)] == 0);
    // 3.0.22-beta.7: the storage's line changed (Alt+right click first) under a new name - shown anew to all
    CHECK_EQ(std::string(kh::keyOf(Moment::ItemPicks)), std::string("item_moves"));
    const kh::Seen old = kh::read("item_picks learned\nitem_picks 5\n");
    CHECK(!old.learned[size_t(Moment::ItemPicks)] && old.times[size_t(Moment::ItemPicks)] == 0);
}

// A moment's line shows from its start for its time at most (counted once it showed a second), the first kTimes
// times it comes; never again once the player has done what it says
TEST(key_hints_a_few_times_then_no_more) {
    kh::Shower sh;
    kh::Seen seen;
    bool changed = false;
    const std::array<bool, kh::kMoments> none{}, entered = only(Moment::View3d);
    uint32_t t = 1000;
    for (int episode = 0; episode < kh::kTimes; ++episode) {
        CHECK(sh.frame(t, entered, none, seen, changed) == Moment::View3d);
        t += 500;
        CHECK(sh.frame(t, entered, none, seen, changed) == Moment::View3d);
        changed = false;
        for (int i = 0; i < 20; ++i) sh.frame(t += 100, entered, none, seen, changed);  // 2.5 s in all
        CHECK(changed);
        CHECK_EQ(seen.times[size_t(Moment::View3d)], episode + 1);
        Moment last = Moment::View3d;
        for (int i = 0; i < 80; ++i) last = sh.frame(t += 100, entered, none, seen, changed);  // past its time
        CHECK(last == Moment::Count);
        CHECK_EQ(seen.times[size_t(Moment::View3d)], episode + 1);  // one episode, one time
        sh.frame(t += 100, none, none, seen, changed);  // the view left
    }
    CHECK(sh.frame(t += 100, entered, none, seen, changed) == Moment::Count);  // shown kTimes times: no more
    // a short glance does not count: under a second
    kh::Shower quick;
    kh::Seen fresh;
    quick.frame(100, entered, none, fresh, changed);
    quick.frame(600, entered, none, fresh, changed);
    quick.frame(700, none, none, fresh, changed);
    CHECK_EQ(fresh.times[size_t(Moment::View3d)], 0);
}

TEST(key_hints_learned_never_again) {
    kh::Shower sh;
    kh::Seen seen;
    bool changed = false;
    const std::array<bool, kh::kMoments> none{}, window = only(Moment::View3dWindow);
    CHECK(sh.frame(100, window, none, seen, changed) == Moment::View3dWindow);
    // a window open while the mouse turns the view: the line stays as long as the window does
    uint32_t t = 100;
    for (int i = 0; i < 300; ++i) CHECK(sh.frame(t += 100, window, none, seen, changed) == Moment::View3dWindow);
    changed = false;
    CHECK(sh.frame(t += 100, none, window, seen, changed) == Moment::Count);  // Alt held: done
    CHECK(changed && seen.learned[size_t(Moment::View3dWindow)]);
    sh.frame(t += 100, none, none, seen, changed);
    CHECK(sh.frame(t += 100, window, none, seen, changed) == Moment::Count);  // known: never again
}

// The one where the player is stuck first; the other keeps its own time for later
TEST(key_hints_the_stuck_one_first) {
    kh::Shower sh;
    kh::Seen seen;
    bool changed = false;
    const std::array<bool, kh::kMoments> none{};
    std::array<bool, kh::kMoments> both = only(Moment::View3d);
    both[size_t(Moment::View3dWindow)] = true;
    uint32_t t = 0;
    for (int i = 0; i < 120; ++i) CHECK(sh.frame(t += 100, both, none, seen, changed) == Moment::View3dWindow);
    CHECK_EQ(seen.times[size_t(Moment::View3d)], 0);  // not shown yet: not counted
    // the window closed: the 3D view's own line for its whole time
    const std::array<bool, kh::kMoments> entered = only(Moment::View3d);
    int shown = 0;
    for (int i = 0; i < 120; ++i) shown += sh.frame(t += 100, entered, none, seen, changed) == Moment::View3d ? 1 : 0;
    CHECK(shown >= 75 && shown <= 85);  // kept about its 8 s
}

// The line's look: each key a cap («Ctrl+Shift+V» three, «W A S D» four), the words after them, the glass centred
TEST(key_hints_a_line_of_caps) {
    CHECK(kh::capsOf("Ctrl+Shift+V") == std::vector<std::string>({"Ctrl", "Shift", "V"}));
    CHECK(kh::capsOf("W A S D") == std::vector<std::string>({"W", "A", "S", "D"}));
    CHECK(kh::capsOf("Alt") == std::vector<std::string>({"Alt"}));
    const std::vector<kh::Part> parts = {{"Alt", "курсор для вікон"}, {"Ctrl+Shift+V", "звичайний вигляд"}};
    const kh::Layout L = kh::layout(parts, 960, 900, 1.0f);
    CHECK_EQ(L.parts.size(), size_t(2));
    if (L.parts.size() != 2) return;
    CHECK_EQ(L.parts[0].caps.size(), size_t(1));
    CHECK_EQ(L.parts[1].caps.size(), size_t(3));
    CHECK(L.x + L.w / 2 >= 959 && L.x + L.w / 2 <= 961 && L.y + L.h == 900);
    CHECK(L.parts[0].words.x > L.parts[0].caps[0].x + L.parts[0].caps[0].w);    // the words after the caps
    CHECK(L.parts[1].caps[0].x > L.parts[0].words.x + L.parts[0].words.w);      // the next part after them
    CHECK(L.parts[1].caps[1].x > L.parts[1].caps[0].x + L.parts[1].caps[0].w);  // a cap after a cap
    const kh::Layout big = kh::layout(parts, 960, 900, 2.0f);
    CHECK(big.w > L.w * 3 / 2 && big.h > L.h * 3 / 2);
    const std::vector<kh::Words> w = kh::words(L, parts);
    int caps = 0, said = 0;
    for (const kh::Words& x : w) (x.bold ? caps : said) += 1;
    CHECK_EQ(caps, 4);
    CHECK_EQ(said, 2);
}

// [interface] control_hints - on at first; a row of the game's «Налаштування», at once
TEST(config_interface_key_hints) {
    CHECK(parseConfig("").keyHints);
    CHECK(!parseConfig("[interface]\ncontrol_hints = 0\n").keyHints);
    const settings::IniSetting* s = settings::findSetting("interface.control_hints");
    CHECK(s != nullptr && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.control_hints"));
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[interface]\ncontrol_hints = 0\n"));
    CHECK(!u.next.keyHints && u.waiting.empty() && u.applied.size() == 1 && u.applied[0] == "control_hints 1 -> 0");
}
