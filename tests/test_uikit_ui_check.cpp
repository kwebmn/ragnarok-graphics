// 3.0.24-beta.34 (docs/superpowers/plans/2026-11-27-mcp-full-test.md, task 4): the MCP's ui check - a window's faint
// words (light on light, R-8BCJAH's) and those its box cuts; the readable ones not named
#include <string>

#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/ui_check.hpp"

using namespace uagfx;

TEST(uikit_ui_check_finds_faint_and_cut_words) {
    const char* rml = R"RML(<rml><head><style>
body { width: 400px; height: 200px; background-color: #f6f8fc; font-family: Plex; font-size: 14px; }
.ok { display: block; color: #181c26; }
.faint { display: block; color: #eceff5; }
.box { display: block; width: 40px; overflow: hidden; white-space: nowrap; color: #181c26; }
</style></head><body><div class="ok">Readable</div><div class="faint">Settings</div>
<div class="box">A name far too long for its box</div></body></rml>)RML";
    std::string error;
    Rml::ElementDocument* doc = rmlhost::load(rml, nullptr, 0, 0, &error);
    CHECK(doc != nullptr);
    if (!doc) return;
    rmlhost::update();
    const std::vector<uikit::uicheck::Word> all = uikit::uicheck::words(doc, uikit::uicheck::Rgbf{0.5f, 0.5f, 0.5f});
    CHECK_EQ(all.size(), size_t(3));
    const std::vector<uikit::uicheck::Word> bad = uikit::uicheck::problems(doc);
    CHECK_EQ(bad.size(), size_t(2));
    bool faint = false, cut = false;
    for (const uikit::uicheck::Word& w : bad) {
        if (w.text == "Settings") faint = w.contrast < 1.2f && !w.cut;
        if (w.text.rfind("A name", 0) == 0) cut = w.cut && w.contrast > 10;
    }
    CHECK(faint && cut);
    rmlhost::close(doc);
}

// The live sheets' grounds are boxes of their own beside the words (the HUD's: uisheet's painted boxes), not their
// parents: a word's ground is whatever was drawn before it under its middle
TEST(uikit_ui_check_grounds_drawn_beside_the_words) {
    const char* rml = R"RML(<rml><head><style>
body { width: 400px; height: 200px; font-family: Plex; font-size: 14px; }
.paint { position: absolute; left: 0; top: 0; width: 300px; height: 100px; background-color: #10141c; }
.word { position: absolute; left: 10px; top: 10px; width: 200px; height: 20px; color: #9aa3b5; }
</style></head><body><div class="paint"></div><div class="word">Archer</div></body></rml>)RML";
    std::string error;
    Rml::ElementDocument* doc = rmlhost::load(rml, nullptr, 0, 0, &error);
    CHECK(doc != nullptr);
    if (!doc) return;
    rmlhost::update();
    const std::vector<uikit::uicheck::Word> all = uikit::uicheck::words(doc, uikit::uicheck::Rgbf{0.45f, 0.45f, 0.45f});
    CHECK_EQ(all.size(), size_t(1));
    CHECK(!all.empty() && all[0].ground == 0x10141Cu && all[0].contrast > 6.0f);
    CHECK(uikit::uicheck::problems(doc).empty());
    rmlhost::close(doc);
}

// The laptop's run (3.0.24-beta.35): the ui tool's answer broke on a word cut at 60 bytes in a letter's middle
// (Cyrillic: two bytes a letter) - its text cut at a letter's edge, whole UTF-8
TEST(uikit_ui_check_cuts_words_at_a_letter) {
    const char* rml = R"RML(<rml><head><style>
body { width: 900px; height: 100px; font-family: Plex; font-size: 14px; }
.w { display: block; color: #181c26; background-color: #f6f8fc; }
</style></head><body><div class="w">QЯбвгдежзийклмнопрстуфхцчшщьюяабвгдежзийклмнопрст</div></body></rml>)RML";
    std::string error;
    Rml::ElementDocument* doc = rmlhost::load(rml, nullptr, 0, 0, &error);
    CHECK(doc != nullptr);
    if (!doc) return;
    rmlhost::update();
    const std::vector<uikit::uicheck::Word> all = uikit::uicheck::words(doc, uikit::uicheck::Rgbf{0.5f, 0.5f, 0.5f});
    CHECK_EQ(all.size(), size_t(1));
    if (!all.empty()) {
        const std::string& t = all[0].text;
        CHECK(t.size() <= 60 && t.size() >= 58);
        CHECK((static_cast<unsigned char>(t.back()) & 0xC0) != 0xC0);  // no letter's first byte left alone
        CHECK(t.size() % 2 == 1);  // the Q and whole two-byte letters
    }
    rmlhost::close(doc);
}

// 3.0.24-beta.50 (the owner's «делай все», item 2; the audit 28.11: the light theme's icons white on white were not
// caught - the check read words only): the one-colour pictures too - their ink (inkOf: the mean of their solid
// pixels; false for a picture of many colours, an item's or a face) against the grounds under them, 3:1 (WCAG 1.4.11)
TEST(uikit_ui_check_finds_faint_icons) {
    const char* rml = R"RML(<rml><head><style>
body { width: 300px; height: 100px; background-color: #f6f8fc; font-family: Plex; font-size: 14px; }
img { display: block; width: 18px; height: 18px; }
</style></head><body><img src="pic:white"/><img src="pic:ink"/><img src="pic:item"/></body></rml>)RML";
    std::string error;
    Rml::ElementDocument* doc = rmlhost::load(rml, nullptr, 0, 0, &error);
    CHECK(doc != nullptr);
    if (!doc) return;
    rmlhost::update();
    const uikit::uicheck::InkOf inkOf = [](const std::string& src, unsigned& rgb) {
        if (src == "pic:white") return rgb = 0xECEFF5, true;
        if (src == "pic:ink") return rgb = 0x1B1F2A, true;
        return false;  // the item's picture: many colours, not judged
    };
    const std::vector<uikit::uicheck::Word> bad =
        uikit::uicheck::problems(doc, uikit::uicheck::Rgbf{0.45f, 0.45f, 0.45f}, inkOf);
    CHECK_EQ(bad.size(), size_t(1));
    CHECK(bad.size() == 1 && bad[0].text == "icon pic:white" && bad[0].contrast < 1.2f);
    CHECK(uikit::uicheck::problems(doc).empty());  // no inkOf: the words alone, as before
    rmlhost::close(doc);
}
