#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_15_0) { CHECK(std::strcmp(uagfx::kVersion, "0.15.0") == 0); }
