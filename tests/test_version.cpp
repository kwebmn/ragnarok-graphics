#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_0_1) { CHECK(std::strcmp(uagfx::kVersion, "1.0.1") == 0); }
