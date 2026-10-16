#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_90_1) { CHECK(std::strcmp(uagfx::kVersion, "0.90.1") == 0); }
