#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_6_4) { CHECK(std::strcmp(uagfx::kVersion, "1.6.4") == 0); }
