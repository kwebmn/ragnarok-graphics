#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_0_2) { CHECK(std::strcmp(uagfx::kVersion, "1.0.2") == 0); }
