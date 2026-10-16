#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_90_3) { CHECK(std::strcmp(uagfx::kVersion, "0.90.3") == 0); }
