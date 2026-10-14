#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_22_1) { CHECK(std::strcmp(uagfx::kVersion, "0.22.1") == 0); }
