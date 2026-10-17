#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_16_0) { CHECK(std::strcmp(uagfx::kVersion, "1.16.0") == 0); }
