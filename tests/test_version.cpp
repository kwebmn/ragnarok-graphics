#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_22_3) { CHECK(std::strcmp(uagfx::kVersion, "0.22.3") == 0); }
