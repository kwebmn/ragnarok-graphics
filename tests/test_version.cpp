#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_65_0) { CHECK(std::strcmp(uagfx::kVersion, "0.65.0") == 0); }
