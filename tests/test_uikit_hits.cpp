#include <cmath>
#include <cstdio>

#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/hits.hpp"

using namespace uagfx;

// 3.0.18-beta.94: the drop protocol - the element RmlUi laid out under a point, by its data-hit and data-index
TEST(uikit_hit_finds_the_cell_flexbox_put_under_the_point) {
    const char* rml = R"(<rml><head><style>
body { width: 200px; height: 120px; font-family: Plex; font-size: 14px; }
.row { display: flex; flex-wrap: wrap; width: 200px; }
.c { width: 40px; height: 40px; margin: 5px; }
.ic { width: 20px; height: 20px; }
.title { height: 20px; }
div { display: block; }
</style></head><body>
<div class="title" data-hit="title">Речі</div>
<div class="row"><div class="c" data-for="v : cells" data-hit="cell" data-attr-data-index="v"><div
  class="ic"></div></div></div>
</body></rml>)";
    static std::vector<int> cells{0, 1, 2, 3, 4, 5};
    std::string error;
    Rml::ElementDocument* d = rmlhost::load(rml, [&](Rml::DataModelConstructor& c) {
        c.RegisterArray<std::vector<int>>();
        c.Bind("cells", &cells);
    }, 100, 50, &error);
    CHECK(d != nullptr);
    if (!error.empty()) std::printf("%s", error.c_str());
    CHECK_EQ(error, std::string());
    // the cells four a row (50 px each): row 1 at y 20..70 of the document, row 2 at 70..120
    uikit::Hit h = uikit::hitAt(d, 7, 27);  // in the first cell's icon: the cell, walked up
    CHECK_EQ(h.part, std::string("cell"));
    CHECK_EQ(h.index, 0);
    CHECK(std::abs(h.box.x - 5) < 0.01f);
    CHECK(std::abs(h.box.y - 25) < 0.01f);
    h = uikit::hitAt(d, 160, 30);
    CHECK_EQ(h.index, 3);
    h = uikit::hitAt(d, 60, 80);  // the second row's second
    CHECK_EQ(h.part, std::string("cell"));
    CHECK_EQ(h.index, 5);
    h = uikit::hitAt(d, 47, 30);  // between two cells: the window's ground
    CHECK_EQ(h.part, std::string());
    CHECK(h.inside);
    h = uikit::hitAt(d, 10, 5);
    CHECK_EQ(h.part, std::string("title"));
    h = uikit::hitAt(d, 300, 300);  // off it
    CHECK(!h.inside);
    CHECK_EQ(uikit::placesOf(d, "cell").size(), size_t(6));
    rmlhost::close(d);
}

// 3.0.18-beta.112 (R-GXZU9X: «не крутится персонаж стрелками»): a passive document (pointer-events none) - its parts
// found by their boxes all the same
TEST(uikit_hit_finds_parts_of_a_passive_document) {
    const char* rml = R"(<rml><head><style>
body { width: 200px; height: 100px; font-family: Plex; font-size: 14px; pointer-events: none; }
div { display: block; }
.a { position: absolute; left: 10px; top: 10px; width: 30px; height: 30px; }
.b { position: absolute; left: 100px; top: 10px; width: 30px; height: 30px; }
</style></head><body><div class="a" data-hit="turnleft"></div><div class="b" data-hit="turnright"></div>
</body></rml>)";
    Rml::ElementDocument* d = rmlhost::load(rml, nullptr, 50, 50);
    CHECK(d != nullptr);
    CHECK_EQ(uikit::hitAt(d, 20, 20).part, std::string("turnleft"));
    CHECK_EQ(uikit::hitAt(d, 110, 20).part, std::string("turnright"));
    CHECK_EQ(uikit::hitAt(d, 70, 70).part, std::string());
    rmlhost::close(d);
}
