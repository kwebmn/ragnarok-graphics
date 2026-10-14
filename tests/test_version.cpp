#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_10_1) { CHECK(std::strcmp(uagfx::kVersion, "0.10.1") == 0); }
