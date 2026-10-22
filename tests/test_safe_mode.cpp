// 2.12: the safe mode - a game that failed to start twice in a row starts on Direct3D 11 without the risky parts.
#include <string>

#include "core/renderer_choice.hpp"
#include "core/safe_mode.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(safe_mode_after_two_failed_starts) {
    CHECK_EQ(safe::failedInRow(0, safe::Start::Failed), 1);
    CHECK_EQ(safe::failedInRow(1, safe::Start::Failed), 2);
    CHECK_EQ(safe::failedInRow(2, safe::Start::Fine), 0);  // a good start: counted from nothing again
    CHECK(!safe::needed(1, false));
    CHECK(safe::needed(2, false));
    CHECK(safe::needed(0, true));  // [renderer] safe_mode = 1
}

TEST(a_crash_after_the_first_picture_is_no_failed_start) {
    const std::string start = "uaRO Graphics 2.12.0 in C:\\uaRO\\uaRO.exe\n(uaRO renderer)\n";
    const std::string crash = "crash: exception 0xc0000005 at DDRAW.dll+0x1234\n";
    CHECK(safe::startFailed(start + crash));                                       // before any picture
    CHECK(!safe::startFailed(start + std::string(safe::kFirstPicture) + "\n" + crash));  // two hours later
    CHECK(safe::startFailed(start + "the uaRO renderer could not start\n"));
    CHECK(!safe::startFailed(start + std::string(safe::kFirstPicture) + "\nDirectDraw released, renderer closed\n"));
    CHECK(!safe::startFailed(""));  // no game before
}

TEST(safe_config_turns_the_risky_parts_off) {
    Config c;
    c.api = Api::D3d12;
    c.bridge = c.remix = c.hdr = c.renderThread = true;
    c.dlss = "dlaa";
    c.frameGeneration = 2;
    c.ai.enabled = true;
    c.ai.generate = true;
    safe::apply(c);
    CHECK(c.api == Api::D3d11);
    CHECK(!c.bridge && !c.remix && !c.hdr && !c.renderThread);
    CHECK(c.dlss == "off" && c.frameGeneration == 0);
    CHECK(!c.ai.on());
    CHECK(c.effects == effectsOff());
    Config off;
    off.api = Api::System;  // switched off stays off
    safe::apply(off);
    CHECK(off.api == Api::System);
}

TEST(safe_mode_without_direct3d_11_is_the_games_own_look) {
    Config c;
    safe::apply(c);
    SystemFacts f;
    f.windows10 = f.d3d12 = true;  // Direct3D 12 there, 11 not: still no risk taken
    CHECK(chooseRenderer(c, f, true).renderer == Renderer::System);
}

// A start sent to «Гра» (or with uaRO Graphics switched off) draws nothing of ours: it neither fails nor goes well -
// the count of failed starts stays as it was (a player who starts from the old shortcut every time).
TEST(a_start_that_drew_nothing_leaves_the_count) {
    const std::string redirected =
        "uaRO Graphics 2.12.0 in C:\\uaRO\\uaRO.exe\nlauncher: a direct start - «Гра» opens\n";
    CHECK(safe::judge(redirected) == safe::Start::Unknown);
    CHECK_EQ(safe::failedInRow(1, safe::judge(redirected)), 1);
    CHECK(safe::judge("(uaRO renderer)\ncrash: exception 0xc0000005\n") == safe::Start::Failed);
    CHECK(safe::judge(std::string("(uaRO renderer)\n") + safe::kFirstPicture + "\n") == safe::Start::Fine);
    CHECK_EQ(safe::failedInRow(1, safe::Start::Failed), 2);
    CHECK_EQ(safe::failedInRow(1, safe::Start::Fine), 0);
}
