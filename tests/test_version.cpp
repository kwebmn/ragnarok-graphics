#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_10_3) { CHECK(std::strcmp(uagfx::kVersion, "0.10.3") == 0); }
