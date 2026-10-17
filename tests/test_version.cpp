#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_0_1) { CHECK(std::strcmp(uagfx::kVersion, "2.0.1") == 0); }
