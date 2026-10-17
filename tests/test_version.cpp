#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_10_1) { CHECK(std::strcmp(uagfx::kVersion, "1.10.1") == 0); }
