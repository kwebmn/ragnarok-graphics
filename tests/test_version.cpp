#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_6_1) { CHECK(std::strcmp(uagfx::kVersion, "0.6.1") == 0); }
