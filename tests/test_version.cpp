#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_10_0) { CHECK(std::strcmp(uagfx::kVersion, "0.10.0") == 0); }
