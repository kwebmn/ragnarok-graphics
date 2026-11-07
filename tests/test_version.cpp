#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_15) { CHECK(std::strcmp(uagfx::kVersion, "3.0.15") == 0); }
