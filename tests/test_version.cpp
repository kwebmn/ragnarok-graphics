#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_15_0) { CHECK(std::strcmp(uagfx::kVersion, "1.15.0") == 0); }
