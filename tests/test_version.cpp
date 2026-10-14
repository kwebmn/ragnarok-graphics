#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_13_1) { CHECK(std::strcmp(uagfx::kVersion, "0.13.1") == 0); }
