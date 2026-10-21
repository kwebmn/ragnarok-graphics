#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_7_20) { CHECK(std::strcmp(uagfx::kVersion, "2.7.20") == 0); }
