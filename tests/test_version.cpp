#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_75_0) { CHECK(std::strcmp(uagfx::kVersion, "0.75.0") == 0); }
