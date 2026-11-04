// 3.0.11-beta.212 (the tester: «віспер чат - в старому стилі»): the game's 1:1 whisper windows (class 0x699BC8, one a
// partner) as cards (core/whisper_card) - the partner's name, the lines, the game's own line to type into, «Надіслати»
// and ✕; several of them cascaded
#include <algorithm>
#include <string>
#include <vector>

#include "core/whisper_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

whispercard::Card card(int lines) {
    whispercard::Card c;
    c.name = "CHAMPanzee09";
    for (int i = 0; i < lines; ++i) c.lines.push_back(whispercard::Line{"line " + std::to_string(i), 0});
    return c;
}

bool has(const std::vector<whispercard::Words>& w, const std::string& text) {
    return std::any_of(w.begin(), w.end(), [&](const whispercard::Words& x) { return x.text == text; });
}

}  // namespace

// Cascaded: each next one lower and to the right, all inside the screen
TEST(whisper_card_cascade) {
    const whispercard::Layout a = whispercard::layout(0, 1920, 1080, 1.0f);
    const whispercard::Layout b = whispercard::layout(1, 1920, 1080, 1.0f);
    CHECK(b.x > a.x && b.y > a.y);
    for (int i = 0; i < 12; ++i) {
        const whispercard::Layout L = whispercard::layout(i, 1280, 720, 1.0f);
        CHECK(L.x >= 0 && L.y >= 0 && L.x + L.w <= 1280 && L.y + L.h <= 720);
    }
}

// The parts where they are drawn
TEST(whisper_card_hits) {
    const whispercard::Layout L = whispercard::layout(0, 1920, 1080, 1.0f);
    using P = whispercard::Part;
    const auto at = [&](const panel::Box& b) { return whispercard::hitTest(L, b.x + b.w / 2, b.y + b.h / 2); };
    CHECK(at(L.input) == P::Input && at(L.send) == P::Send && at(L.close) == P::Close);
    CHECK(at(L.lines) == P::Body && whispercard::hitTest(L, -2, -2) == P::None);
    CHECK(L.lineRows >= 5);
}

// The words: the partner, the newest lines that fit (the newest last), the line typed with its caret
TEST(whisper_card_words) {
    whispercard::Card c = card(30);
    c.input = "привіт";
    c.focus = true;
    c.caret = true;
    const whispercard::Layout L = whispercard::layout(0, 1920, 1080, 1.0f);
    whispercard::Texts t;
    t.with = "Шепіт: {1}";
    t.send = "Надіслати";
    const std::vector<whispercard::Words> w = whispercard::words(L, c, whispercard::Part::None, t);
    CHECK(has(w, "Шепіт: CHAMPanzee09") && has(w, "Надіслати") && has(w, "привіт|"));
    CHECK(has(w, "line 29") && !has(w, "line 0"));
}
