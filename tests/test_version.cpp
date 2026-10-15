#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_48_0) { CHECK(std::strcmp(uagfx::kVersion, "0.48.0") == 0); }
