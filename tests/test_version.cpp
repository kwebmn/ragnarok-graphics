#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_2_0) { CHECK(std::strcmp(uagfx::kVersion, "1.2.0") == 0); }
