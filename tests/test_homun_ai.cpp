// 3.0.11-beta.290 (the owner's feature 22): our own homunculus and mercenary AI
#include <cstring>
#include <string>

#include "core/homun_ai.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(homun_ai_files_and_choices) {
    CHECK(std::strncmp(homunai::aiLua(), homunai::kMarker, std::strlen(homunai::kMarker)) == 0);
    CHECK(std::strncmp(homunai::aiMLua(), homunai::kMarker, std::strlen(homunai::kMarker)) == 0);
    CHECK(std::string(homunai::aiLua()).find("function AI(myid)") != std::string::npos);
    CHECK(homunai::ours(""));
    CHECK(homunai::ours(homunai::aiLua()));
    CHECK(!homunai::ours("-- AzzyAI\nfunction AI(myid)"));
    homunai::Choices c;
    c.aggressive = true;
    c.follow = 5;
    const std::string cfg = homunai::config(c);
    CHECK(homunai::ours(cfg));
    CHECK(cfg.find("UARO_AGGRESSIVE = 1\n") != std::string::npos);
    CHECK(cfg.find("UARO_FOLLOW = 5\n") != std::string::npos);
    CHECK(cfg.find("UARO_DEFEND = 1\n") != std::string::npos);
}
