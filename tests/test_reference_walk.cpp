// 3.0.18-beta.246 (the reference's stage 8: the site builds the walkable cells with its data): the site's manifest of
// the maps' walkable parts - the quests' shape ({v, sha256, size, count, url}), its file named by its hash
#include <string>

#include "core/reference_walk.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(reference_walk_manifest_names_its_file) {
    refwalk::Manifest m;
    CHECK(refquests::parse(R"({"v":1,"sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",)"
                           R"("size":1267832,"count":697,"url":"https://uaro.kiev.ua/x"})",
                           m));
    CHECK_EQ(refwalk::fileName(m), std::string("walkable-0123456789ab.bin"));
    CHECK_EQ(refwalk::sitePath(m), std::string("/api/public/game/walkable-0123456789ab.bin"));
    CHECK(m.count == 697 && m.size == 1267832);
}
