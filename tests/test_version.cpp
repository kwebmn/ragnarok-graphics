#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_6_0) { CHECK(std::strcmp(uagfx::kVersion, "1.6.0") == 0); }
