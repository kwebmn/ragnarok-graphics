#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_3_0_0) { CHECK(std::strcmp(uagfx::kVersion, "3.0.0") == 0); }
