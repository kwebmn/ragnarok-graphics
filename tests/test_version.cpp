#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_16_0) { CHECK(std::strcmp(uagfx::kVersion, "0.16.0") == 0); }
