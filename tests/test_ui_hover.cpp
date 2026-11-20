// 3.0.22-beta.40 (the owner's interface over the MCP: the character block's tips through «Навички» and «Статус»): the
// windows are passive documents (their parts hit-test them; RmlUi does not see them) - no hover for what lies under one
#include "core/ui_hover.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(ui_hover_none_under_a_passive_window) {
    using uihover::DocAt;
    using uihover::Layer;
    const std::vector<DocAt> docs = {
        {1, 0, 553, 514, Layer::Hud, -1, false},       // the character block (live)
        {0, 0, 2040, 1147, Layer::Hud, -1, true},     // the experience's strip (passive, over all the screen)
        {329, 191, 1384, 707, Layer::Windows, 10, true},  // «Навички» (passive)
    };
    CHECK(uihover::passiveOver(docs, 476, 255));    // over the block and the window: the window's
    CHECK(!uihover::passiveOver(docs, 100, 100));   // the block alone (the strip's a panel's: not a window)
    CHECK(uihover::passiveOver(docs, 1000, 600));   // the window alone
    std::vector<DocAt> live = docs;
    live.push_back({400, 200, 300, 300, Layer::Windows, 11, false});  // a live window over it
    CHECK(!uihover::passiveOver(live, 476, 255));
    live.push_back({450, 240, 50, 50, Layer::Top, -1, true});  // a passive part on the top over that
    CHECK(uihover::passiveOver(live, 476, 255));
}
