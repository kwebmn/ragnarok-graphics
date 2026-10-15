#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_74_0) { CHECK(std::strcmp(uagfx::kVersion, "0.74.0") == 0); }
