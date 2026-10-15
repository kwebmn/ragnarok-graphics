#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_40_0) { CHECK(std::strcmp(uagfx::kVersion, "0.40.0") == 0); }
