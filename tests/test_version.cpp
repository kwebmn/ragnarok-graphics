#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_60_0) { CHECK(std::strcmp(uagfx::kVersion, "0.60.0") == 0); }
