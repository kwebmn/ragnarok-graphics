#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_24_0) { CHECK(std::strcmp(uagfx::kVersion, "0.24.0") == 0); }
