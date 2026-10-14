#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_8_0) { CHECK(std::strcmp(uagfx::kVersion, "0.8.0") == 0); }
