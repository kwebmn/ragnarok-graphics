#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_42_0) { CHECK(std::strcmp(uagfx::kVersion, "0.42.0") == 0); }
