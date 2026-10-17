#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_21_0) { CHECK(std::strcmp(uagfx::kVersion, "1.21.0") == 0); }
