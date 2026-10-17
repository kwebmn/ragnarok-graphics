#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_22_0) { CHECK(std::strcmp(uagfx::kVersion, "1.22.0") == 0); }
