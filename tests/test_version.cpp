#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_84_0) { CHECK(std::strcmp(uagfx::kVersion, "0.84.0") == 0); }
