#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_13_0) { CHECK(std::strcmp(uagfx::kVersion, "0.13.0") == 0); }
