#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_6_0) { CHECK(std::strcmp(uagfx::kVersion, "0.6.0") == 0); }
