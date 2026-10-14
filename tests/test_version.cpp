#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_10_4) { CHECK(std::strcmp(uagfx::kVersion, "0.10.4") == 0); }
