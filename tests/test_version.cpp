#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_16_1) { CHECK(std::strcmp(uagfx::kVersion, "1.16.1") == 0); }
