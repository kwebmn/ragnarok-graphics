#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_10_0) { CHECK(std::strcmp(uagfx::kVersion, "1.10.0") == 0); }
