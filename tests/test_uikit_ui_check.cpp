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
