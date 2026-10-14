#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_33_0) { CHECK(std::strcmp(uagfx::kVersion, "0.33.0") == 0); }
