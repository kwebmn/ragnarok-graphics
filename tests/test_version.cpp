#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_7_1) { CHECK(std::strcmp(uagfx::kVersion, "0.7.1") == 0); }
