#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_50_0) { CHECK(std::strcmp(uagfx::kVersion, "0.50.0") == 0); }
