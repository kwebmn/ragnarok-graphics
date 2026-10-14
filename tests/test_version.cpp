#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_21_0) { CHECK(std::strcmp(uagfx::kVersion, "0.21.0") == 0); }
