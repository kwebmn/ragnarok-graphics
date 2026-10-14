#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_32_0) { CHECK(std::strcmp(uagfx::kVersion, "0.32.0") == 0); }
