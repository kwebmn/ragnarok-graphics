#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_2_0) { CHECK(std::strcmp(uagfx::kVersion, "0.2.0") == 0); }
