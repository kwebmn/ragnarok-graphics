#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_1_0) { CHECK(std::strcmp(uagfx::kVersion, "0.1.0") == 0); }
