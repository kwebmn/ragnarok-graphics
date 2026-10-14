#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_31_0) { CHECK(std::strcmp(uagfx::kVersion, "0.31.0") == 0); }
