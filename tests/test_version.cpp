#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_17) { CHECK(std::strcmp(uagfx::kVersion, "3.0.17") == 0); }
