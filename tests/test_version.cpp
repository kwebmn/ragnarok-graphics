#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_13_0) { CHECK(std::strcmp(uagfx::kVersion, "1.13.0") == 0); }
