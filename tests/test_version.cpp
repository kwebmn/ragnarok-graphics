#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_7_4) { CHECK(std::strcmp(uagfx::kVersion, "2.7.4") == 0); }
