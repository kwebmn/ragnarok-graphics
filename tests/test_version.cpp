#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_5) { CHECK(std::strcmp(uagfx::kVersion, "3.0.5") == 0); }
