#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_7_10) { CHECK(std::strcmp(uagfx::kVersion, "2.7.10") == 0); }
