#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_8_1) { CHECK(std::strcmp(uagfx::kVersion, "0.8.1") == 0); }
