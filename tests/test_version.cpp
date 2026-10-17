#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_20_0) { CHECK(std::strcmp(uagfx::kVersion, "1.20.0") == 0); }
