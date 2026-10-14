#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_20_0) { CHECK(std::strcmp(uagfx::kVersion, "0.20.0") == 0); }
