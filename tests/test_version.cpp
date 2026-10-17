#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_24_0) { CHECK(std::strcmp(uagfx::kVersion, "1.24.0") == 0); }
