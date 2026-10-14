#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_14_1) { CHECK(std::strcmp(uagfx::kVersion, "0.14.1") == 0); }
