#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_87_1) { CHECK(std::strcmp(uagfx::kVersion, "0.87.1") == 0); }
