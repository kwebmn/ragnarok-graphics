#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_13_1) { CHECK(std::strcmp(uagfx::kVersion, "1.13.1") == 0); }
